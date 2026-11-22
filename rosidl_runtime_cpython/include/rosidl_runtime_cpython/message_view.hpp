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

#ifndef ROSIDL_RUNTIME_CPYTHON__MESSAGE_VIEW_HPP_
#define ROSIDL_RUNTIME_CPYTHON__MESSAGE_VIEW_HPP_

#include <memory>
#include <string>
#include <utility>

#include "rosidl_runtime_cpython/borrowed_view.hpp"
#include "rosidl_runtime_cpython/sequence_view.hpp"

namespace rosidl_runtime_cpython
{

inline void assign_fields(const py::object & target, py::handle values)
{
  if (!py::isinstance<py::dict>(values)) {
    throw py::type_error("Expected a mapping of message fields");
  }
  for (auto item : py::reinterpret_borrow<py::dict>(values)) {
    py::setattr(target, py::cast<std::string>(item.first).c_str(), item.second);
  }
}

template<typename View>
class MessageView
{
public:
  MessageView(
    View * value, std::shared_ptr<BorrowedViewState> state, bool read_only,
    std::shared_ptr<void> owner = {})
  : value_(value, std::move(state)), read_only_(read_only), owner_(std::move(owner))
  {}

  View & get() const {return value_.get();}
  const std::shared_ptr<BorrowedViewState> & state() const {return value_.state();}
  const std::shared_ptr<void> & owner() const {return owner_;}
  bool read_only() const {return read_only_;}

  void require_mutable() const
  {
    state()->require_valid();
    if (read_only_) {
      throw py::type_error("Subscription views are read-only");
    }
  }

private:
  BorrowedView<View> value_;
  bool read_only_;
  std::shared_ptr<void> owner_;
};

/// Accessors receive an element lifetime shared by all wrappers for the container.
template<bool Resizable, typename Container, typename Get, typename Set>
SequenceView
make_sequence_view(
  Container & container, std::shared_ptr<BorrowedViewState> state, bool read_only,
  std::shared_ptr<void> owner, Get get, Set set)
{
  BorrowedView<Container> checked(&container, state);
  auto length = [checked]() {return checked.get().size();};
  auto get_item = [checked, state, read_only, owner, get](std::size_t index) {
      auto & sequence = checked.get();
      return get(sequence[index], state->elements_state(&sequence), read_only, owner);
    };
  auto set_item = [checked, set](std::size_t index, py::handle value) {
      set(checked.get()[index], value);
    };
  SequenceView::Append append;
  SequenceView::Clear clear;
  if constexpr (Resizable) {
    append = [checked, set](py::handle value) {
        auto & sequence = checked.get();
        auto index = sequence.size();
        sequence.resize(index + 1);
        try {
          set(sequence[index], value);
        } catch (...) {
          sequence.resize(index);
          throw;
        }
      };
    clear = [checked]() {checked.get().clear();};
  }
  return SequenceView(
    &container, std::move(state), read_only, std::move(owner), std::move(length),
      std::move(get_item),
    std::move(set_item), std::move(append), std::move(clear));
}

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__MESSAGE_VIEW_HPP_
