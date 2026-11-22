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

#ifndef ROSIDL_RUNTIME_CPYTHON__SEQUENCE_VIEW_HPP_
#define ROSIDL_RUNTIME_CPYTHON__SEQUENCE_VIEW_HPP_

#include <pybind11/pybind11.h>

#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include "rosidl_runtime_cpython/borrowed_view.hpp"

namespace rosidl_runtime_cpython
{

namespace py = pybind11;

/// Checked, type-erased access to a generated array or sequence field.
class SequenceView
{
public:
  using Size = std::function<std::size_t()>;
  using Get = std::function<py::object(std::size_t)>;
  using Set = std::function<void(std::size_t, py::handle)>;
  using Append = std::function<void(py::handle)>;
  using Clear = std::function<void()>;

  SequenceView(
    const void * container, std::shared_ptr<BorrowedViewState> state, bool read_only,
    std::shared_ptr<void> owner, Size size, Get get, Set set, Append append = {},
    Clear clear = {})
  : container_(container), state_(std::move(state)), read_only_(read_only),
    owner_(std::move(owner)),
    size_(std::move(size)), get_(std::move(get)), set_(std::move(set)),
    append_(std::move(append)), clear_(std::move(clear))
  {
    if (!container_ || !state_ || !size_ || !get_ || !set_) {
      throw std::invalid_argument("Sequence view requires a container, state and accessors");
    }
  }

  py::ssize_t size() const
  {
    state_->require_valid();
    auto length = size_();
    if (length > static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max())) {
      throw std::overflow_error("Sequence is too large for Python");
    }
    return static_cast<py::ssize_t>(length);
  }

  py::object get(py::ssize_t index) const {return get_(checked_index(index));}

  void set(py::ssize_t index, py::handle value)
  {
    require_mutable();
    auto position = checked_index(index);
    state_->invalidate_elements(container_);
    set_(position, value);
  }

  void append(py::handle value)
  {
    require_mutable();
    if (!append_) {
      throw py::type_error("Cannot resize a fixed array");
    }
    state_->invalidate_elements(container_);
    append_(value);
  }

  void clear()
  {
    require_mutable();
    if (!clear_) {
      throw py::type_error("Cannot resize a fixed array");
    }
    state_->invalidate_elements(container_);
    clear_();
  }

  void assign(py::iterable values, bool mapping_elements = false)
  {
    require_mutable();
    if (py::isinstance<SequenceView>(values)) {
      const auto & other = py::cast<const SequenceView &>(values);
      other.state_->require_valid();
      if (other.container_ == container_) {
        return;
      }
    }
    py::list items(values);
    if (mapping_elements) {
      for (auto item : items) {
        if (!py::isinstance<py::dict>(item)) {
          throw py::type_error("Expected a mapping of message fields");
        }
      }
    }
    if (!append_) {
      if (items.size() != static_cast<std::size_t>(size())) {
        throw py::value_error("Fixed array assignment requires the same length");
      }
      for (std::size_t i = 0; i < items.size(); ++i) {
        set(static_cast<py::ssize_t>(i), items[i]);
      }
      return;
    }
    clear();
    for (auto item : items) {
      append(item);
    }
  }

private:
  std::size_t checked_index(py::ssize_t index) const
  {
    auto length = size();
    if (index < 0) {
      index += length;
    }
    if (index < 0 || index >= length) {
      throw py::index_error("Sequence index out of range");
    }
    return static_cast<std::size_t>(index);
  }

  void require_mutable() const
  {
    state_->require_valid();
    if (read_only_) {
      throw py::type_error("Subscription views are read-only");
    }
  }

  const void * container_;
  std::shared_ptr<BorrowedViewState> state_;
  bool read_only_;
  std::shared_ptr<void> owner_;
  Size size_;
  Get get_;
  Set set_;
  Append append_;
  Clear clear_;
};

inline void bind_sequence_view(py::module_ & module)
{
  py::class_<SequenceView>(module, "SequenceView", py::module_local())
  .def("__len__", &SequenceView::size)
  .def("__getitem__", &SequenceView::get)
  .def("__setitem__", &SequenceView::set)
  .def("append", &SequenceView::append)
  .def("clear", &SequenceView::clear);
}

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SEQUENCE_VIEW_HPP_
