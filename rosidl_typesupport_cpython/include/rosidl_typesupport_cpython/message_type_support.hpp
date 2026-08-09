// Copyright 2026 Ekumen Inc.
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

#ifndef ROSIDL_TYPESUPPORT_CPYTHON__MESSAGE_TYPE_SUPPORT_HPP_
#define ROSIDL_TYPESUPPORT_CPYTHON__MESSAGE_TYPE_SUPPORT_HPP_

#include <pybind11/pybind11.h>

#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_typesupport_cpython/visibility_control.h"

namespace rosidl_typesupport_cpython
{

/// Get the dispatch type support handle for a Python message class.
/**
 * This is the client-library entry point (used by rclpy's CPython extension):
 * given a Python message class (or instance), resolve the dispatch handle for
 * the corresponding message type.
 *
 * Resolution:
 *   - package origin and fully-qualified message name are derived from
 *     `msg_class.__module__` / `msg_class.__name__`;
 *   - the generated per-package dispatch library `{pkg}__rosidl_typesupport_cpython`
 *     is dlopen'd and the per-message C symbol
 *     `rosidl_typesupport_cpython__get_message_type_support_handle__...` is
 *     resolved and called;
 *   - the returned handle carries `typesupport_identifier ==
 *     rosidl_typesupport_cpython::typesupport_identifier` and a
 *     `type_support_map_t` over the available CPython typesupport
 *     implementations; middleware resolves a concrete implementation from it.
 *
 * The handle is owned by the generated dispatch library (static storage) and
 * must not be freed.  Lookups are cached per (package, message) pair.
 *
 * \param msg_class A Python message class or instance (experimental Python
 *                  message classes supported; standard classes resolve too if
 *                  a CPython typesupport was generated for them).
 * \return The dispatch `rosidl_message_type_support_t *`, or nullptr if the
 *         message type could not be resolved (Python error state set).
 */
ROSIDL_TYPESUPPORT_CPYTHON_PUBLIC
const rosidl_message_type_support_t *
get_message_typesupport_handle(pybind11::object msg_class);

}  // namespace rosidl_typesupport_cpython

#endif  // ROSIDL_TYPESUPPORT_CPYTHON__MESSAGE_TYPE_SUPPORT_HPP_
