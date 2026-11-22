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
};

inline constexpr char kMessageTypeBridgeCapsule[] = "rosidl_runtime_cpython.MessageTypeBridge";

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__BRIDGE_HPP_
