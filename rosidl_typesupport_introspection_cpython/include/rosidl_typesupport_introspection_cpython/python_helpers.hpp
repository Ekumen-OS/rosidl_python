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

#ifndef ROSIDL_TYPESUPPORT_INTROSPECTION_CPYTHON__PYTHON_HELPERS_HPP_
#define ROSIDL_TYPESUPPORT_INTROSPECTION_CPYTHON__PYTHON_HELPERS_HPP_

/// @file python_helpers.hpp
/// @brief GIL-guarded pybind11 helpers shared by the generated introspection
///        type support code.
///
/// The generated per-message introspection descriptors describe Python
/// experimental message classes (containers from rosidl_runtime_cpython).
/// The `MessageMember` function pointers must operate on the Python
/// containers, so `void *` is treated as a type-erased `PyObject *`:
///
///  - the "member" / container argument is a `PyObject *` (Sequence or Array
///    instance),
///  - the element pointers returned by the get functions are `PyObject *`
///    (new references, owned by the caller),
///  - the fetch / assign "value" argument is a pointer to a `PyObject *` slot.
///
/// Every helper acquires the GIL on entry: the function pointers are called
/// from arbitrary middleware / DDS threads which do not hold it.  Python
/// exceptions are caught and cleared so they never cross the C ABI; failures
/// are reported through the return value (0 / nullptr / false).
///
/// The helpers are intentionally type-generic (Python sequence protocol) so a
/// single implementation serves every member: primitive and object-mode
/// sequences, arrays, strings and nested messages alike.

#include <Python.h>

#include <pybind11/pybind11.h>

#include <cstddef>

namespace rosidl_typesupport_introspection_cpython
{

namespace detail
{

/// Borrowed reference to a Python object from a type-erased `void *`.
inline pybind11::object as_py_object(const void * untyped)
{
  return pybind11::reinterpret_borrow<pybind11::object>(
    reinterpret_cast<PyObject *>(const_cast<void *>(untyped)));
}

/// size_function: number of elements in a Python container (Sequence/Array).
inline size_t cpython_size_function(const void * untyped_container)
{
  pybind11::gil_scoped_acquire gil;
  try {
    pybind11::object container = as_py_object(untyped_container);
    return static_cast<size_t>(pybind11::len(container));
  } catch (const pybind11::error_already_set &) {
    PyErr_Clear();
    return 0;
  }
}

/// get_function: element at *index* as a new-reference `PyObject *` (cast to
/// `void *`).  The caller owns the returned reference.
inline void * cpython_get_element(const void * untyped_container, size_t index)
{
  pybind11::gil_scoped_acquire gil;
  PyObject * element = PySequence_GetItem(
    as_py_object(untyped_container).ptr(), static_cast<Py_ssize_t>(index));
  if (nullptr == element) {
    PyErr_Clear();
    return nullptr;
  }
  return element;
}

/// Non-const forwarding overload matching the `MessageMember` get_function
/// signature (`void * (*)(void *, size_t)`).
inline void * cpython_get_element(void * untyped_container, size_t index)
{
  return cpython_get_element(static_cast<const void *>(untyped_container), index);
}

/// get_const_function: same as cpython_get_element, cast to `const void *` to
/// match the `MessageMember` get_const_function signature.
inline const void * cpython_get_const_element(
  const void * untyped_container, size_t index)
{
  return cpython_get_element(untyped_container, index);
}

/// fetch_function: store a new-reference `PyObject *` to the element at
/// *index* into the `PyObject *` slot pointed to by *untyped_value*.
inline void cpython_fetch_function(
  const void * untyped_container, size_t index, void * untyped_value)
{
  pybind11::gil_scoped_acquire gil;
  PyObject * element = PySequence_GetItem(
    as_py_object(untyped_container).ptr(), static_cast<Py_ssize_t>(index));
  if (nullptr == element) {
    PyErr_Clear();
    *reinterpret_cast<PyObject **>(untyped_value) = nullptr;
    return;
  }
  *reinterpret_cast<PyObject **>(untyped_value) = element;
}

/// assign_function: copy the `PyObject *` from the slot pointed to by
/// *untyped_value* into the container at *index* (the container takes its own
/// reference; the caller keeps ownership of the slot).
inline void cpython_assign_function(
  void * untyped_container, size_t index, const void * untyped_value)
{
  pybind11::gil_scoped_acquire gil;
  try {
    PyObject * value = *reinterpret_cast<PyObject * const *>(untyped_value);
    if (PySequence_SetItem(
        as_py_object(untyped_container).ptr(),
        static_cast<Py_ssize_t>(index), value) != 0)
    {
      throw pybind11::error_already_set();
    }
  } catch (const pybind11::error_already_set &) {
    PyErr_Clear();
  }
}

/// resize_function: call `container.resize(size)`.  Returns true on success.
inline bool cpython_resize_function(void * untyped_container, size_t size)
{
  pybind11::gil_scoped_acquire gil;
  try {
    pybind11::object container = as_py_object(untyped_container);
    container.attr("resize")(size);
    return true;
  } catch (const pybind11::error_already_set &) {
    PyErr_Clear();
    return false;
  }
}

}  // namespace detail

}  // namespace rosidl_typesupport_introspection_cpython

#endif  // ROSIDL_TYPESUPPORT_INTROSPECTION_CPYTHON__PYTHON_HELPERS_HPP_
