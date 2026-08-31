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

/// Native String/WString wrappers over rosidl_runtime_cpp::BasicString<CharT, N>
/// (ADR-002/004/011): non-owning reference views through the type-erased
/// StringInterface / StringReference<CharT, UpperBound> (unbounded + bounded),
/// byte/code-unit MutableSequence, UTF-8 / UTF-16-LE, in-place mutation,
/// buffer/NumPy views, as_builtin/from_builtin, and test-only .Make() /
/// BoundedString.Make(bound, value) factories (aliasing shared_ptr fixtures).

#ifndef ROSIDL_RUNTIME_CPYTHON__SRC__STRING_HPP_
#define ROSIDL_RUNTIME_CPYTHON__SRC__STRING_HPP_

#include "primitives_common.hpp"
#include "scalar.hpp"

#include "rosidl_runtime_cpp/experimental/string.hpp"

namespace rosidl_runtime_cpython
{

// ============================================================================
// String interface / reference (type-erased, hides the bound)
// ============================================================================

template<typename CharT>
class StringInterface
{
public:
  virtual ~StringInterface() = default;

  virtual size_t size() const = 0;
  virtual const CharT * data() const = 0;
  virtual CharT * data() = 0;
  virtual std::basic_string_view<CharT> view() const = 0;
  virtual void assign(std::basic_string_view<CharT> value) = 0;
  virtual void assign(std::basic_string_view<CharT> value, size_t pos) = 0;
  virtual void append(std::basic_string_view<CharT> suffix) = 0;
  virtual void push_back(CharT c) = 0;
  virtual void resize(size_t n) = 0;
  virtual void clear() = 0;
  virtual size_t max_size() const = 0;
  virtual bool is_bounded() const = 0;
};

template<typename CharT, size_t UpperBound>
class StringReference final : public StringInterface<CharT>
{
public:
  explicit StringReference(rosidl_runtime_cpp::BasicString<CharT, UpperBound> * value)
  : value_(value)
  {
  }

  size_t size() const override
  {
    return value_->size();
  }

  const CharT * data() const override
  {
    return value_->data();
  }

  CharT * data() override
  {
    return value_->data();
  }

  std::basic_string_view<CharT> view() const override
  {
    return value_->view();
  }

  void assign(std::basic_string_view<CharT> value) override
  {
    value_->assign(value);
  }

  void assign(std::basic_string_view<CharT> value, size_t pos) override
  {
    value_->assign(value, pos);
  }

  void append(std::basic_string_view<CharT> suffix) override
  {
    value_->append(suffix);
  }

  void push_back(CharT c) override
  {
    value_->push_back(c);
  }

  void resize(size_t n) override
  {
    value_->resize(n);
  }

  void clear() override
  {
    value_->clear();
  }

  size_t max_size() const override
  {
    // Unbounded: max code units (matches the container's element-count
    // semantics: SIZE_MAX / sizeof(CharT)).
    return UpperBound == 0
      ? std::numeric_limits<size_t>::max() / sizeof(CharT)
      : UpperBound;
  }

  bool is_bounded() const override
  {
    return UpperBound != 0;
  }

private:
  rosidl_runtime_cpp::BasicString<CharT, UpperBound> * value_;
};

// ============================================================================
// String helpers
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

// ============================================================================
// String wrapper
// ============================================================================

// Forward declaration (defined below).
template<typename CharT>
class StringWrapper;

// Fixture data owner for standalone strings (test-only .Make() factories,
// ADR-011): the owner holds the BasicString; the wrapper is a non-owning view.
template<typename CharT, size_t UpperBound>
struct StringData
{
  rosidl_runtime_cpp::BasicString<CharT, UpperBound> data;
  StringWrapper<CharT> wrapper;
  explicit StringData(std::basic_string_view<CharT> value)
  : data(value), wrapper(std::unique_ptr<StringInterface<CharT>>(
      new StringReference<CharT, UpperBound>(&data)))
  {
  }
};

template<typename CharT>
class StringWrapper
{
public:
  using ViewT = typename StringViewType<CharT>::type;

