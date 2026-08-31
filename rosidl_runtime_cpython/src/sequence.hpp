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

/// Native primitive sequence wrappers over
/// rosidl_runtime_cpp::BasicSequence<T, N> (ADR-002/003/004/011): non-owning
/// reference views through the type-erased SequenceInterface /
/// SequenceReference<T, UpperBound> (unbounded + bounded), list-based value
/// slices, buffer/NumPy views (primitives only), as_builtin/from_builtin, and
/// test-only .Make() / Bounded*Sequence.Make(bound, values) factories
/// (aliasing shared_ptr fixtures).
///
/// One class per distinct element type (UInt8Sequence, Int32Sequence, ...);
/// aliased IDL element types (uint8/octet/byte/char) share UInt8Sequence.
/// Element conversion dispatches on is_numpy_compatible<T> via ElementTraits:
/// primitives convert to/from builtins; message element types (Phase 1B)
/// convert to/from parent-anchored message wrappers. buffer()/numpy() are
/// primitive-only (message-element registrations omit them).

#ifndef ROSIDL_RUNTIME_CPYTHON__SRC__SEQUENCE_HPP_
#define ROSIDL_RUNTIME_CPYTHON__SRC__SEQUENCE_HPP_

#include "primitives_common.hpp"

#include "rosidl_runtime_cpp/experimental/sequence.hpp"

namespace rosidl_runtime_cpython
{

// ============================================================================
// Sequence interface / reference (type-erased, hides the bound)
// ============================================================================

template<typename T>
class SequenceInterface
{
public:
  virtual ~SequenceInterface() = default;

  virtual size_t size() const = 0;
  virtual T & at(size_t i) = 0;
  virtual const T & at(size_t i) const = 0;
  virtual T * data() = 0;
  virtual const T * data() const = 0;
  virtual void push_back(T v) = 0;
  virtual void resize(size_t n) = 0;
  virtual void clear() = 0;
  virtual size_t max_size() const = 0;
  virtual bool is_bounded() const = 0;

  // Bulk mutation, delegating to BasicSequence::insert/erase/assign. The
  // vector overloads handle materialized sources (bool included, since
  // std::vector<bool> has no data()); the pointer overloads are the zero-copy
  // path for typed buffer sources (numpy arrays, buffers, other wrappers).
  virtual void insert(size_t index, size_t count, T value) = 0;
  virtual void insert(size_t index, const std::vector<T> & values) = 0;
  virtual void insert(size_t index, const T * first, size_t count) = 0;
  virtual void erase(size_t index, size_t count) = 0;
  virtual void assign(const std::vector<T> & values) = 0;
  virtual void assign(const T * first, size_t count) = 0;
};

template<typename T, size_t UpperBound = 0>
class SequenceReference final : public SequenceInterface<T>
{
public:
  explicit SequenceReference(rosidl_runtime_cpp::BasicSequence<T, UpperBound> * value)
  : value_(value)
  {
  }

