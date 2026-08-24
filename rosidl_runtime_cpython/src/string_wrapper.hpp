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

/// Native unbounded String/WString wrappers over
/// rosidl_runtime_cpp::BasicString<CharT, 0> (ADR-002/004): byte/code-unit
/// MutableSequence, UTF-8 / UTF-16-LE, in-place mutation, buffer/NumPy views,
/// as_builtin/from_builtin.

#ifndef ROSIDL_RUNTIME_CPYTHON__SRC__STRING_WRAPPER_HPP_
#define ROSIDL_RUNTIME_CPYTHON__SRC__STRING_WRAPPER_HPP_

#include "primitives_common.hpp"
#include "scalar_wrapper.hpp"

namespace rosidl_runtime_cpython
{

// ============================================================================
// String / WString wrappers (unbounded BasicString<CharT, 0>)
// ============================================================================

// Convert a Python str/bytes to the raw CharT bytes for the target type.
// char: UTF-8 (str) or raw bytes; char16_t: UTF-16-LE (str) or raw bytes.
// Note: the returned std::string data is reinterpreted as CharT* below;
// std::string allocations are at least 8-byte aligned in practice, satisfying
// the char16_t (2-byte) alignment requirement.
template<typename CharT>
std::string to_char_bytes(py::handle h)
{
  if (py::isinstance<py::str>(h)) {
    if constexpr (std::is_same_v<CharT, char>) {
      return py::cast<std::string>(h);
    } else {
      return py::cast<std::string>(h.attr("encode")("utf-16-le"));
    }
  }
  if (py::isinstance<py::bytes>(h) || py::isinstance<py::bytearray>(h)) {
    std::string b = py::cast<std::string>(h);
    if constexpr (std::is_same_v<CharT, char16_t>) {
      if (b.size() % 2 != 0) {
        throw py::value_error("bytes for WString must have even length");
      }
    }
    return b;
  }
  throw py::type_error("assign requires str or bytes");
}

// View type for buffer/numpy: unsigned counterpart (byte/code-unit
// semantics, matching the legacy containers).
template<typename CharT> struct StringViewType;
template<> struct StringViewType<char> { using type = uint8_t; };
template<> struct StringViewType<char16_t> { using type = uint16_t; };

// Convert a code unit to a 1-character Python str (chr semantics): indexing
// and iterating a string yields strings, not integers.
template<typename CharT>
py::object code_unit_to_str(CharT c)
{
  if constexpr (std::is_same_v<CharT, char>) {
    // char is signed on Linux: cast through unsigned char so byte 200 -> 200,
    // not a huge unsigned value.
    return py::reinterpret_steal<py::object>(
      PyUnicode_FromOrdinal(static_cast<int>(static_cast<unsigned char>(c))));
  } else {
    return py::reinterpret_steal<py::object>(
      PyUnicode_FromOrdinal(static_cast<int>(c)));
  }
}

// C++ iterator over a StringWrapper's code units, yielding 1-char strs.
// Holds a strong reference to the parent so the wrapper (and its buffer)
// stays alive; mutating the string during iteration invalidates the iterator
// (standard C++ iterator semantics).
template<typename CharT>
class StringIterator
{
public:
  StringIterator(py::object parent, const CharT * data, size_t size, bool reverse)
  : parent_(std::move(parent)), data_(data), size_(size),
    pos_(reverse ? size_ : 0), reverse_(reverse)
  {
  }

  py::object next()
  {
    if (reverse_) {
      if (pos_ == 0) {
        throw py::stop_iteration();
      }
      return code_unit_to_str(data_[--pos_]);
    }
    if (pos_ >= size_) {
      throw py::stop_iteration();
    }
    return code_unit_to_str(data_[pos_++]);
  }

