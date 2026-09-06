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

/// Native fixed-array wrappers over rosidl_runtime_cpp::Array<T, N>
/// (ADR-004 amendment, ADR-011): non-owning reference views hiding N behind
/// ArrayInterface<T> / ArrayReference<T, N>. Indexing is backed by a NumPy
/// view for primitives (scalar index -> numpy scalar, slice -> numpy view)
/// and by wrapper-based access for message elements (views / copies);
/// test-only .Make(N, values) factories support N in 1..9.
///
/// Element conversion dispatches on is_numpy_compatible<T> via ElementTraits:
/// primitives convert to/from builtins; message element types (Phase 1B)
/// convert to/from parent-anchored message wrappers. buffer()/numpy() are
/// primitive-only (message-element registrations omit them).

#ifndef ROSIDL_RUNTIME_CPYTHON__SRC__ARRAY_HPP_
#define ROSIDL_RUNTIME_CPYTHON__SRC__ARRAY_HPP_

#include "primitives_common.hpp"
#include "string.hpp"

#include "rosidl_runtime_cpp/experimental/array.hpp"

namespace rosidl_runtime_cpython
{

// ============================================================================
// Array interface / reference (type-erased, hides N)
// ============================================================================

template<typename T>
class ArrayInterface
{
public:
  virtual ~ArrayInterface() = default;

  virtual size_t size() const = 0;
  virtual T & at(size_t i) = 0;
  virtual const T & at(size_t i) const = 0;
  virtual T * data() = 0;
  virtual const T * data() const = 0;
};

template<typename T, size_t N>
class ArrayReference final : public ArrayInterface<T>
{
public:
  explicit ArrayReference(rosidl_runtime_cpp::Array<T, N> * value)
  : value_(value)
  {
  }

  size_t size() const override
  {
    return N;
  }

  T & at(size_t i) override
  {
    return value_->at(i);  // checked: throws std::out_of_range -> IndexError
  }

  const T & at(size_t i) const override
  {
    return value_->at(i);
  }

  T * data() override
  {
    return value_->data();
  }

  const T * data() const override
  {
    return value_->data();
  }

private:
  rosidl_runtime_cpp::Array<T, N> * value_;
};

// ============================================================================
// Array wrapper
// ============================================================================

// C++ iterator over an ArrayWrapper's elements, yielding builtins
// (primitives) or parent-anchored message views (message elements).
// Holds a strong reference to the parent so the wrapper (and its buffer)
// stays alive.
template<typename T>
class ArrayIterator
{
public:
  ArrayIterator(py::object parent, T * data, size_t size, bool reverse)
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
      return ElementTraits<T>::to_py(data_[--pos_], parent_);
    }
    if (pos_ >= size_) {
      throw py::stop_iteration();
    }
    return ElementTraits<T>::to_py(data_[pos_++], parent_);
  }

  py::object iter()
  {
    return py::cast(this, py::return_value_policy::reference);
  }

private:
  py::object parent_;  // keeps the wrapper alive
  T * data_;
  size_t size_;
  size_t pos_;
  bool reverse_;
};

// Forward declaration (defined below).
template<typename T>
class ArrayWrapper;

// Fixture data owner for standalone arrays (test-only .Make() factories,
// ADR-011): the owner holds the Array; the wrapper is a non-owning view.
template<typename T, size_t N>
struct ArrayData
{
  rosidl_runtime_cpp::Array<T, N> data;
  ArrayWrapper<T> wrapper;
  ArrayData()
  : data(), wrapper(std::unique_ptr<ArrayInterface<T>>(
      new ArrayReference<T, N>(&data)))
  {
  }
};

template<typename T>
class ArrayWrapper
{
public:
  // Non-owning: references C++ array storage through a type-erased
  // interface (ADR-011). `parent` is the Python parent object (message
  // handle) that keeps the storage alive; empty for standalone fixtures.
  explicit ArrayWrapper(std::unique_ptr<ArrayInterface<T>> impl, py::object parent = {})
  : impl_(std::move(impl)), parent_(std::move(parent))
  {
  }