  // Non-owning: references C++ string storage through a type-erased
  // interface (ADR-011). `parent` is the Python parent object (message
  // handle) that keeps the storage alive; empty for standalone fixtures.
  explicit StringWrapper(std::unique_ptr<StringInterface<CharT>> impl, py::object parent = {})
  : impl_(std::move(impl)), parent_(std::move(parent))
  {
  }

  // Test-only factories (ADR-011): unbounded (bound 0) and bounded (1..9).
  static std::shared_ptr<StringWrapper<CharT>> make(const std::string & value)
  {
    std::basic_string_view<CharT> view(
      reinterpret_cast<const CharT *>(value.data()), value.size() / sizeof(CharT));
    auto owner = std::make_shared<StringData<CharT, 0>>(view);
    return std::shared_ptr<StringWrapper<CharT>>(owner, &owner->wrapper);
  }

  static std::shared_ptr<StringWrapper<CharT>> make_bounded(
    size_t bound, const std::string & value)
  {
    std::basic_string_view<CharT> view(
      reinterpret_cast<const CharT *>(value.data()), value.size() / sizeof(CharT));
    // bound 1..9 -> index 0..8; bound 0 underflows to SIZE_MAX and bound >= 10
    // is out of range, so both fall through to the invalid_argument below.
    auto result = compile_time_switch<9>(bound - 1, [&](auto ic) {
      constexpr size_t B = decltype(ic)::value + 1;
      auto owner = std::make_shared<StringData<CharT, B>>(view);
      return std::shared_ptr<StringWrapper<CharT>>(owner, &owner->wrapper);
    });
    if (!result) {
      throw std::invalid_argument("BoundedString.Make supports bound 1..9");
    }
    return *result;
  }

  // ---- bulk operations ----------------------------------------------------

  void assign(py::handle h)
  {
    std::string bytes = to_char_bytes<CharT>(h);
    impl_->assign(std::basic_string_view<CharT>(
      reinterpret_cast<const CharT *>(bytes.data()), bytes.size() / sizeof(CharT)));
  }

  void append_str(py::handle h)
  {
    std::string bytes = to_char_bytes<CharT>(h);
    impl_->append(std::basic_string_view<CharT>(
      reinterpret_cast<const CharT *>(bytes.data()), bytes.size() / sizeof(CharT)));
  }

  // ---- value / builtin interop -------------------------------------------