  // Python iterator protocol: iter(iterator) returns itself.
  py::object iter()
  {
    return py::cast(this, py::return_value_policy::reference);
  }

private:
  py::object parent_;  // keeps the wrapper alive
  const CharT * data_;
  size_t size_;
  size_t pos_;
  bool reverse_;
};

// Range-checked code-unit coercion for per-character mutation (ADR-003):
// char accepts [0, 255], char16_t accepts [0, 65535]. Accepts a 1-char str,
// a Char scalar (UInt8 for char), or an int code-unit value.
template<typename CharT>
CharT coerce_code_unit(py::handle h)
{
  // char is signed on Linux: use the unsigned counterpart for the bound
  // (255 for char, 65535 for char16_t).
  constexpr long long hi = static_cast<long long>(
    std::numeric_limits<std::make_unsigned_t<CharT>>::max());
  if (py::isinstance<py::str>(h)) {
    if (PyUnicode_GetLength(h.ptr()) == 1) {
      int cp = PyUnicode_ReadChar(h.ptr(), 0);
      if (cp >= 0 && cp <= hi) {
        return static_cast<CharT>(cp);
      }
    }
    raise_overflow("code unit value out of range");
  }
  if (py::isinstance<ScalarWrapper<uint8_t>>(h)) {
    if constexpr (std::is_same_v<CharT, char>) {
      auto & w = py::cast<ScalarWrapper<uint8_t> &>(h);
      return static_cast<CharT>(w.get());
    }
  }
  long long v = py::cast<long long>(h);
  if (v < 0 || v > hi) {
    raise_overflow("code unit value out of range");
  }
  return static_cast<CharT>(v);
}

template<typename CharT>
class StringWrapper
{
public:
  using StringT = rosidl_runtime_cpp::BasicString<CharT, 0>;
  using ViewT = typename StringViewType<CharT>::type;

  StringWrapper()
  : value_()
  {
  }

  explicit StringWrapper(py::handle h)
  : value_()
  {
    assign(h);
  }

  StringT & string()
  {
    return value_;
  }

  const StringT & string() const
  {
    return value_;
  }

  // ---- bulk operations ----------------------------------------------------

  void assign(py::handle h)
  {
    std::string bytes = to_char_bytes<CharT>(h);
    value_.assign(std::basic_string_view<CharT>(
      reinterpret_cast<const CharT *>(bytes.data()), bytes.size() / sizeof(CharT)));
  }

  void append_str(py::handle h)
  {
    std::string bytes = to_char_bytes<CharT>(h);
    value_.append(std::basic_string_view<CharT>(
      reinterpret_cast<const CharT *>(bytes.data()), bytes.size() / sizeof(CharT)));
  }

  // ---- value / builtin interop -------------------------------------------

  std::string str_() const
  {
    if constexpr (std::is_same_v<CharT, char>) {
      return std::string(value_.data(), value_.size());
    } else {
      std::string bytes(
        reinterpret_cast<const char *>(value_.data()), value_.size() * sizeof(CharT));
      return py::cast<std::string>(py::bytes(bytes).attr("decode")("utf-16-le"));
    }
  }

  py::object as_builtin() const
  {
    return py::str(str_());
  }

  void from_builtin(py::handle h)
  {
    assign(h);
  }

  py::object get_value() const
  {
    return as_builtin();
  }

  void set_value(py::handle h)
  {
    assign(h);
  }

  py::object dtype() const
  {
    static py::object dtype_enum = py::module_::import("rosidl_runtime_cpython.dtype").attr("Dtype");
    if constexpr (std::is_same_v<CharT, char>) {
      return dtype_enum.attr("CHAR");
    } else {
      return dtype_enum.attr("WCHAR");
    }
  }

  // ---- sequence protocol (byte/code-unit level) ---------------------------

  py::ssize_t len() const
  {
    return static_cast<py::ssize_t>(value_.size());
  }