  // Test-only factory (ADR-011): Array.Make(N, values) for N in 1..9.
  static std::shared_ptr<ArrayWrapper<T>> make(size_t n, py::handle values)
  {
    // N 1..9 -> index 0..8; N 0 underflows to SIZE_MAX and N >= 10 is out of
    // range, so both fall through to the invalid_argument below.
    auto result = compile_time_switch<9>(n - 1, [&](auto ic) {
      constexpr size_t N = decltype(ic)::value + 1;
      auto owner = std::make_shared<ArrayData<T, N>>();
      auto w = std::shared_ptr<ArrayWrapper<T>>(owner, &owner->wrapper);
      // No Python object exists yet, so no numpy view: copy directly from a
      // typed source, or materialize the values and copy into the fixed-size
      // storage.
      fill_from(w->impl_.get(), values);
      return w;
    });
    if (!result) {
      throw std::invalid_argument("Array.Make supports N in 1..9");
    }
    return *result;
  }

  size_t size() const
  {
    return impl_->size();
  }

  // C++-level element access (used by generated code and benchmarks); the
  // Python-facing API is __getitem__/__setitem__.
  T get(size_t i) const
  {
    return impl_->at(i);  // checked: throws std::out_of_range -> IndexError
  }

  void set(size_t i, T v)
  {
    impl_->at(i) = v;
  }

  // ---- builtin interop ----------------------------------------------------

  py::object as_builtin() const
  {
    py::list result;
    for (size_t i = 0; i < impl_->size(); ++i) {
      result.append(ElementTraits<T>::to_builtin(impl_->at(i)));
    }
    return result;
  }

  void from_builtin(py::handle h)
  {
    // Strict fill (ADR-003): copy directly from a typed source, or
    // materialize the values and copy into the fixed-size storage.
    fill_from(impl_.get(), h);
  }

  // Source-dispatch fill (used by generated message field setters): zero-copy
  // from a same-dtype numpy/buffer source, materialize otherwise.
  void assign(py::handle value)
  {
    if constexpr (is_numpy_compatible<T>::value) {
      auto src = as_typed_buffer(value);
      if (src) {
        if (src->second != impl_->size()) {
          throw py::value_error("array requires exactly "
            + std::to_string(impl_->size()) + " elements, got "
            + std::to_string(src->second));
        }
        std::copy(src->first, src->first + src->second, impl_->data());
        return;
      }
    }
    std::vector<T> tmp;
    for (auto item : py::iter(value)) {
      tmp.push_back(ElementTraits<T>::from_py(item));
    }
    if (tmp.size() != impl_->size()) {
      throw py::value_error("array requires exactly "
        + std::to_string(impl_->size()) + " elements, got "
        + std::to_string(tmp.size()));
    }
    for (size_t i = 0; i < impl_->size(); ++i) {
      impl_->at(i) = tmp[i];
    }
  }

  // ---- sequence protocol (NumPy-view backed) ------------------------------

  py::ssize_t len() const
  {
    return static_cast<py::ssize_t>(impl_->size());
  }