  size_t size() const override
  {
    return value_->size();
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

  void push_back(T v) override
  {
    value_->push_back(v);
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
    // Unbounded: max elements (matches BasicSequence::max_size():
    // SIZE_MAX / sizeof(T)).
    return UpperBound == 0
      ? std::numeric_limits<size_t>::max() / sizeof(T)
      : UpperBound;
  }

  bool is_bounded() const override
  {
    return UpperBound != 0;
  }

  void insert(size_t index, size_t count, T value) override
  {
    value_->insert(index, count, value);
  }

  void insert(size_t index, const std::vector<T> & values) override
  {
    value_->insert(index, values.begin(), values.end());
  }

  void insert(size_t index, const T * first, size_t count) override
  {
    value_->insert(index, first, first + count);
  }

  void erase(size_t index, size_t count) override
  {
    value_->erase(index, count);
  }

  void assign(const std::vector<T> & values) override
  {
    value_->assign(values.begin(), values.end());
  }

  void assign(const T * first, size_t count) override
  {
    value_->assign(first, first + count);
  }

private:
  rosidl_runtime_cpp::BasicSequence<T, UpperBound> * value_;
};

// ============================================================================
// Sequence wrapper
// ============================================================================

// C++ iterator over a SequenceWrapper's elements, yielding builtins
// (primitives) or parent-anchored message views (message elements).
// Holds a strong reference to the parent so the wrapper (and its buffer)
// stays alive; mutating the sequence during iteration invalidates the
// iterator (standard C++ iterator semantics).
template<typename T>
class SequenceIterator
{
public:
  SequenceIterator(py::object parent, T * data, size_t size, bool reverse)
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
class SequenceWrapper;

// Fixture data owner for standalone sequences (test-only .Make() factories,
// ADR-011): the owner holds the BasicSequence; the wrapper is a non-owning
// view.
template<typename T, size_t UpperBound = 0>
struct SequenceData
{
  rosidl_runtime_cpp::BasicSequence<T, UpperBound> data;
  SequenceWrapper<T> wrapper;
  SequenceData()
  : data(), wrapper(std::unique_ptr<SequenceInterface<T>>(
      new SequenceReference<T, UpperBound>(&data)))
  {
  }
};

template<typename T>
class SequenceWrapper
{
public:
  // Non-owning: references C++ sequence storage through a type-erased
  // interface (ADR-011). `parent` is the Python parent object (message
  // handle) that keeps the storage alive; empty for standalone fixtures.
  explicit SequenceWrapper(std::unique_ptr<SequenceInterface<T>> impl, py::object parent = {})
  : impl_(std::move(impl)), parent_(std::move(parent))
  {
  }

  // Test-only factories (ADR-011): unbounded (bound 0) and bounded (1..9).
  static std::shared_ptr<SequenceWrapper<T>> make(py::handle values)
  {
    auto owner = std::make_shared<SequenceData<T, 0>>();
    auto w = std::shared_ptr<SequenceWrapper<T>>(owner, &owner->wrapper);
    w->extend(values);
    return w;
  }

  static std::shared_ptr<SequenceWrapper<T>> make_bounded(size_t bound, py::handle values)
  {
    // bound 1..9 -> index 0..8; bound 0 underflows to SIZE_MAX and bound >= 10
    // is out of range, so both fall through to the invalid_argument below.
    auto result = compile_time_switch<9>(bound - 1, [&](auto ic) {
      constexpr size_t B = decltype(ic)::value + 1;
      auto owner = std::make_shared<SequenceData<T, B>>();
      auto w = std::shared_ptr<SequenceWrapper<T>>(owner, &owner->wrapper);
      w->extend(values);
      return w;
    });
    if (!result) {
      throw std::invalid_argument("BoundedSequence.Make supports bound 1..9");
    }
    return *result;
  }

  // ---- builtin interop ----------------------------------------------------

  py::object as_builtin() const
  {
    // Explicit escape hatch: a list of builtins (primitives) or dicts
    // (message elements).
    py::list result;
    for (size_t i = 0; i < impl_->size(); ++i) {
      result.append(ElementTraits<T>::to_builtin(impl_->at(i)));
    }
    return result;
  }

  void from_builtin(py::handle h)
  {
    assign(h);
  }

