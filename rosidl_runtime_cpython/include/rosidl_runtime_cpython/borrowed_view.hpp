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

#ifndef ROSIDL_RUNTIME_CPYTHON__BORROWED_VIEW_HPP_
#define ROSIDL_RUNTIME_CPYTHON__BORROWED_VIEW_HPP_

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace rosidl_runtime_cpython
{

/// Shared validity of a loan and all Python-visible views derived from it.
/// Access and invalidation must be serialized by the caller.
class BorrowedViewState : public std::enable_shared_from_this<BorrowedViewState>
{
public:
  BorrowedViewState() = default;
  explicit BorrowedViewState(std::shared_ptr<const BorrowedViewState> parent)
  : parent_(std::move(parent))
  {
    if (!parent_) {
      throw std::invalid_argument("Borrowed view state requires a parent");
    }
  }

  void invalidate() noexcept {valid_ = false;}
  bool valid() const noexcept {return valid_ && (!parent_ || parent_->valid());}

  void require_valid() const
  {
    if (!valid()) {
      throw std::logic_error("The borrowed message is no longer valid");
    }
  }

  /// Share an invalidation state among wrappers for elements of the same container.
  std::shared_ptr<BorrowedViewState> elements_state(const void * container)
  {
    require_valid();
    if (!container) {
      throw std::invalid_argument("Container address is null");
    }
    auto & entry = elements_[container];
    auto result = entry.lock();
    if (!result || !result->valid()) {
      result = std::make_shared<BorrowedViewState>(shared_from_this());
      entry = result;
    }
    return result;
  }

  /// Call before any mutation that could invalidate container element addresses.
  void invalidate_elements(const void * container)
  {
    require_valid();
    if (!container) {
      throw std::invalid_argument("Container address is null");
    }
    auto entry = elements_.find(container);
    if (entry != elements_.end()) {
      if (auto state = entry->second.lock()) {
        state->invalidate();
      }
      elements_.erase(entry);
    }
  }

private:
  bool valid_{true};
  std::shared_ptr<const BorrowedViewState> parent_;
  std::unordered_map<const void *, std::weak_ptr<BorrowedViewState>> elements_;
};

/// A checked reference into middleware-owned storage.
/// References returned by get() must not escape the synchronized operation.
template<typename T>
class BorrowedView
{
public:
  BorrowedView(T * value, std::shared_ptr<BorrowedViewState> state)
  : value_(value), state_(std::move(state))
  {
    if (!value_ || !state_) {
      throw std::invalid_argument("Borrowed view requires a value and lifetime state");
    }
  }

  T & get() const
  {
    state_->require_valid();
    return *value_;
  }

  const std::shared_ptr<BorrowedViewState> & state() const noexcept {return state_;}

private:
  T * value_;
  std::shared_ptr<BorrowedViewState> state_;
};

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__BORROWED_VIEW_HPP_
