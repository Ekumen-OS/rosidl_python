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
#include "rosidl_runtime_cpython/bridge.hpp"

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

// Non-template base for message handles: enables type-erased unwrap/wrap by
// rclpy and the typesupport adapters (which cannot include the generated
// per-message binding headers — that would create a CMake build cycle through
// the rosidl_generator_py __py target).  Registered as a pybind11 base class
// in the runtime's _primitives module; generated handles derive from it.
class MessageHandleInterface
{
public:
  virtual ~MessageHandleInterface() = default;

  // The underlying C++ message pointer (type-erased).
  virtual void * get_void() = 0;

  // Detach the handle from its message without deleting it (used when the
  // C++ typesupport consumes the message via release/compact).
  virtual void detach() = 0;

  // The per-message-type bridge to the C++ typesupport (nullptr for handles
  // without one, e.g. standard messages or test fixtures).
  const MessageTypeBridge * type_bridge() const
  {
    return type_bridge_;
  }

protected:
  const MessageTypeBridge * type_bridge_ = nullptr;
};

// Shared ownership machinery for generated message handles. `msg_` owns the
// message when constructed from a unique_ptr (top-level); it is a non-owning
// reference when constructed from a raw pointer with a no-op deleter (nested
// view or loan). `parent_` anchors a nested view to the Python parent object.
template<typename Msg>
class MessageHandleBase : public MessageHandleInterface
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

  // Top-level non-owning view (loan): the middleware owns the storage.  The
  // handle must not delete the message; the middleware frees it when the loan
  // is returned.  No parent anchor: the MessageLoan object manages the
  // lifecycle.
  explicit MessageHandleBase(Msg * msg)
  : msg_(msg, MessageDeleter<Msg>{false}), parent_()
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

  // Detach the handle from its message without deleting it. Used by the
  // typesupport adapters when the C++ side consumes the message (release /
  // compact): the C++ typesupport deletes the message, so the handle must
  // release ownership to avoid a double-free. After detach, get() returns
  // nullptr and field access is undefined (Phase 5 adds explicit checks).
  void detach() override
  {
    msg_.release();
  }

  void * get_void() override
  {
    return msg_.get();
  }

protected:
  std::unique_ptr<Msg, MessageDeleter<Msg>> msg_;
  py::object parent_;  // anchor (empty for top-level handles)
};

}  // namespace rosidl_runtime_cpython

// Type-erased message-handle helpers for the typesupport adapters.  Defined
// in core.cpp (single TU of the rosidl_runtime_cpython_core shared library)
// so every DSO shares one instance (inline variables / function-local statics
// in inline functions get GNU unique symbols per DSO).
namespace rosidl_runtime_cpython
{

/// Unwrap a Python message handle (PyObject *) to its C++ message pointer.
/// Must be called with the GIL held.  Returns nullptr on null input.
void * unwrap_message_handle(void * py_obj);

/// Detach a Python message handle (release ownership without deleting).
/// Must be called with the GIL held.  No-op on null input.
void detach_message_handle(void * py_obj);

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SRC__MESSAGE_HANDLE_HPP_