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

/// Array wrapper registration (array.hpp). The final module (primitives.cpp)
/// calls register_arrays(); template instantiation is implicit here.

#include "array.hpp"

namespace rosidl_runtime_cpython
{

// String-element arrays (no buffer protocol: strings are not buffer-compatible
// in the same way as primitives).  Registered so generated getters returning
// ArrayWrapper<String>/ArrayWrapper<WString> resolve.
template<typename T>
void register_string_array(py::module_ & m, const char * name)
{
  py::class_<ArrayIterator<T>>(m, (std::string(name) + "Iterator").c_str())
    .def("__iter__", &ArrayIterator<T>::iter)
    .def("__next__", &ArrayIterator<T>::next);

  py::class_<ArrayWrapper<T>, std::shared_ptr<ArrayWrapper<T>>>(
    m, name)
    .def("size", &ArrayWrapper<T>::size)
    .def("__len__", &ArrayWrapper<T>::len)
    .def("__getitem__", &ArrayWrapper<T>::getitem)
    .def("__setitem__", &ArrayWrapper<T>::setitem)
    .def("__iter__", &ArrayWrapper<T>::iter)
    .def("__reversed__", &ArrayWrapper<T>::reversed)
    .def("__contains__", &ArrayWrapper<T>::contains)
    .def("__eq__", &ArrayWrapper<T>::eq)
    .def("__ne__", &ArrayWrapper<T>::ne)
    .def("index", &ArrayWrapper<T>::index, py::arg("value"))
    .def("count", &ArrayWrapper<T>::count, py::arg("value"))
    .def("reverse", &ArrayWrapper<T>::reverse)
    .def("fill", &ArrayWrapper<T>::fill, py::arg("value"))
    .def("assign", &ArrayWrapper<T>::assign, py::arg("value"))
    .def("as_builtin", &ArrayWrapper<T>::as_builtin)
    .def("from_builtin", &ArrayWrapper<T>::from_builtin, py::arg("value"))
    .def("__repr__", &ArrayWrapper<T>::repr);
}

void register_arrays(py::module_ & m)
{
  // Fixed arrays (ADR-004 amendment): one class per element type.
  register_array<bool>(m, "BoolArray");
  register_array<uint8_t>(m, "UInt8Array");
  register_array<uint16_t>(m, "UInt16Array");
  register_array<uint32_t>(m, "UInt32Array");
  register_array<uint64_t>(m, "UInt64Array");
  register_array<int8_t>(m, "Int8Array");
  register_array<int16_t>(m, "Int16Array");
  register_array<int32_t>(m, "Int32Array");
  register_array<int64_t>(m, "Int64Array");
  register_array<float>(m, "Float32Array");
  register_array<double>(m, "Float64Array");
  register_array<long double>(m, "LongDoubleArray");

  // Aliases for the shared UInt8Array class (ADR-004 aliasing acceptance).
  m.attr("CharArray") = m.attr("UInt8Array");
  m.attr("ByteArray") = m.attr("UInt8Array");
  m.attr("OctetArray") = m.attr("UInt8Array");

  // String-element arrays (no buffer protocol).
  register_string_array<rosidl_runtime_cpp::String>(m, "StringArray");
  register_string_array<rosidl_runtime_cpp::WString>(m, "WStringArray");
}

}  // namespace rosidl_runtime_cpython