  // ---- sequence protocol --------------------------------------------------

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
        throw py::index_error("sequence index out of range");
      }
      // Primitives: builtin. Message elements: parent-anchored view.
      return ElementTraits<T>::to_py(impl_->at(static_cast<size_t>(i)), self_obj());
    }
    // Slice: return a list of builtins/views (builtin-return policy, ADR-002
    // amendment).
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
        throw py::index_error("sequence index out of range");
      }
      impl_->at(static_cast<size_t>(i)) = ElementTraits<T>::from_py(value);
      return;
    }
    // Slice assignment.
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(impl_->size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    if constexpr (is_numpy_compatible<T>::value) {
      // Typed buffer source (SequenceWrapper<T>, numpy array of dtype T, or
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
        py::array_t<T> view(
          {static_cast<py::ssize_t>(impl_->size())}, impl_->data(), self_obj());
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
      py::array_t<T> view(
        {static_cast<py::ssize_t>(impl_->size())}, impl_->data(), self_obj());
      view[slice] = to_numpy(tmp);
    } else {
      for (py::ssize_t k = 0; k < slicelength; ++k) {
        impl_->at(static_cast<size_t>(start + k * step)) = tmp[static_cast<size_t>(k)];
      }
    }
  }

  py::object iter() const
  {
    return py::cast(SequenceIterator<T>(
      self_obj(), impl_->data(), impl_->size(), /* reverse */ false));
  }

  py::object reversed() const
  {
    return py::cast(SequenceIterator<T>(
      self_obj(), impl_->data(), impl_->size(), /* reverse */ true));
  }

  py::object contains(py::handle value) const
  {
    // C++ linear scan (no per-element Python calls). Type mismatches return
    // False (Python `in` semantics), not an exception.
    T target;
    try {
      target = ElementTraits<T>::from_py(value);
    } catch (const std::exception &) {
      // builtin_exception (py::type_error etc.) does not set the Python error
      // until translated; clear any pending error just in case.
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
    if (py::isinstance<SequenceWrapper<T>>(other)) {
      auto & o = py::cast<SequenceWrapper<T> &>(other);
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
    if (py::isinstance<SequenceWrapper<T>>(other)) {
      auto & o = py::cast<SequenceWrapper<T> &>(other);
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
      throw py::type_error("cannot compare sequence with this type");
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

  // ---- deletion -----------------------------------------------------------

  void delitem(py::handle index)
  {
    if (py::isinstance<py::int_>(index)) {
      py::ssize_t i = py::cast<py::ssize_t>(index);
      if (i < 0) {
        i += static_cast<py::ssize_t>(impl_->size());
      }
      if (i < 0 || i >= static_cast<py::ssize_t>(impl_->size())) {
        throw py::index_error("sequence index out of range");
      }
      impl_->erase(static_cast<size_t>(i), 1);
      return;
    }
    py::slice slice = py::cast<py::slice>(index);
    py::ssize_t start, stop, step, slicelength;
    if (!slice.compute(static_cast<py::ssize_t>(impl_->size()), &start, &stop, &step, &slicelength)) {
      throw py::error_already_set();
    }
    if (step == 1) {
      impl_->erase(static_cast<size_t>(start), static_cast<size_t>(slicelength));
    } else if (step > 0) {
      // Delete from the end so earlier indices stay valid.
      for (py::ssize_t k = slicelength - 1; k >= 0; --k) {
        impl_->erase(static_cast<size_t>(start + k * step), 1);
      }
    } else {
      // Negative step: indices decrease; delete from the front so later
      // (smaller) indices stay valid.
      for (py::ssize_t k = 0; k < slicelength; ++k) {
        impl_->erase(static_cast<size_t>(start + k * step), 1);
      }
    }
  }

  // ---- concatenation / repetition (list semantics) ------------------------

  std::shared_ptr<SequenceWrapper<T>> add(py::handle other) const
  {
    auto owner = std::make_shared<SequenceData<T, 0>>();
    auto w = std::shared_ptr<SequenceWrapper<T>>(owner, &owner->wrapper);
    for (size_t i = 0; i < impl_->size(); ++i) {
      w->impl_->push_back(impl_->at(i));
    }
    for (auto item : py::iter(other)) {
      w->impl_->push_back(ElementTraits<T>::from_py(item));
    }
    return w;
  }

  std::shared_ptr<SequenceWrapper<T>> radd(py::handle other) const
  {
    auto owner = std::make_shared<SequenceData<T, 0>>();
    auto w = std::shared_ptr<SequenceWrapper<T>>(owner, &owner->wrapper);
    for (auto item : py::iter(other)) {
      w->impl_->push_back(ElementTraits<T>::from_py(item));
    }
    for (size_t i = 0; i < impl_->size(); ++i) {
      w->impl_->push_back(impl_->at(i));
    }
    return w;
  }

  py::object iadd(py::handle other)
  {
    extend(other);
    return py::cast(this, py::return_value_policy::reference);
  }

  std::shared_ptr<SequenceWrapper<T>> mul(py::handle count) const
  {
    py::ssize_t n = py::cast<py::ssize_t>(count);
    if (n < 0) {
      n = 0;
    }
    auto owner = std::make_shared<SequenceData<T, 0>>();
    auto w = std::shared_ptr<SequenceWrapper<T>>(owner, &owner->wrapper);
    for (py::ssize_t rep = 0; rep < n; ++rep) {
      for (size_t i = 0; i < impl_->size(); ++i) {
        w->impl_->push_back(impl_->at(i));
      }
    }
    return w;
  }

  std::shared_ptr<SequenceWrapper<T>> rmul(py::handle count) const
  {
    return mul(count);
  }

  py::object imul(py::handle count)
  {
    py::ssize_t n = py::cast<py::ssize_t>(count);
    if (n <= 0) {
      impl_->clear();
      return py::cast(this, py::return_value_policy::reference);
    }
    // Materialize self first: growth may reallocate and dangle the source.
    std::vector<T> tmp;
    for (size_t i = 0; i < impl_->size(); ++i) {
      tmp.push_back(impl_->at(i));
    }
    for (py::ssize_t rep = 1; rep < n; ++rep) {
      for (size_t i = 0; i < tmp.size(); ++i) {
        impl_->push_back(tmp[i]);
      }
    }
    return py::cast(this, py::return_value_policy::reference);
  }

  // ---- search / transform (list semantics) --------------------------------

  py::ssize_t index(py::handle value) const
  {
    T target = ElementTraits<T>::from_py(value);
    for (size_t i = 0; i < impl_->size(); ++i) {
      if (impl_->at(i) == target) {
        return static_cast<py::ssize_t>(i);
      }
    }
    throw py::value_error("value not in sequence");
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

  std::shared_ptr<SequenceWrapper<T>> copy() const
  {
    auto owner = std::make_shared<SequenceData<T, 0>>();
    auto w = std::shared_ptr<SequenceWrapper<T>>(owner, &owner->wrapper);
    for (size_t i = 0; i < impl_->size(); ++i) {
      w->impl_->push_back(impl_->at(i));
    }
    return w;
  }

  // ---- mutation -----------------------------------------------------------

  void append(py::handle value)
  {
    impl_->push_back(ElementTraits<T>::from_py(value));
  }

  void extend(py::handle values)
  {
    if constexpr (is_numpy_compatible<T>::value) {
      // Self-aliasing source (s.extend(s)): materialize before mutating —
      // insert may reallocate and dangle the source.
      auto snap = snapshot_if_aliasing(values);
      if (snap) {
        impl_->insert(impl_->size(), *snap);
        return;
      }
      // Zero-copy bulk append from a typed buffer source (SequenceWrapper<T>,
      // numpy array of dtype T, or buffer).
      auto src = as_typed_buffer(values);
      if (src) {
        impl_->insert(impl_->size(), src->first, src->second);
        return;
      }
    }
    // List/iterable source: materialize (the necessary temporary for Python
    // scalars), then bulk insert. Message elements are always materialized so
    // self-aliasing views cannot dangle on reallocation.
    std::vector<T> tmp;
    for (auto item : py::iter(values)) {
      tmp.push_back(ElementTraits<T>::from_py(item));
    }
    impl_->insert(impl_->size(), tmp);
  }

  void insert(py::handle index, py::handle value)
  {
    // Coerce before mutating so a coercion failure leaves the sequence intact.
    T v = ElementTraits<T>::from_py(value);
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
    impl_->insert(static_cast<size_t>(i), 1, v);
  }

  py::object pop(py::handle index)
  {
    py::ssize_t i = py::cast<py::ssize_t>(index);
    if (i < 0) {
      i += static_cast<py::ssize_t>(impl_->size());
    }
    if (i < 0 || i >= static_cast<py::ssize_t>(impl_->size())) {
      throw py::index_error("pop index out of range");
    }
    // For message elements, to_py returns a LIVE view into storage[i], which
    // the erase below overwrites; return a detached copy (dict) instead.
    py::object result;
    if constexpr (is_numpy_compatible<T>::value) {
      result = ElementTraits<T>::to_py(impl_->at(static_cast<size_t>(i)), self_obj());
    } else {
      result = ElementTraits<T>::to_builtin(impl_->at(static_cast<size_t>(i)));
    }
    impl_->erase(static_cast<size_t>(i), 1);
    return result;
  }

  void remove(py::handle value)
  {
    T target = ElementTraits<T>::from_py(value);
    for (size_t i = 0; i < impl_->size(); ++i) {
      if (impl_->at(i) == target) {
        impl_->erase(i, 1);
        return;
      }
    }
    throw py::value_error("value not in sequence");
  }

  void clear()
  {
    impl_->clear();
  }

  void resize(py::handle new_size)
  {
    py::ssize_t n = py::cast<py::ssize_t>(new_size);
    if (n < 0) {
      throw py::value_error("negative resize");
    }
    impl_->resize(static_cast<size_t>(n));
  }

  void assign(py::handle values)
  {
    if constexpr (is_numpy_compatible<T>::value) {
      // Self-aliasing source (s.assign(s) keeps contents): materialize before
      // clearing.
      auto snap = snapshot_if_aliasing(values);
      if (snap) {
        impl_->assign(*snap);
        return;
      }
      // Zero-copy bulk replace from a typed buffer source.
      auto src = as_typed_buffer(values);
      if (src) {
        impl_->assign(src->first, src->second);
        return;
      }
    }
    // List/iterable source: materialize, then bulk assign. Message elements
    // are always materialized so self-aliasing views cannot dangle.
    std::vector<T> tmp;
    for (auto item : py::iter(values)) {
      tmp.push_back(ElementTraits<T>::from_py(item));
    }
    impl_->assign(tmp);
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
    py::object self_obj = py::reinterpret_borrow<py::object>(
      py::cast(this, py::return_value_policy::reference));
    py::array_t<T> arr(
      {static_cast<py::ssize_t>(impl_->size())}, impl_->data(), self_obj);
    return arr;
  }

  std::string repr() const
  {
    return class_name() + "(" + py::cast<std::string>(py::repr(as_builtin())) + ")";
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

  // Extract a typed (data, count) from a SequenceWrapper<T>, C-contiguous
  // numpy array of dtype T, or C-contiguous buffer source; nullopt otherwise.
  // Primitive-only: message element types have no numpy/buffer representation.
  static std::optional<std::pair<const T *, size_t>> as_typed_buffer(py::handle h)
  {
    if constexpr (is_numpy_compatible<T>::value) {
      if (py::isinstance<SequenceWrapper<T>>(h)) {
        auto & o = py::cast<SequenceWrapper<T> &>(h);
        return std::make_pair(o.impl_->data(), o.impl_->size());
      }
      return typed_buffer_from_numpy_or_buffer<T>(h);
    }
    return std::nullopt;
  }

  // If *h* aliases this sequence's own buffer, return a snapshot copy;
  // nullopt otherwise. Guards extend/assign against reallocation dangling
  // the source.
  std::optional<std::vector<T>> snapshot_if_aliasing(py::handle h) const
  {
    auto src = as_typed_buffer(h);
    if (src) {
      const T * d = impl_->data();
      if (!std::less<const T *>()(src->first, d) &&
        std::less<const T *>()(src->first, d + impl_->size()))
      {
        return std::vector<T>(src->first, src->first + src->second);
      }
    }
    return std::nullopt;
  }

  // The Python object wrapping this wrapper (used to anchor message views).
  py::object self_obj() const
  {
    return py::reinterpret_borrow<py::object>(
      py::cast(const_cast<SequenceWrapper<T> *>(this), py::return_value_policy::reference));
  }

  static std::string class_name()
  {
    if constexpr (std::is_same_v<T, bool>) {
      return "BoolSequence";
    } else if constexpr (std::is_same_v<T, uint8_t>) {
      return "UInt8Sequence";
    } else if constexpr (std::is_same_v<T, uint16_t>) {
      return "UInt16Sequence";
    } else if constexpr (std::is_same_v<T, uint32_t>) {
      return "UInt32Sequence";
    } else if constexpr (std::is_same_v<T, uint64_t>) {
      return "UInt64Sequence";
    } else if constexpr (std::is_same_v<T, int8_t>) {
      return "Int8Sequence";
    } else if constexpr (std::is_same_v<T, int16_t>) {
      return "Int16Sequence";
    } else if constexpr (std::is_same_v<T, int32_t>) {
      return "Int32Sequence";
    } else if constexpr (std::is_same_v<T, int64_t>) {
      return "Int64Sequence";
    } else if constexpr (std::is_same_v<T, float>) {
      return "Float32Sequence";
    } else if constexpr (std::is_same_v<T, double>) {
      return "Float64Sequence";
    } else if constexpr (std::is_same_v<T, long double>) {
      return "LongDoubleSequence";
    } else {
      return "Sequence";  // message-element sequences
    }
  }

  std::unique_ptr<SequenceInterface<T>> impl_;
  py::object parent_;  // anchor (empty for standalone fixtures)
};

// Test-only bounded factory class (ADR-011):
// BoundedUInt8Sequence.Make(bound, values), etc.
template<typename T>
struct BoundedSequenceFactory
{
  static std::shared_ptr<SequenceWrapper<T>> make(size_t bound, py::handle values)
  {
    return SequenceWrapper<T>::make_bounded(bound, values);
  }
};

template<typename T>
void register_sequence(py::module_ & m, const char * name)
{
  py::class_<SequenceIterator<T>>(m, (std::string(name) + "Iterator").c_str())
    .def("__iter__", &SequenceIterator<T>::iter)
    .def("__next__", &SequenceIterator<T>::next);

  py::class_<SequenceWrapper<T>, std::shared_ptr<SequenceWrapper<T>>> cls(
    m, name, py::buffer_protocol());
  cls
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
    .def("__lt__", &SequenceWrapper<T>::lt)
    .def("__le__", &SequenceWrapper<T>::le)
    .def("__gt__", &SequenceWrapper<T>::gt)
    .def("__ge__", &SequenceWrapper<T>::ge)
    .def("__add__", &SequenceWrapper<T>::add)
    .def("__radd__", &SequenceWrapper<T>::radd)
    .def("__iadd__", &SequenceWrapper<T>::iadd)
    .def("__mul__", &SequenceWrapper<T>::mul)
    .def("__rmul__", &SequenceWrapper<T>::rmul)
    .def("__imul__", &SequenceWrapper<T>::imul)
    .def("index", &SequenceWrapper<T>::index, py::arg("value"))
    .def("count", &SequenceWrapper<T>::count, py::arg("value"))
    .def("sort", &SequenceWrapper<T>::sort)
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
    .def("numpy", &SequenceWrapper<T>::numpy)
    .def_buffer(&SequenceWrapper<T>::buffer)
    .def("__repr__", &SequenceWrapper<T>::repr);

  // Bounded factory: BoundedUInt8Sequence.Make(bound, values), etc.
  py::class_<BoundedSequenceFactory<T>>(m, (std::string("Bounded") + name).c_str())
    .def_static("Make", &BoundedSequenceFactory<T>::make,
      py::arg("bound"), py::arg("values"));
}

// Registers every sequence wrapper class (defined in sequence.cpp).
void register_sequences(py::module_ & m);

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SRC__SEQUENCE_HPP_