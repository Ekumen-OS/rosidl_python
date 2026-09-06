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

/// Sequence wrapper registration (sequence.hpp). The final module
/// (primitives.cpp) calls register_sequences(); template instantiation is
/// implicit here.

#include "sequence.hpp"

namespace rosidl_runtime_cpython
{

// String-element sequences (no buffer protocol: strings are not
// buffer-compatible in the same way as primitives).  Registered so generated
// getters returning SequenceWrapper<String>/SequenceWrapper<WString> resolve.
template<typename T>
void register_string_sequence(py::module_ & m, const char * name)
{
  py::class_<SequenceIterator<T>>(m, (std::string(name) + "Iterator").c_str())
    .def("__iter__", &SequenceIterator<T>::iter)
    .def("__next__", &SequenceIterator<T>::next);

  py::class_<SequenceWrapper<T>, std::shared_ptr<SequenceWrapper<T>>>(
    m, name)
    .def(py::init([]() { return SequenceWrapper<T>::make(py::list()); }))
    .def(py::init([](py::handle h) { return SequenceWrapper<T>::make(h); }), py::arg("data"))
    .def_static("Make", &SequenceWrapper<T>::make, py::arg("values"))
    .def("as_builtin", &SequenceWrapper<T>::as_builtin)
    .def("from_builtin", &SequenceWrapper<T>::from_builtin, py::arg("value"))
    .def("max_size", &SequenceWrapper<T>::max_size)
    .def("is_bounded", &SequenceWrapper<T>::is_bounded)
    .def("__len__", &SequenceWrapper<T>::len)
    .def("__getitem__", &SequenceWrapper<T>::getitem)
    .def("__setitem__", &SequenceWrapper<T>::setitem)
    .def("__delitem__", &SequenceWrapper<T>::delitem)
    .def("__iter__", &SequenceWrapper<T>::iter)
    .def("__reversed__", &SequenceWrapper<T>::reversed)
    .def("__contains__", &SequenceWrapper<T>::contains)
    .def("__eq__", &SequenceWrapper<T>::eq)
    .def("__ne__", &SequenceWrapper<T>::ne)
    .def("__add__", &SequenceWrapper<T>::add)
    .def("__radd__", &SequenceWrapper<T>::radd)
    .def("__iadd__", &SequenceWrapper<T>::iadd)
    .def("__mul__", &SequenceWrapper<T>::mul)
    .def("__rmul__", &SequenceWrapper<T>::rmul)
    .def("__imul__", &SequenceWrapper<T>::imul)
    .def("index", &SequenceWrapper<T>::index, py::arg("value"))
    .def("count", &SequenceWrapper<T>::count, py::arg("value"))
    .def("reverse", &SequenceWrapper<T>::reverse)
    .def("copy", &SequenceWrapper<T>::copy)
    .def("append", &SequenceWrapper<T>::append, py::arg("value"))
    .def("extend", &SequenceWrapper<T>::extend, py::arg("values"))
    .def("insert", &SequenceWrapper<T>::insert, py::arg("index"), py::arg("value"))
    .def("pop", &SequenceWrapper<T>::pop, py::arg("index") = py::int_(-1))
    .def("remove", &SequenceWrapper<T>::remove, py::arg("value"))
    .def("clear", &SequenceWrapper<T>::clear)
    .def("resize", &SequenceWrapper<T>::resize, py::arg("new_size"))
    .def("assign", &SequenceWrapper<T>::assign, py::arg("values"))
    .def("__repr__", &SequenceWrapper<T>::repr);
}

void register_sequences(py::module_ & m)
{
  // Unbounded + bounded primitive sequences (ADR-004 amendment): one class
  // per element type.
  register_sequence<bool>(m, "BoolSequence");
  register_sequence<uint8_t>(m, "UInt8Sequence");
  register_sequence<uint16_t>(m, "UInt16Sequence");
  register_sequence<uint32_t>(m, "UInt32Sequence");
  register_sequence<uint64_t>(m, "UInt64Sequence");
  register_sequence<int8_t>(m, "Int8Sequence");
  register_sequence<int16_t>(m, "Int16Sequence");
  register_sequence<int32_t>(m, "Int32Sequence");
  register_sequence<int64_t>(m, "Int64Sequence");
  register_sequence<float>(m, "Float32Sequence");
  register_sequence<double>(m, "Float64Sequence");
  register_sequence<long double>(m, "LongDoubleSequence");

  // Aliases for the shared SequenceU8 class (ADR-004 aliasing acceptance).
  m.attr("CharSequence") = m.attr("UInt8Sequence");
  m.attr("ByteSequence") = m.attr("UInt8Sequence");
  m.attr("OctetSequence") = m.attr("UInt8Sequence");

  // String-element sequences (no buffer protocol).
  register_string_sequence<rosidl_runtime_cpp::String>(m, "StringSequence");
  register_string_sequence<rosidl_runtime_cpp::WString>(m, "WStringSequence");
}

}  // namespace rosidl_runtime_cpython