  py::object getitem(py::handle index) const
  {
    if (py::isinstance<py::int_>(index)) {
      py::ssize_t i = py::cast<py::ssize_t>(index);
      if (i < 0) {
        i += static_cast<py::ssize_t>(value_.size());
      }
      if (i < 0 || i >= static_cast<py::ssize_t>(value_.size())) {
        throw py::index_error("string index out of range");
      }
      // Indexing yields a 1-char str, not an int.
      return code_unit_to_str(value_[static_cast<size_t>(i)]);
    }
    // Slice: return a wrapped String/WString copy (no list of ints).
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(value_.size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    StringWrapper result;
    if (step == 1) {
      result.value_.assign(value_.view().substr(
        static_cast<size_t>(start), static_cast<size_t>(slicelength)));
    } else {
      for (py::ssize_t k = 0; k < slicelength; ++k) {
        result.value_.push_back(value_[static_cast<size_t>(start + k * step)]);
      }
    }
    return py::cast(result);
  }

  void setitem(py::handle index, py::handle value)
  {
    if (py::isinstance<py::int_>(index)) {
      py::ssize_t i = py::cast<py::ssize_t>(index);
      if (i < 0) {
        i += static_cast<py::ssize_t>(value_.size());
      }
      if (i < 0 || i >= static_cast<py::ssize_t>(value_.size())) {
        throw py::index_error("string index out of range");
      }
      value_[static_cast<size_t>(i)] = coerce_code_unit<CharT>(value);
      return;
    }
    // Slice assignment: replace the range with the given values.
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(value_.size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    // Efficient path: String/WString (same CharT), str, or bytes/bytearray
    // sources are fetched as raw bytes (one copy) instead of materializing a
    // Python list of code units.
    std::optional<std::basic_string_view<CharT>> src_view;
    std::string owned_bytes;  // keeps encoded/copied bytes alive
    if (py::isinstance<StringWrapper<CharT>>(value)) {
      auto & o = py::cast<StringWrapper<CharT> &>(value);
      auto v = o.string().view();
      if (v.data() == value_.data()) {  // same object aliases self
        // Aliases self: copy to owned storage to stay safe on growth.
        owned_bytes.assign(
          reinterpret_cast<const char *>(v.data()), v.size() * sizeof(CharT));
        src_view = std::basic_string_view<CharT>(
          reinterpret_cast<const CharT *>(owned_bytes.data()), v.size());
      } else {
        src_view = v;
      }
    } else if (
      py::isinstance<py::str>(value) || py::isinstance<py::bytes>(value) ||
      py::isinstance<py::bytearray>(value))
    {
      owned_bytes = to_char_bytes<CharT>(value);
      src_view = std::basic_string_view<CharT>(
        reinterpret_cast<const CharT *>(owned_bytes.data()),
        owned_bytes.size() / sizeof(CharT));
    }
    if (src_view) {
      if (src_view->size() != static_cast<size_t>(slicelength)) {
        throw py::value_error("attempt to assign sequence of size "
          + std::to_string(src_view->size()) + " to extended slice of size "
          + std::to_string(slicelength));
      }
      if (step == 1) {
        value_.assign(*src_view, static_cast<size_t>(start));
      } else {
        for (py::ssize_t k = 0; k < slicelength; ++k) {
          value_[static_cast<size_t>(start + k * step)] = (*src_view)[static_cast<size_t>(k)];
        }
      }
      return;
    }
    // Fall back to list/tuple coercion.
    py::list values = py::cast<py::list>(value);
    if (py::len(values) != slicelength) {
      throw py::value_error("attempt to assign sequence of size "
        + std::to_string(py::len(values)) + " to extended slice of size "
        + std::to_string(slicelength));
    }
    if (step == 1) {
      // Contiguous overwrite at offset start (assign(view, pos)).
      std::basic_string<CharT> tmp;
      tmp.reserve(static_cast<size_t>(slicelength));
      for (py::ssize_t k = 0; k < slicelength; ++k) {
        tmp.push_back(coerce_code_unit<CharT>(values[k]));
      }
      value_.assign(tmp, static_cast<size_t>(start));
      return;
    }
    for (py::ssize_t k = 0; k < slicelength; ++k) {
      value_[static_cast<size_t>(start + k * step)] = coerce_code_unit<CharT>(values[k]);
    }
  }

  void delitem(py::handle index)
  {
    if (py::isinstance<py::int_>(index)) {
      py::ssize_t i = py::cast<py::ssize_t>(index);
      if (i < 0) {
        i += static_cast<py::ssize_t>(value_.size());
      }
      if (i < 0 || i >= static_cast<py::ssize_t>(value_.size())) {
        throw py::index_error("string index out of range");
      }
      // In-place shift left (no temporary string).
      for (size_t j = static_cast<size_t>(i); j + 1 < value_.size(); ++j) {
        value_[j] = value_[j + 1];
      }
      value_.resize(value_.size() - 1);
      return;
    }
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(value_.size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    if (step == 1) {
      // Contiguous range [start, stop): result = [0, start) + [stop, size).
      std::basic_string_view<CharT> cur(value_.data(), value_.size());
      if (start == 0) {
        // Prefix removal: assign the tail [stop, size) (single-view assign).
        value_.assign(cur.substr(static_cast<size_t>(stop)));
      } else {
        // In-place compaction: memmove the tail [stop, size) to [start, ...).
        std::memmove(value_.data() + start, value_.data() + stop,
          (value_.size() - static_cast<size_t>(stop)) * sizeof(CharT));
        value_.resize(value_.size() - static_cast<size_t>(slicelength));
      }
      return;
    }
    // Step != 1: boolean keep-mask, then a two-pointer in-place shift.
    std::vector<bool> keep(value_.size(), true);
    for (py::ssize_t k = 0; k < slicelength; ++k) {
      keep[static_cast<size_t>(start + k * step)] = false;
    }
    size_t write = 0;
    for (size_t read = 0; read < value_.size(); ++read) {
      if (keep[read]) {
        if (write != read) {
          value_[write] = value_[read];
        }
        ++write;
      }
    }
    value_.resize(write);
  }

  py::object iter() const
  {
    // C++ iterator yielding 1-char strs; no intermediary list.
    py::object self_obj = py::reinterpret_borrow<py::object>(
      py::cast(this, py::return_value_policy::reference));
    return py::cast(StringIterator<CharT>(
      self_obj, value_.data(), value_.size(), /* reverse */ false));
  }

  py::object contains(py::handle value) const
  {
    // Accept a 1-char str, a Char scalar (UInt8 for char), or an int
    // code-unit value; C++ linear scan (no per-element Python calls).
    std::optional<CharT> target;
    // char is signed on Linux: use the unsigned counterpart for the bound.
    constexpr long long hi = static_cast<long long>(
      std::numeric_limits<std::make_unsigned_t<CharT>>::max());
    if (py::isinstance<py::str>(value)) {
      if (PyUnicode_GetLength(value.ptr()) == 1) {
        int cp = PyUnicode_ReadChar(value.ptr(), 0);
        if (cp >= 0 && cp <= hi) {
          target = static_cast<CharT>(cp);
        }
      }
    } else if (py::isinstance<ScalarWrapper<uint8_t>>(value)) {
      if constexpr (std::is_same_v<CharT, char>) {
        auto & w = py::cast<ScalarWrapper<uint8_t> &>(value);
        target = static_cast<CharT>(w.get());
      }
    } else if (py::isinstance<py::int_>(value)) {
      long long v = py::cast<long long>(value);
      if (v >= 0 && v <= hi) {
        target = static_cast<CharT>(v);
      }
    }
    if (target) {
      for (size_t i = 0; i < value_.size(); ++i) {
        if (value_[i] == *target) {
          return py::bool_(true);
        }
      }
    }
    return py::bool_(false);
  }

  py::object reversed() const
  {
    py::object self_obj = py::reinterpret_borrow<py::object>(
      py::cast(this, py::return_value_policy::reference));
    return py::cast(StringIterator<CharT>(
      self_obj, value_.data(), value_.size(), /* reverse */ true));
  }

  void append(py::handle value)
  {
    value_.push_back(coerce_code_unit<CharT>(value));
  }

  void insert(py::handle index, py::handle value)
  {
    py::ssize_t i = py::cast<py::ssize_t>(index);
    if (i < 0) {
      i += static_cast<py::ssize_t>(value_.size());
    }
    if (i < 0) {
      i = 0;
    }
    if (i > static_cast<py::ssize_t>(value_.size())) {
      i = static_cast<py::ssize_t>(value_.size());
    }
    // In-place shift right (no temporary string).
    value_.resize(value_.size() + 1);
    for (size_t j = value_.size() - 1; j > static_cast<size_t>(i); --j) {
      value_[j] = value_[j - 1];
    }
    value_[static_cast<size_t>(i)] = coerce_code_unit<CharT>(value);
  }

  void clear()
  {
    value_.clear();
  }

  // ---- equality -----------------------------------------------------------

  py::object eq(py::handle other) const
  {
    if (py::isinstance<StringWrapper<CharT>>(other)) {
      auto & o = py::cast<StringWrapper<CharT> &>(other);
      return py::bool_(value_.view() == o.value_.view());
    }
    if (py::isinstance<py::str>(other)) {
      return py::bool_(str_() == py::cast<std::string>(other));
    }
    return py::bool_(false);
  }

  // ---- arithmetic (concatenation / repetition) ----------------------------

  py::object add(py::handle other) const
  {
    if (py::isinstance<StringWrapper<CharT>>(other)) {
      auto & o = py::cast<StringWrapper<CharT> &>(other);
      StringWrapper result;
      result.value_.assign(value_.view());
      result.value_.append(o.value_.view());
      return py::cast(result);
    }
    if (py::isinstance<py::str>(other)) {
      StringWrapper result;
      result.value_.assign(value_.view());
      std::string bytes = to_char_bytes<CharT>(other);
      result.value_.append(std::basic_string_view<CharT>(
        reinterpret_cast<const CharT *>(bytes.data()), bytes.size() / sizeof(CharT)));
      return py::cast(result);
    }
    return not_implemented();
  }

  py::object radd(py::handle other) const
  {
    if (py::isinstance<py::str>(other)) {
      StringWrapper result;
      std::string bytes = to_char_bytes<CharT>(other);
      result.value_.assign(std::basic_string_view<CharT>(
        reinterpret_cast<const CharT *>(bytes.data()), bytes.size() / sizeof(CharT)));
      result.value_.append(value_.view());
      return py::cast(result);
    }
    return not_implemented();
  }

  py::object mul(py::handle other) const
  {
    py::ssize_t n = py::cast<py::ssize_t>(other);
    if (n <= 0) {
      return py::cast(StringWrapper());
    }
    std::basic_string<CharT> tmp;
    tmp.reserve(value_.size() * static_cast<size_t>(n));
    for (py::ssize_t i = 0; i < n; ++i) {
      tmp.append(value_.data(), value_.size());
    }
    StringWrapper result;
    result.value_.assign(tmp);
    return py::cast(result);
  }

  py::object iadd(py::handle other)
  {
    if (py::isinstance<StringWrapper<CharT>>(other)) {
      auto & o = py::cast<StringWrapper<CharT> &>(other);
      value_.append(o.value_.view());
      return py::cast(this, py::return_value_policy::reference);
    }
    if (py::isinstance<py::str>(other)) {
      std::string bytes = to_char_bytes<CharT>(other);
      value_.append(std::basic_string_view<CharT>(
        reinterpret_cast<const CharT *>(bytes.data()), bytes.size() / sizeof(CharT)));
      return py::cast(this, py::return_value_policy::reference);
    }
    return not_implemented();
  }

  py::object imul(py::handle other)
  {
    py::ssize_t n = py::cast<py::ssize_t>(other);
    if (n <= 0) {
      value_.clear();
      return py::cast(this, py::return_value_policy::reference);
    }
    std::basic_string<CharT> tmp;
    tmp.reserve(value_.size() * static_cast<size_t>(n));
    for (py::ssize_t i = 0; i < n; ++i) {
      tmp.append(value_.data(), value_.size());
    }
    value_.assign(tmp);
    return py::cast(this, py::return_value_policy::reference);
  }

  // ---- buffer / numpy -----------------------------------------------------

  py::buffer_info buffer() const
  {
    return py::buffer_info(
      const_cast<ViewT *>(reinterpret_cast<const ViewT *>(value_.data())),
      sizeof(ViewT),
      py::format_descriptor<ViewT>::format(),
      1,
      {static_cast<py::ssize_t>(value_.size())},
      {static_cast<py::ssize_t>(sizeof(ViewT))});
  }

  py::array_t<ViewT> numpy()
  {
    py::object self_obj = py::reinterpret_borrow<py::object>(
      py::cast(this, py::return_value_policy::reference));
    py::array_t<ViewT> arr(
      {static_cast<py::ssize_t>(value_.size())},
      reinterpret_cast<const ViewT *>(value_.data()), self_obj);
    return arr;
  }

  std::string repr() const
  {
    return class_name() + "(" + py::cast<std::string>(py::repr(py::str(str_()))) + ")";
  }

private:
  static std::string class_name()
  {
    if constexpr (std::is_same_v<CharT, char>) {
      return "String";
    } else {
      return "WString";
    }
  }

  StringT value_;
};

template<typename CharT>
void register_string(py::module_ & m, const char * name)
{
  // Iterator class: yields 1-char strs; distinct name per CharT.
  py::class_<StringIterator<CharT>>(m,
    std::is_same_v<CharT, char> ? "StringIterator" : "WStringIterator")
    .def("__iter__", &StringIterator<CharT>::iter)
    .def("__next__", &StringIterator<CharT>::next);

  py::class_<StringWrapper<CharT>> cls(m, name, py::buffer_protocol());
  cls
    .def(py::init<>())
    .def(py::init([](py::handle h) { return new StringWrapper<CharT>(h); }), py::arg("data"))
    .def_property("value", &StringWrapper<CharT>::get_value, &StringWrapper<CharT>::set_value)
    .def_property_readonly("dtype", &StringWrapper<CharT>::dtype)
    .def("assign", &StringWrapper<CharT>::assign, py::arg("data"))
    .def("append_str", &StringWrapper<CharT>::append_str, py::arg("data"))
    .def("as_builtin", &StringWrapper<CharT>::as_builtin)
    .def("from_builtin", &StringWrapper<CharT>::from_builtin, py::arg("value"))
    .def("__str__", &StringWrapper<CharT>::str_)
    .def("__repr__", &StringWrapper<CharT>::repr)
    .def("__eq__", &StringWrapper<CharT>::eq)
    .def("__len__", &StringWrapper<CharT>::len)
    .def("__getitem__", &StringWrapper<CharT>::getitem)
    .def("__setitem__", &StringWrapper<CharT>::setitem)
    .def("__delitem__", &StringWrapper<CharT>::delitem)
    .def("__iter__", &StringWrapper<CharT>::iter)
    .def("__contains__", &StringWrapper<CharT>::contains)
    .def("__reversed__", &StringWrapper<CharT>::reversed)
    .def("append", &StringWrapper<CharT>::append, py::arg("value"))
    .def("insert", &StringWrapper<CharT>::insert, py::arg("index"), py::arg("value"))
    .def("clear", &StringWrapper<CharT>::clear)
    .def("__add__", &StringWrapper<CharT>::add)
    .def("__radd__", &StringWrapper<CharT>::radd)
    .def("__mul__", &StringWrapper<CharT>::mul)
    .def("__rmul__", &StringWrapper<CharT>::mul)
    .def("__iadd__", &StringWrapper<CharT>::iadd)
    .def("__imul__", &StringWrapper<CharT>::imul)
    .def("numpy", &StringWrapper<CharT>::numpy)
    .def_buffer(&StringWrapper<CharT>::buffer);
}


}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SRC__STRING_WRAPPER_HPP_
