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

#include "primitives_common.hpp"

#include <string>

#include "message_handle.hpp"

namespace rosidl_runtime_cpython
{

// Single definition of the thread-local element-conversion counter. Declared
// in primitives_common.hpp but defined here (one TU) so every DSO — the
// _primitives extension and the generated message binding libraries — shares
// the same instance. Inline variables and function-local statics in inline
// functions both get GNU unique symbols (local binding), which would give
// each DSO its own copy and break the copy-count benchmark.
size_t & element_copies()
{
  static thread_local size_t c = 0;
  return c;
}

// ----------------------------------------------------------------------------
// Type-erased message-handle helpers (shared across DSOs)
// ----------------------------------------------------------------------------

void * unwrap_message_handle(void * py_obj)
{
  if (nullptr == py_obj) {
    return nullptr;
  }
  py::handle h = py::reinterpret_borrow<py::object>(
    reinterpret_cast<PyObject *>(py_obj));
  auto * base = py::cast<MessageHandleInterface *>(h);
  return base->get_void();
}

void detach_message_handle(void * py_obj)
{
  if (nullptr == py_obj) {
    return;
  }
  py::handle h = py::reinterpret_borrow<py::object>(
    reinterpret_cast<PyObject *>(py_obj));
  py::cast<MessageHandleInterface *>(h)->detach();
}

}  // namespace rosidl_runtime_cpython