  std::string str_() const
  {
    if constexpr (std::is_same_v<CharT, char>) {
      return std::string(impl_->data(), impl_->size());
    } else {
      std::string bytes(
        reinterpret_cast<const char *>(impl_->data()), impl_->size() * sizeof(CharT));
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

  // ---- sequence protocol (byte/code-unit level) ---------------------------

  py::ssize_t len() const
  {
    return static_cast<py::ssize_t>(impl_->size());
  }

  py::object getitem(py::handle index) const
  {
    if (py::isinstance<py::int_>(index)) {
      py::ssize_t i = py::cast<py::ssize_t>(index);
      if (i < 0) {
        i += static_cast<py::ssize_t>(impl_->size());
      }
      if (i < 0 || i >= static_cast<py::ssize_t>(impl_->size())) {
        throw py::index_error("string index out of range");
      }
      // Indexing yields a 1-char str, not an int.
      return code_unit_to_str(impl_->data()[static_cast<size_t>(i)]);
    }
    // Slice: return a str (builtin-return policy, ADR-002 amendment).
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(impl_->size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    std::basic_string<CharT> tmp;
    tmp.reserve(static_cast<size_t>(slicelength));
    if (step == 1) {
      tmp.assign(impl_->data() + start, static_cast<size_t>(slicelength));
    } else {
      for (py::ssize_t k = 0; k < slicelength; ++k) {
        tmp.push_back(impl_->data()[static_cast<size_t>(start + k * step)]);
      }
    }
    if constexpr (std::is_same_v<CharT, char>) {
      return py::str(std::string(tmp.data(), tmp.size()));
    } else {
      std::string bytes(
        reinterpret_cast<const char *>(tmp.data()), tmp.size() * sizeof(CharT));
      return py::bytes(bytes).attr("decode")("utf-16-le");
    }
  }

  void setitem(py::handle index, py::handle value)
  {
    if (py::isinstance<py::int_>(index)) {
      py::ssize_t i = py::cast<py::ssize_t>(index);
      if (i < 0) {
        i += static_cast<py::ssize_t>(impl_->size());
      }
      if (i < 0 || i >= static_cast<py::ssize_t>(impl_->size())) {
        throw py::index_error("string index out of range");
      }
      impl_->data()[static_cast<size_t>(i)] = coerce_code_unit<CharT>(value);
      return;
    }
    // Slice assignment: replace the range with the given values.
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(impl_->size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    // Efficient path: String/WString (same CharT), str, or bytes/bytearray
    // sources are fetched as raw bytes (one copy) instead of materializing a
    // Python list of code units.
    std::optional<std::basic_string_view<CharT>> src_view;
    std::string owned_bytes;  // keeps encoded/copied bytes alive
    if (py::isinstance<StringWrapper<CharT>>(value)) {
      auto & o = py::cast<StringWrapper<CharT> &>(value);
      auto v = o.impl_->view();
      if (v.data() == impl_->data()) {  // same object aliases self
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
        impl_->assign(*src_view, static_cast<size_t>(start));
      } else {
        for (py::ssize_t k = 0; k < slicelength; ++k) {
          impl_->data()[static_cast<size_t>(start + k * step)] = (*src_view)[static_cast<size_t>(k)];
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
      impl_->assign(tmp, static_cast<size_t>(start));
      return;
    }
    for (py::ssize_t k = 0; k < slicelength; ++k) {
      impl_->data()[static_cast<size_t>(start + k * step)] = coerce_code_unit<CharT>(values[k]);
    }
  }

  void delitem(py::handle index)
  {
    if (py::isinstance<py::int_>(index)) {
      py::ssize_t i = py::cast<py::ssize_t>(index);
      if (i < 0) {
        i += static_cast<py::ssize_t>(impl_->size());
      }
      if (i < 0 || i >= static_cast<py::ssize_t>(impl_->size())) {
        throw py::index_error("string index out of range");
      }
      // In-place shift left (no temporary string).
      for (size_t j = static_cast<size_t>(i); j + 1 < impl_->size(); ++j) {
        impl_->data()[j] = impl_->data()[j + 1];
      }
      impl_->resize(impl_->size() - 1);
      return;
    }
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(impl_->size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    if (step == 1) {
      // Contiguous range [start, stop): result = [0, start) + [stop, size).
      std::basic_string_view<CharT> cur(impl_->data(), impl_->size());
      if (start == 0) {
        // Prefix removal: assign the tail [stop, size) (single-view assign).
        impl_->assign(cur.substr(static_cast<size_t>(stop)));
      } else {
        // In-place compaction: memmove the tail [stop, size) to [start, ...).
        std::memmove(impl_->data() + start, impl_->data() + stop,
          (impl_->size() - static_cast<size_t>(stop)) * sizeof(CharT));
        impl_->resize(impl_->size() - static_cast<size_t>(slicelength));
      }
      return;
    }
    // Step != 1: boolean keep-mask, then a two-pointer in-place shift.
    std::vector<bool> keep(impl_->size(), true);
    for (py::ssize_t k = 0; k < slicelength; ++k) {
      keep[static_cast<size_t>(start + k * step)] = false;
    }
    size_t write = 0;
    for (size_t read = 0; read < impl_->size(); ++read) {
      if (keep[read]) {
        if (write != read) {
          impl_->data()[write] = impl_->data()[read];
        }
        ++write;
      }
    }
    impl_->resize(write);
  }

  py::object iter() const
  {
    // C++ iterator yielding 1-char strs; no intermediary list.
    py::object self_obj = py::reinterpret_borrow<py::object>(
      py::cast(this, py::return_value_policy::reference));
    return py::cast(StringIterator<CharT>(
      self_obj, impl_->data(), impl_->size(), /* reverse */ false));
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
      for (size_t i = 0; i < impl_->size(); ++i) {
        if (impl_->data()[i] == *target) {
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
      self_obj, impl_->data(), impl_->size(), /* reverse */ true));
  }

  void append(py::handle value)
  {
    impl_->push_back(coerce_code_unit<CharT>(value));
  }

  void insert(py::handle index, py::handle value)
  {
    // Coerce before mutating so a coercion failure leaves the string intact.
    CharT c = coerce_code_unit<CharT>(value);
    py::ssize_t i = py::cast<py::ssize_t>(index);
    if (i < 0) {
      i += static_cast<py::ssize_t>(impl_->size());
    }
    if (i < 0) {
      i = 0;
    }
    if (i > static_cast<py::ssize_t>(impl_->size())) {
      i = static_cast<py::ssize_t>(impl_->size());
    }
    // In-place shift right (no temporary string).
    impl_->resize(impl_->size() + 1);
    for (size_t j = impl_->size() - 1; j > static_cast<size_t>(i); --j) {
      impl_->data()[j] = impl_->data()[j - 1];
    }
    impl_->data()[static_cast<size_t>(i)] = c;
  }

  void clear()
  {
    impl_->clear();
  }

  // ---- equality -----------------------------------------------------------

  py::object eq(py::handle other) const
  {
    if (py::isinstance<StringWrapper<CharT>>(other)) {
      auto & o = py::cast<StringWrapper<CharT> &>(other);
      return py::bool_(impl_->view() == o.impl_->view());
    }
    if (py::isinstance<py::str>(other)) {
      return py::bool_(str_() == py::cast<std::string>(other));
    }
    return py::bool_(false);
  }

  py::object ne(py::handle other) const
  {
    return py::bool_(!py::cast<bool>(eq(other)));
  }

  int compare_lexicographic(py::handle other) const
  {
    std::string other_str;
    if (py::isinstance<StringWrapper<CharT>>(other)) {
      other_str = py::cast<StringWrapper<CharT> &>(other).str_();
    } else if (py::isinstance<py::str>(other)) {
      other_str = py::cast<std::string>(other);
    } else {
      throw py::type_error("cannot compare string with this type");
    }
    const std::string self = str_();
    if (self < other_str) {
      return -1;
    }
    if (other_str < self) {
      return 1;
    }
    return 0;
  }

  py::object lt(py::handle other) const { return py::bool_(compare_lexicographic(other) < 0); }
  py::object le(py::handle other) const { return py::bool_(compare_lexicographic(other) <= 0); }
  py::object gt(py::handle other) const { return py::bool_(compare_lexicographic(other) > 0); }
  py::object ge(py::handle other) const { return py::bool_(compare_lexicographic(other) >= 0); }

  py::object hash() const
  {
    return py::int_(py::hash(py::str(str_())));
  }

  py::object mod(py::handle other) const
  {
    return py::str(str_()).attr("__mod__")(other);
  }

  py::object rmod(py::handle other) const
  {
    return py::str(str_()).attr("__rmod__")(other);
  }

  py::object format(py::handle spec) const
  {
    std::string s = py::cast<std::string>(spec);
    return py::str(str_()).attr("__format__")(py::str(s));
  }

  // ---- arithmetic (concatenation / repetition; builtin-return policy) -----

  py::object add(py::handle other) const
  {
    if (py::isinstance<StringWrapper<CharT>>(other)) {
      auto & o = py::cast<StringWrapper<CharT> &>(other);
      return py::str(str_() + o.str_());
    }
    if (py::isinstance<py::str>(other)) {
      return py::str(str_() + py::cast<std::string>(other));
    }
    return not_implemented();
  }

  py::object radd(py::handle other) const
  {
    if (py::isinstance<py::str>(other)) {
      return py::str(py::cast<std::string>(other) + str_());
    }
    return not_implemented();
  }

  py::object mul(py::handle other) const
  {
    py::ssize_t n = py::cast<py::ssize_t>(other);
    if (n <= 0) {
      return py::str("");
    }
    std::string tmp;
    tmp.reserve(str_().size() * static_cast<size_t>(n));
    for (py::ssize_t i = 0; i < n; ++i) {
      tmp += str_();
    }
    return py::str(tmp);
  }

  py::object iadd(py::handle other)
  {
    if (py::isinstance<StringWrapper<CharT>>(other)) {
      auto & o = py::cast<StringWrapper<CharT> &>(other);
      impl_->append(o.impl_->view());
      return py::cast(this, py::return_value_policy::reference);
    }
    if (py::isinstance<py::str>(other)) {
      std::string bytes = to_char_bytes<CharT>(other);
      impl_->append(std::basic_string_view<CharT>(
        reinterpret_cast<const CharT *>(bytes.data()), bytes.size() / sizeof(CharT)));
      return py::cast(this, py::return_value_policy::reference);
    }
    return not_implemented();
  }

  py::object imul(py::handle other)
  {
    py::ssize_t n = py::cast<py::ssize_t>(other);
    if (n <= 0) {
      impl_->clear();
      return py::cast(this, py::return_value_policy::reference);
    }
    std::basic_string<CharT> tmp;
    tmp.reserve(impl_->size() * static_cast<size_t>(n));
    for (py::ssize_t i = 0; i < n; ++i) {
      tmp.append(impl_->data(), impl_->size());
    }
    impl_->assign(tmp);
    return py::cast(this, py::return_value_policy::reference);
  }

  // ---- buffer / numpy -----------------------------------------------------

  py::buffer_info buffer() const
  {
    return py::buffer_info(
      const_cast<ViewT *>(reinterpret_cast<const ViewT *>(impl_->data())),
      sizeof(ViewT),
      py::format_descriptor<ViewT>::format(),
      1,
      {static_cast<py::ssize_t>(impl_->size())},
      {static_cast<py::ssize_t>(sizeof(ViewT))});
  }

  py::array_t<ViewT> numpy()
  {
    py::object self_obj = py::reinterpret_borrow<py::object>(
      py::cast(this, py::return_value_policy::reference));
    py::array_t<ViewT> arr(
      {static_cast<py::ssize_t>(impl_->size())},
      reinterpret_cast<const ViewT *>(impl_->data()), self_obj);
    return arr;
  }

  std::string repr() const
  {
    return class_name() + "(" + py::cast<std::string>(py::repr(py::str(str_()))) + ")";
  }

  // ---- bounds -------------------------------------------------------------

  size_t max_size() const
  {
    return impl_->max_size();
  }

  bool is_bounded() const
  {
    return impl_->is_bounded();
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

  std::unique_ptr<StringInterface<CharT>> impl_;
  py::object parent_;  // anchor (empty for standalone fixtures)
};

// Test-only bounded factory class (ADR-011): BoundedString.Make(bound, value).
template<typename CharT>
struct BoundedStringFactory
{
  static std::shared_ptr<StringWrapper<CharT>> make(size_t bound, const std::string & value)
  {
    return StringWrapper<CharT>::make_bounded(bound, value);
  }
};

template<typename CharT>
void register_string(py::module_ & m, const char * name)
{
  // Iterator class: yields 1-char strs; distinct name per CharT.
  py::class_<StringIterator<CharT>>(m,
    std::is_same_v<CharT, char> ? "StringIterator" : "WStringIterator")
    .def("__iter__", &StringIterator<CharT>::iter)
    .def("__next__", &StringIterator<CharT>::next);

  py::class_<StringWrapper<CharT>, std::shared_ptr<StringWrapper<CharT>>> cls(
    m, name, py::buffer_protocol());
  cls
    .def(py::init([]() { return StringWrapper<CharT>::make(""); }))
    .def(py::init([](py::handle h) {
      // to_char_bytes encodes str to UTF-8 (char) or UTF-16-LE (char16_t);
      // make() interprets the raw bytes as CharT units.
      return StringWrapper<CharT>::make(to_char_bytes<CharT>(h));
    }), py::arg("data"))
    .def_static("Make", [](py::handle h) {
      return StringWrapper<CharT>::make(to_char_bytes<CharT>(h));
    }, py::arg("value"))
    .def_property("value", &StringWrapper<CharT>::get_value, &StringWrapper<CharT>::set_value)
    .def("assign", &StringWrapper<CharT>::assign, py::arg("data"))
    .def("append_str", &StringWrapper<CharT>::append_str, py::arg("data"))
    .def("as_builtin", &StringWrapper<CharT>::as_builtin)
    .def("from_builtin", &StringWrapper<CharT>::from_builtin, py::arg("value"))
    .def("max_size", &StringWrapper<CharT>::max_size)
    .def("is_bounded", &StringWrapper<CharT>::is_bounded)
    .def("__str__", &StringWrapper<CharT>::str_)
    .def("__repr__", &StringWrapper<CharT>::repr)
    .def("__eq__", &StringWrapper<CharT>::eq)
    .def("__ne__", &StringWrapper<CharT>::ne)
    .def("__lt__", &StringWrapper<CharT>::lt)
    .def("__le__", &StringWrapper<CharT>::le)
    .def("__gt__", &StringWrapper<CharT>::gt)
    .def("__ge__", &StringWrapper<CharT>::ge)
    .def("__hash__", &StringWrapper<CharT>::hash)
    .def("__mod__", &StringWrapper<CharT>::mod)
    .def("__rmod__", &StringWrapper<CharT>::rmod)
    .def("__format__", &StringWrapper<CharT>::format, py::arg("spec"))
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

  // Bounded factory: BoundedString.Make(bound, value) / BoundedWString.Make.
  py::class_<BoundedStringFactory<CharT>>(m,
    std::is_same_v<CharT, char> ? "BoundedString" : "BoundedWString")
    .def_static("Make", [](size_t bound, py::handle h) {
      return StringWrapper<CharT>::make_bounded(bound, to_char_bytes<CharT>(h));
    }, py::arg("bound"), py::arg("value"));
}

// Registers the String/WString wrapper classes (defined in string.cpp).
void register_strings(py::module_ & m);

// Element conversion for string elements in message-element containers
// (sequences/arrays of strings): to_py produces a parent-anchored
// StringWrapper view; to_builtin a str; from_py copies out of a str.
template<>
struct ElementTraits<rosidl_runtime_cpp::String>
{
  static py::object to_py(rosidl_runtime_cpp::String & v, py::object parent)
  {
    return py::cast(std::make_shared<StringWrapper<char>>(
      std::unique_ptr<StringInterface<char>>(new StringReference<char, 0>(&v)),
      std::move(parent)));
  }

  static py::object to_builtin(const rosidl_runtime_cpp::String & v)
  {
    return py::str(std::string(v.data(), v.size()));
  }

  static rosidl_runtime_cpp::String from_py(py::handle h)
  {
    rosidl_runtime_cpp::String s;
    s.assign(py::cast<std::string>(h));
    return s;
  }
};

template<>
struct ElementTraits<rosidl_runtime_cpp::WString>
{
  static py::object to_py(rosidl_runtime_cpp::WString & v, py::object parent)
  {
    return py::cast(std::make_shared<StringWrapper<char16_t>>(
      std::unique_ptr<StringInterface<char16_t>>(new StringReference<char16_t, 0>(&v)),
      std::move(parent)));
  }

  static py::object to_builtin(const rosidl_runtime_cpp::WString & v)
  {
    std::string bytes(
      reinterpret_cast<const char *>(v.data()), v.size() * sizeof(char16_t));
    return py::bytes(bytes).attr("decode")("utf-16-le");
  }

  static rosidl_runtime_cpp::WString from_py(py::handle h)
  {
    rosidl_runtime_cpp::WString s;
    s.assign(py::cast<std::u16string>(h));
    return s;
  }
};

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SRC__STRING_HPP_