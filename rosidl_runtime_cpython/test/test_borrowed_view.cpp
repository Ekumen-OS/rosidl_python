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

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "rosidl_runtime_cpython/borrowed_view.hpp"

TEST(BorrowedView, InvalidationReachesRetainedMemberViews)
{
  struct Message {int member;};
  Message message{1};
  auto state = std::make_shared<rosidl_runtime_cpython::BorrowedViewState>();
  rosidl_runtime_cpython::BorrowedView<Message> root(&message, state);
  rosidl_runtime_cpython::BorrowedView<int> child(&root.get().member, root.state());

  child.get() = 2;
  EXPECT_EQ(2, message.member);
  state->invalidate();
  EXPECT_THROW(root.get(), std::logic_error);
  EXPECT_THROW(child.get(), std::logic_error);
}

TEST(BorrowedView, RejectsNullInputs)
{
  int value = 0;
  auto state = std::make_shared<rosidl_runtime_cpython::BorrowedViewState>();
  EXPECT_THROW(rosidl_runtime_cpython::BorrowedView<int>(nullptr, state), std::invalid_argument);
  EXPECT_THROW(rosidl_runtime_cpython::BorrowedView<int>(&value, nullptr), std::invalid_argument);
}

TEST(BorrowedView, ChildStateCanBeInvalidatedWithoutInvalidatingParent)
{
  int value = 5;
  auto root = std::make_shared<rosidl_runtime_cpython::BorrowedViewState>();
  auto child = std::make_shared<rosidl_runtime_cpython::BorrowedViewState>(root);
  rosidl_runtime_cpython::BorrowedView<int> outer(&value, root);
  rosidl_runtime_cpython::BorrowedView<int> element(&value, child);
  child->invalidate();
  EXPECT_EQ(5, outer.get());
  EXPECT_THROW(element.get(), std::logic_error);

  auto replacement = std::make_shared<rosidl_runtime_cpython::BorrowedViewState>(root);
  rosidl_runtime_cpython::BorrowedView<int> new_element(&value, replacement);
  root->invalidate();
  EXPECT_THROW(new_element.get(), std::logic_error);
}

TEST(BorrowedView, ContainerMutationsInvalidateAllRetainedElements)
{
  int container = 0;
  int other_container = 0;
  int element = 1;
  auto root = std::make_shared<rosidl_runtime_cpython::BorrowedViewState>();
  auto first = root->elements_state(&container);
  auto second = root->elements_state(&container);
  auto other = root->elements_state(&other_container);
  auto nested = first->elements_state(&element);
  rosidl_runtime_cpython::BorrowedView<int> view(&element, second);
  rosidl_runtime_cpython::BorrowedView<int> nested_view(&element, nested);

  root->invalidate_elements(&container);
  EXPECT_THROW(view.get(), std::logic_error);
  EXPECT_THROW(nested_view.get(), std::logic_error);
  EXPECT_TRUE(root->valid());
  EXPECT_TRUE(other->valid());
  auto replacement = root->elements_state(&container);
  EXPECT_TRUE(replacement->valid());
  EXPECT_NE(first, replacement);
  root->invalidate();
  EXPECT_FALSE(replacement->valid());
  EXPECT_FALSE(other->valid());
}

TEST(BorrowedView, ContainerStateRejectsInvalidInputs)
{
  int container = 0;
  auto root = std::make_shared<rosidl_runtime_cpython::BorrowedViewState>();
  EXPECT_THROW(root->elements_state(nullptr), std::invalid_argument);
  EXPECT_THROW(root->invalidate_elements(nullptr), std::invalid_argument);
  root->invalidate();
  EXPECT_THROW(root->elements_state(&container), std::logic_error);
  EXPECT_THROW(root->invalidate_elements(&container), std::logic_error);
}
