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

#include "rosidl_typesupport_cpython/message_type_support.hpp"

#include <pybind11/pybind11.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "rcutils/error_handling.h"
#include "rcutils/snprintf.h"

#include "rcpputils/shared_library.hpp"

namespace py = pybind11;

namespace rosidl_typesupport_cpython
{

namespace
{

/// Canonical formatter shared by the generator and the dispatcher.
/**
 * Builds the fully-qualified C symbol name for a message:
 * `rosidl_typesupport_cpython__get_message_type_support_handle__<pkg>__<msg|msg_experimental>__<Name>`
 *
 * \param pkg          ROS package name (e.g. "std_msgs").
 * \param interface_type Message interface type: "msg" for standard messages,
 *                       "msg_experimental" for experimental messages.
 * \param message_name Message type name (e.g. "String").
 * \return The C symbol name.
 */
std::string
message_symbol_name(
  const std::string & pkg,
  const std::string & interface_type,
  const std::string & message_name)
{
  return "rosidl_typesupport_cpython__get_message_type_support_handle__" +
         pkg + "__" + interface_type + "__" + message_name;
}

/// Resolve the dispatch handle for a (pkg, interface_type, message_name) triple.
/**
 * dlopens `{pkg}__rosidl_typesupport_cpython` and calls the per-message C
 * symbol.  The returned handle is owned by static storage in that library.
 *
 * The SharedLibrary is cached for the process lifetime: dlclose at refcount 0
 * would unmap the library and dangle the returned handle (and its dispatch
 * function).  This mirrors the dispatch template's `map->data[i]` cache, which
 * keeps the impl library loaded for the same reason.
 */
const rosidl_message_type_support_t *
resolve_handle(
  const std::string & pkg,
  const std::string & interface_type,
  const std::string & message_name)
{
  std::string library_basename = pkg + "__rosidl_typesupport_cpython";
  std::string library_name;
  try {
    library_name = rcpputils::get_platform_library_name(library_basename);
  } catch (const std::runtime_error & e) {
    RCUTILS_SET_ERROR_MSG_WITH_FORMAT_STRING(
      "Failed to compute library name for '%s' due to %s",
      library_basename.c_str(), e.what());
    return nullptr;
  }

  // Keep the SharedLibrary alive for the lifetime of the process: the handle
  // returned by the symbol points into static storage inside this library.
  static std::unordered_map<std::string, std::unique_ptr<rcpputils::SharedLibrary>> libraries;
  static std::mutex libraries_mutex;
  std::lock_guard<std::mutex> lock(libraries_mutex);

  auto & lib = libraries[library_name];
  if (!lib) {
    try {
      lib = std::make_unique<rcpputils::SharedLibrary>(library_name);
    } catch (const std::runtime_error & e) {
      RCUTILS_SET_ERROR_MSG_WITH_FORMAT_STRING(
        "Could not load library %s: %s", library_name.c_str(), e.what());
      return nullptr;
    } catch (const std::bad_alloc & e) {
      RCUTILS_SET_ERROR_MSG_WITH_FORMAT_STRING(
        "Could not load library %s: %s", library_name.c_str(), e.what());
      return nullptr;
    }
  }

  std::string symbol_name = message_symbol_name(pkg, interface_type, message_name);
  void * sym = nullptr;
  try {
    if (!lib->has_symbol(symbol_name)) {
      RCUTILS_SET_ERROR_MSG_WITH_FORMAT_STRING(
        "Failed to find symbol '%s' in library %s", symbol_name.c_str(),
        library_name.c_str());
      return nullptr;
    }
    sym = lib->get_symbol(symbol_name);
  } catch (const std::exception & e) {
    RCUTILS_SET_ERROR_MSG_WITH_FORMAT_STRING(
      "Failed to get symbol '%s' in library %s: %s", symbol_name.c_str(),
      library_name.c_str(), e.what());
    return nullptr;
  }

  typedef const rosidl_message_type_support_t * (* funcSignature)(void);
  funcSignature func = reinterpret_cast<funcSignature>(sym);
  return func();
}

}  // namespace

const rosidl_message_type_support_t *
get_message_typesupport_handle(py::object msg_class)
{
  // Derive package origin + fully-qualified name from the message class.
  // Examples:
  //   std_msgs.msg.experimental._string -> pkg "std_msgs", experimental
  //   std_msgs.msg._string              -> pkg "std_msgs", standard
  py::object py_module = msg_class.attr("__module__");
  std::string module = py::cast<std::string>(py_module);
  std::string pkg = module.substr(0, module.find('.'));

  bool experimental = module.find(".experimental.") != std::string::npos ||
    module.rfind(".experimental", 0) == 0;
  std::string interface_type = experimental ? "msg_experimental" : "msg";

  py::object py_name = msg_class.attr("__name__");
  std::string message_name = py::cast<std::string>(py_name);

  // No handle cache: the handle is a static const at a fixed address in the
  // dispatch library (kept loaded by resolve_handle's library cache), so
  // re-resolving via the symbol is cheap and idempotent.
  return resolve_handle(pkg, interface_type, message_name);
}

}  // namespace rosidl_typesupport_cpython
