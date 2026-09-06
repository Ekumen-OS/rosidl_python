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

#include "rosidl_runtime_c/message_type_support_struct.h"

namespace rosidl_runtime_cpython
{

/// Per-message-type bridge between the Python message handle and the C++
/// typesupport (ADR: MessageTypeBridge).
///
/// Each generated experimental message handle class carries a static const
/// instance of this struct, exposed through
/// `MessageHandleInterface::type_bridge()`.  rclpy extracts it via
/// `py::cast<MessageHandleInterface *>(pymsg)->type_bridge()` and uses the
/// C++ typesupport directly — no CPython typesupport dispatch involved.
///
/// Ownership contract:
/// - `wrap_cpp_message` ALWAYS creates a non-owning Python handle (used on
///   borrow/take-loaned paths where the middleware owns the storage).
/// - `unwrap_cpp_message` ALWAYS returns a raw pointer — no ownership
///   transfer; the Python handle retains ownership (if any).
/// - `unwrap_cpp_constraints` returns the raw C++ Constraints pointer (no
///   ownership transfer); `wrap_cpp_constraints` copies the value into a
///   Python-owned object.
///
/// All functions set CPython error flags and return nullptr on failure.
struct MessageTypeBridge
{
  /// Fetch the C++ typesupport handle for this message type.
  /// Uses the generic `rosidl_typesupport_cpp` dispatch — never specialized
  /// to a single implementation; the middleware resolves the concrete one.
  const rosidl_message_type_support_t * (*get_cpp_typesupport)() = nullptr;

  /// Wrap a C++ message pointer in a Python handle (new reference).
  /// Always non-owning: the caller (middleware) owns the memory.
  PyObject * (*wrap_cpp_message)(void * msg) = nullptr;

  /// Unwrap a Python handle to its C++ message pointer.
  /// No ownership transfer: the Python handle retains ownership (if any).
  void * (*unwrap_cpp_message)(PyObject * py) = nullptr;

  /// Wrap a C++ Constraints pointer in a Python object (new reference).
  /// Copies the value; the Python object owns its copy.
  PyObject * (*wrap_cpp_constraints)(void * cs) = nullptr;

  /// Unwrap a Python Constraints object to its C++ Constraints pointer.
  /// No ownership transfer: the Python object retains ownership.
  void * (*unwrap_cpp_constraints)(PyObject * py) = nullptr;
};

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__BRIDGE_HPP_