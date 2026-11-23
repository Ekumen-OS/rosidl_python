// Copyright 2026 Ekumen, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ROSIDL_RUNTIME_CPYTHON__BRIDGE_HPP_
#define ROSIDL_RUNTIME_CPYTHON__BRIDGE_HPP_

#include <Python.h>

#include <memory>

#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_runtime_cpython/borrowed_view.hpp"

namespace rosidl_runtime_cpython
{

/// Per-message operations shared by generated Python views and rclpy.
struct MessageTypeBridge
{
  const rosidl_message_type_support_t * (*type_support)();

  /// Return a new Python reference to a non-owning, checked view.
  PyObject * (*wrap_loan)(
    void * message, std::shared_ptr<BorrowedViewState> state, bool read_only);

  /// Return the typed constraints contained in a Python object, without transferring ownership.
  const void * (*unwrap_constraints)(PyObject * constraints);

  const rosidl_message_type_support_t * (*plain_type_support)();
  std::shared_ptr<void>(*create_view)();
  std::shared_ptr<void>(*create_plain)();
  std::shared_ptr<void>(*view_to_plain)(const void * view);
  void (*view_assign_plain)(void * view, const void * plain);
  std::shared_ptr<void>(*copy_constraints)(const void * constraints);
  bool (*constraints_compatible)(const void * candidate, const void * baseline);
  bool (*view_compatible)(const void * constraints, const void * view);
};

template<typename View>
MessageTypeBridge make_message_type_bridge(
  const rosidl_message_type_support_t * (*type_support)(),
  PyObject * (*wrap_loan)(void *, std::shared_ptr<BorrowedViewState>, bool),
  const void * (*unwrap_constraints)(PyObject *),
  const rosidl_message_type_support_t * (*plain_type_support)())
{
  using Plain = typename View::PlainMessage;
  using Constraints = typename View::Constraints;
  return {
    type_support, wrap_loan, unwrap_constraints, plain_type_support,
    []() -> std::shared_ptr<void> {return std::make_shared<View>();},
    []() -> std::shared_ptr<void> {return std::make_shared<Plain>();},
    [](const void * value) -> std::shared_ptr<void> {
      return std::make_shared<Plain>(static_cast<const View *>(value)->to_plain());
    },
    [](void * value, const void * plain) {
      static_cast<View *>(value)->assign(*static_cast<const Plain *>(plain));
    },
    [](const void * value) -> std::shared_ptr<void> {
      return value ? std::make_shared<Constraints>(*static_cast<const Constraints *>(value)) :
             std::make_shared<Constraints>();
    },
    [](const void * candidate, const void * baseline) {
      return static_cast<const Constraints *>(candidate)->CheckCompatible(
        *static_cast<const Constraints *>(baseline));
    },
    [](const void * constraints, const void * value) {
      return static_cast<const Constraints *>(constraints)->CheckCompatibleView(
        *static_cast<const View *>(value));
    }
  };
}

inline constexpr char kMessageTypeBridgeCapsule[] = "rosidl_runtime_cpython.MessageTypeBridge.v2";

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__BRIDGE_HPP_