  py::object getitem(py::handle index) const
  {
    // int -> builtin (primitives) or parent-anchored view (message elements);
    // slice -> list of builtins/views. numpy semantics are opt-in via
    // .numpy() / the buffer protocol (ADR-003 amendment).
    if (py::isinstance<py::int_>(index)) {
      py::ssize_t i = py::cast<py::ssize_t>(index);
      if (i < 0) {
        i += static_cast<py::ssize_t>(impl_->size());
      }
      if (i < 0 || i >= static_cast<py::ssize_t>(impl_->size())) {
        throw py::index_error("array index out of range");
      }
      return ElementTraits<T>::to_py(impl_->at(static_cast<size_t>(i)), self_obj());
    }
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(impl_->size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    py::list result;
    for (py::ssize_t k = 0; k < slicelength; ++k) {
      result.append(ElementTraits<T>::to_py(
        impl_->at(static_cast<size_t>(start + k * step)), self_obj()));
    }
    return result;
  }

  void setitem(py::handle index, py::handle value)
  {
    if (py::isinstance<py::int_>(index)) {
      py::ssize_t i = py::cast<py::ssize_t>(index);
      if (i < 0) {
        i += static_cast<py::ssize_t>(impl_->size());
      }
      if (i < 0 || i >= static_cast<py::ssize_t>(impl_->size())) {
        throw py::index_error("array index out of range");
      }
      // Scalar assignment is strict (ADR-003): numpy semantics apply only to
      // numpy-array sources on slice assignment.
      impl_->at(static_cast<size_t>(i)) = ElementTraits<T>::from_py(value);
      return;
    }
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(impl_->size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    if constexpr (is_numpy_compatible<T>::value) {
      // Typed buffer source (ArrayWrapper<T>, numpy array of dtype T, or
      // buffer): zero-copy numpy bulk assignment. A strided (step != 1)
      // self-aliasing source is snapshotted first — numpy does not handle
      // strided overlap — while contiguous overlap is handled by numpy
      // (memmove semantics).
      auto src = as_typed_buffer(value);
      if (src) {
        // numpy handles length/broadcasting; snapshot a strided (step != 1)
        // self-aliasing source first — numpy does not handle strided overlap
        // — while contiguous overlap is handled by numpy (memmove semantics).
        std::optional<std::vector<T>> snapshot;
        const T * data = src->first;
        const T * d = impl_->data();
        if (step != 1 && !std::less<const T *>()(data, d) &&
          std::less<const T *>()(data, d + impl_->size()))
        {
          snapshot.emplace(data, data + src->second);
        }
        py::array_t<T> view = numpy_view();
        py::array_t<T> src_arr = snapshot
          ? to_numpy(*snapshot)
          : py::array_t<T>({static_cast<py::ssize_t>(src->second)}, data);
        view[slice] = src_arr;
        return;
      }
    }
    // List/iterable source: materialize (the necessary temporary for Python
    // scalars), then assign. For numpy-compatible types the write is a numpy
    // bulk copy (handles any step); for message elements it is element-wise.
    std::vector<T> tmp;
    for (auto item : py::iter(value)) {
      tmp.push_back(ElementTraits<T>::from_py(item));
    }
    if (tmp.size() != static_cast<size_t>(slicelength)) {
      throw py::value_error("attempt to assign sequence of size "
        + std::to_string(tmp.size()) + " to extended slice of size "
        + std::to_string(slicelength));
    }
    if constexpr (is_numpy_compatible<T>::value) {
      py::array_t<T> view = numpy_view();
      view[slice] = to_numpy(tmp);
    } else {
      for (py::ssize_t k = 0; k < slicelength; ++k) {
        impl_->at(static_cast<size_t>(start + k * step)) = tmp[static_cast<size_t>(k)];
      }
    }
  }

  py::object iter() const
  {
    return py::cast(ArrayIterator<T>(
      self_obj(), impl_->data(), impl_->size(), /* reverse */ false));
  }

  py::object reversed() const
  {
    return py::cast(ArrayIterator<T>(
      self_obj(), impl_->data(), impl_->size(), /* reverse */ true));
  }

  py::object contains(py::handle value) const
  {
    T target;
    try {
      target = ElementTraits<T>::from_py(value);
    } catch (const std::exception &) {
      if (PyErr_Occurred()) {
        PyErr_Clear();
      }
      return py::bool_(false);
    }
    for (size_t i = 0; i < impl_->size(); ++i) {
      if (impl_->at(i) == target) {
        return py::bool_(true);
      }
    }
    return py::bool_(false);
  }

  // ---- comparison (list semantics) ----------------------------------------

  bool eq(py::handle other) const
  {
    if (py::isinstance<ArrayWrapper<T>>(other)) {
      auto & o = py::cast<ArrayWrapper<T> &>(other);
      if (o.impl_->size() != impl_->size()) {
        return false;
      }
      for (size_t i = 0; i < impl_->size(); ++i) {
        if (!(impl_->at(i) == o.impl_->at(i))) {
          return false;
        }
      }
      return true;
    }
    std::vector<T> other_vec;
    if (!materialize(other, other_vec)) {
      return false;
    }
    if (other_vec.size() != impl_->size()) {
      return false;
    }
    for (size_t i = 0; i < impl_->size(); ++i) {
      if (!(impl_->at(i) == other_vec[i])) {
        return false;
      }
    }
    return true;
  }

  bool ne(py::handle other) const
  {
    return !eq(other);
  }

  int compare_lexicographic(py::handle other) const
  {
    std::vector<T> other_vec;
    if (py::isinstance<ArrayWrapper<T>>(other)) {
      auto & o = py::cast<ArrayWrapper<T> &>(other);
      const size_t common = std::min(impl_->size(), o.impl_->size());
      for (size_t i = 0; i < common; ++i) {
        if (impl_->at(i) < o.impl_->at(i)) {
          return -1;
        }
        if (o.impl_->at(i) < impl_->at(i)) {
          return 1;
        }
      }
      return impl_->size() < o.impl_->size() ? -1 :
        (impl_->size() > o.impl_->size() ? 1 : 0);
    }
    if (!materialize(other, other_vec)) {
      throw py::type_error("cannot compare array with this type");
    }
    const size_t common = std::min(impl_->size(), other_vec.size());
    for (size_t i = 0; i < common; ++i) {
      if (impl_->at(i) < other_vec[i]) {
        return -1;
      }
      if (other_vec[i] < impl_->at(i)) {
        return 1;
      }
    }
    return impl_->size() < other_vec.size() ? -1 :
      (impl_->size() > other_vec.size() ? 1 : 0);
  }

  bool lt(py::handle other) const { return compare_lexicographic(other) < 0; }
  bool le(py::handle other) const { return compare_lexicographic(other) <= 0; }
  bool gt(py::handle other) const { return compare_lexicographic(other) > 0; }
  bool ge(py::handle other) const { return compare_lexicographic(other) >= 0; }

  // ---- search / transform (list semantics) --------------------------------

  py::ssize_t index(py::handle value) const
  {
    T target = ElementTraits<T>::from_py(value);
    for (size_t i = 0; i < impl_->size(); ++i) {
      if (impl_->at(i) == target) {
        return static_cast<py::ssize_t>(i);
      }
    }
    throw py::value_error("value not in array");
  }

  py::ssize_t count(py::handle value) const
  {
    T target = ElementTraits<T>::from_py(value);
    py::ssize_t result = 0;
    for (size_t i = 0; i < impl_->size(); ++i) {
      if (impl_->at(i) == target) {
        ++result;
      }
    }
    return result;
  }

  void sort()
  {
    std::sort(impl_->data(), impl_->data() + impl_->size());
  }

  void reverse()
  {
    std::reverse(impl_->data(), impl_->data() + impl_->size());
  }

  void fill(py::handle value)
  {
    T v = ElementTraits<T>::from_py(value);
    std::fill(impl_->data(), impl_->data() + impl_->size(), v);
  }

  // ---- buffer / numpy -----------------------------------------------------

  py::buffer_info buffer() const
  {
    return py::buffer_info(
      const_cast<T *>(impl_->data()),
      sizeof(T),
      py::format_descriptor<T>::format(),
      1,
      {static_cast<py::ssize_t>(impl_->size())},
      {static_cast<py::ssize_t>(sizeof(T))});
  }

  py::array_t<T> numpy()
  {
    return numpy_view();
  }

  std::string repr() const
  {
    return class_name() + "(" + py::cast<std::string>(py::repr(as_builtin())) + ")";
  }

private:
  // Materialize an iterable into a vector<T>; false on coercion failure.
  static bool materialize(py::handle h, std::vector<T> & out)
  {
    try {
      for (auto item : py::iter(h)) {
        out.push_back(ElementTraits<T>::from_py(item));
      }
      return true;
    } catch (const std::exception &) {
      if (PyErr_Occurred()) {
        PyErr_Clear();
      }
      return false;
    }
  }

  // Fill the fixed-size storage from *h*: copy directly from a typed source
  // (ArrayWrapper<T>, C-contiguous numpy array of dtype T, or C-contiguous
  // buffer), or materialize an iterable (the necessary temporary for Python
  // scalars) and copy. The element count must match the storage size exactly.
  static void fill_from(ArrayInterface<T> * impl, py::handle h)
  {
    if constexpr (is_numpy_compatible<T>::value) {
      auto src = as_typed_buffer(h);
      if (src) {
        if (src->second != impl->size()) {
          throw py::value_error("array requires exactly "
            + std::to_string(impl->size()) + " elements, got "
            + std::to_string(src->second));
        }
        std::copy(src->first, src->first + src->second, impl->data());
        return;
      }
    }
    std::vector<T> tmp;
    for (auto item : py::iter(h)) {
      tmp.push_back(ElementTraits<T>::from_py(item));
    }
    if (tmp.size() != impl->size()) {
      throw py::value_error("array requires exactly "
        + std::to_string(impl->size()) + " elements, got "
        + std::to_string(tmp.size()));
    }
    for (size_t i = 0; i < impl->size(); ++i) {
      impl->at(i) = tmp[i];
    }
  }

  // Extract a typed (data, count) from an ArrayWrapper<T>, C-contiguous numpy
  // array of dtype T, or C-contiguous buffer source; nullopt otherwise.
  // Primitive-only: message element types have no numpy/buffer representation.
  static std::optional<std::pair<const T *, size_t>> as_typed_buffer(py::handle h)
  {
    if constexpr (is_numpy_compatible<T>::value) {
      if (py::isinstance<ArrayWrapper<T>>(h)) {
        auto & o = py::cast<ArrayWrapper<T> &>(h);
        return std::make_pair(o.impl_->data(), o.impl_->size());
      }
      return typed_buffer_from_numpy_or_buffer<T>(h);
    }
    return std::nullopt;
  }

  // A NumPy view over the underlying C++ storage, anchored to the wrapper
  // Python object (base = self) so slices and numpy() keep the storage alive.
  py::array_t<T> numpy_view() const
  {
    py::object self_obj = py::reinterpret_borrow<py::object>(
      py::cast(const_cast<ArrayWrapper<T> *>(this), py::return_value_policy::reference));
    return py::array_t<T>(
      {static_cast<py::ssize_t>(impl_->size())},
      const_cast<T *>(impl_->data()), self_obj);
  }

  // The Python object wrapping this wrapper (used to anchor message views).
  py::object self_obj() const
  {
    return py::reinterpret_borrow<py::object>(
      py::cast(const_cast<ArrayWrapper<T> *>(this), py::return_value_policy::reference));
  }

  static std::string class_name()
  {
    if constexpr (std::is_same_v<T, bool>) {
      return "BoolArray";
    } else if constexpr (std::is_same_v<T, uint8_t>) {
      return "UInt8Array";
    } else if constexpr (std::is_same_v<T, uint16_t>) {
      return "UInt16Array";
    } else if constexpr (std::is_same_v<T, uint32_t>) {
      return "UInt32Array";
    } else if constexpr (std::is_same_v<T, uint64_t>) {
      return "UInt64Array";
    } else if constexpr (std::is_same_v<T, int8_t>) {
      return "Int8Array";
    } else if constexpr (std::is_same_v<T, int16_t>) {
      return "Int16Array";
    } else if constexpr (std::is_same_v<T, int32_t>) {
      return "Int32Array";
    } else if constexpr (std::is_same_v<T, int64_t>) {
      return "Int64Array";
    } else if constexpr (std::is_same_v<T, float>) {
      return "Float32Array";
    } else if constexpr (std::is_same_v<T, double>) {
      return "Float64Array";
    } else if constexpr (std::is_same_v<T, long double>) {
      return "LongDoubleArray";
    } else {
      return "Array";  // message-element arrays
    }
  }

  std::unique_ptr<ArrayInterface<T>> impl_;
  py::object parent_;  // anchor (empty for standalone fixtures)
};

template<typename T>
void register_array(py::module_ & m, const char * name)
{
  py::class_<ArrayIterator<T>>(m, (std::string(name) + "Iterator").c_str())
    .def("__iter__", &ArrayIterator<T>::iter)
    .def("__next__", &ArrayIterator<T>::next);

  py::class_<ArrayWrapper<T>, std::shared_ptr<ArrayWrapper<T>>> cls(
    m, name, py::buffer_protocol());
  cls
    .def_static("Make", &ArrayWrapper<T>::make, py::arg("N"), py::arg("values"))
    .def("size", &ArrayWrapper<T>::size)
    .def("as_builtin", &ArrayWrapper<T>::as_builtin)
    .def("from_builtin", &ArrayWrapper<T>::from_builtin, py::arg("value"))
    .def("assign", &ArrayWrapper<T>::assign, py::arg("value"))
    .def("__len__", &ArrayWrapper<T>::len)
    .def("__getitem__", &ArrayWrapper<T>::getitem)
    .def("__setitem__", &ArrayWrapper<T>::setitem)
    .def("__iter__", &ArrayWrapper<T>::iter)
    .def("__reversed__", &ArrayWrapper<T>::reversed)
    .def("__contains__", &ArrayWrapper<T>::contains)
    .def("__eq__", &ArrayWrapper<T>::eq)
    .def("__ne__", &ArrayWrapper<T>::ne)
    .def("__lt__", &ArrayWrapper<T>::lt)
    .def("__le__", &ArrayWrapper<T>::le)
    .def("__gt__", &ArrayWrapper<T>::gt)
    .def("__ge__", &ArrayWrapper<T>::ge)
    .def("index", &ArrayWrapper<T>::index, py::arg("value"))
    .def("count", &ArrayWrapper<T>::count, py::arg("value"))
    .def("sort", &ArrayWrapper<T>::sort)
    .def("reverse", &ArrayWrapper<T>::reverse)
    .def("fill", &ArrayWrapper<T>::fill, py::arg("value"))
    .def("numpy", &ArrayWrapper<T>::numpy)
    .def_buffer(&ArrayWrapper<T>::buffer)
    .def("__repr__", &ArrayWrapper<T>::repr);
}

// Registers every array wrapper class (defined in array.cpp).
void register_arrays(py::module_ & m);

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SRC__ARRAY_HPP_