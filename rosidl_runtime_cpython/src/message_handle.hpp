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

/// MessageHandleBase<Msg>: the shared ownership machinery for generated
/// message handles (ADR-008, ADR-011).
///
/// A generated message handle owns the C++ message via
/// `std::unique_ptr<Msg, MessageDeleter<Msg>>`. The deleter carries an
/// `owns` flag: top-level handles own the message (deleter deletes it);
/// nested message views are non-owning (deleter is a no-op) and are anchored
/// to the parent Python object via `parent_`, which keeps the parent — and
/// therefore the C++ storage — alive while the view exists.
///
/// Generated per-message classes derive from MessageHandleBase<Msg> and add
/// field getters that return non-owning wrappers anchored to the Python
/// `self` (the same pattern as the primitive wrappers).

#ifndef ROSIDL_RUNTIME_CPYTHON__SRC__MESSAGE_HANDLE_HPP_
#define ROSIDL_RUNTIME_CPYTHON__SRC__MESSAGE_HANDLE_HPP_

#include <memory>
#include <string>
#include <utility>

#include <pybind11/pybind11.h>

#include "rosidl_runtime_cpp/message_initialization.hpp"

namespace py = pybind11;

namespace rosidl_runtime_cpython
{

// Convert a Python MessageInitialization (enum or its string value) to the
// C++ enum. Generated message handles use this in their _reset()/create().
inline rosidl_runtime_cpp::MessageInitialization to_init(py::handle h)
{
  std::string s;
  if (py::isinstance<py::str>(h)) {
    s = py::cast<std::string>(h);
  } else {
    s = py::cast<std::string>(h.attr("value"));
  }
  if (s == "ALL") {
    return rosidl_runtime_cpp::MessageInitialization::ALL;
  }
  if (s == "SKIP") {
    return rosidl_runtime_cpp::MessageInitialization::SKIP;
  }
  if (s == "ZERO") {
    return rosidl_runtime_cpp::MessageInitialization::ZERO;
  }
  if (s == "DEFAULTS_ONLY") {
    return rosidl_runtime_cpp::MessageInitialization::DEFAULTS_ONLY;
  }
  throw py::value_error("unknown MessageInitialization: " + s);
}

// Custom deleter for the message unique_ptr. `owns == false` marks a
// non-owning nested view: the pointer references a field inside a parent
// message and must not be deleted.
template<typename Msg>
struct MessageDeleter
{
  bool owns = true;

  void operator()(Msg * m) const
  {
    if (owns) {
      delete m;
    }
  }
};

// Shared ownership machinery for generated message handles. `msg_` owns the
// message when constructed from a unique_ptr (top-level); it is a non-owning
// reference when constructed from a raw pointer with a no-op deleter (nested
// view). `parent_` anchors a nested view to the Python parent object.
template<typename Msg>
class MessageHandleBase
{
public:
  // Top-level: owns the message.
  explicit MessageHandleBase(std::unique_ptr<Msg> msg)
  : msg_(msg.release(), MessageDeleter<Msg>{}), parent_()
  {
  }

  // Nested view: non-owning reference into a parent message's storage,
  // anchored to the Python parent object.
  MessageHandleBase(Msg * msg, py::object parent)
  : msg_(msg, MessageDeleter<Msg>{false}), parent_(std::move(parent))
  {
  }

  // The underlying C++ message (null only if constructed from an empty
  // unique_ptr, which generated code never does).
  Msg * get() const
  {
    return msg_.get();
  }

  // True if this handle owns the message (top-level), false for nested views.
  bool owns() const
  {
    return msg_.get_deleter().owns;
  }

protected:
  std::unique_ptr<Msg, MessageDeleter<Msg>> msg_;
  py::object parent_;  // anchor (empty for top-level handles)
};

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SRC__MESSAGE_HANDLE_HPP_