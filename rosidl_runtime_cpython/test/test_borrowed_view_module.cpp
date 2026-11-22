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

#include <pybind11/pybind11.h>

#include <memory>
#include <vector>

#include "rosidl_runtime_cpython/borrowed_view.hpp"
#include "rosidl_runtime_cpython/message_view.hpp"

namespace py = pybind11;
using rosidl_runtime_cpython::BorrowedView;
using rosidl_runtime_cpython::BorrowedViewState;

struct TestMessage
{
  int field{42};
  std::vector<int> values{1, 2};
};

class TestLoan
{
public:
  TestLoan()
  : message_(std::make_unique<TestMessage>()), state_(std::make_shared<BorrowedViewState>())
  {}

  BorrowedView<TestMessage> view()
  {
    state_->require_valid();
    return BorrowedView<TestMessage>(message_.get(), state_);
  }

  void return_loan()
  {
    state_->invalidate();
    message_.reset();
  }

  ~TestLoan() {return_loan();}

private:
  std::unique_ptr<TestMessage> message_;
  std::shared_ptr<BorrowedViewState> state_;
};

PYBIND11_MODULE(_test_borrowed_view, module)
{
  rosidl_runtime_cpython::bind_sequence_view(module);
  py::class_<BorrowedView<int>>(module, "Field")
  .def_property(
    "value", [](const BorrowedView<int> & field) {return field.get();},
    [](const BorrowedView<int> & field, int value) {field.get() = value;});
  py::class_<BorrowedView<TestMessage>>(module, "Message")
  .def_property_readonly(
    "field", [](const BorrowedView<TestMessage> & message) {
      return BorrowedView<int>(&message.get().field, message.state());
    })
  .def(
    "values", [](const BorrowedView<TestMessage> & message, bool read_only) {
      return rosidl_runtime_cpython::make_sequence_view<true>(
        message.get().values, message.state(), read_only, {},
        [](int & item, const std::shared_ptr<BorrowedViewState> &, bool,
        const std::shared_ptr<void> &) {return py::cast(item);},
        [](int & item, py::handle value) {item = py::cast<int>(value);});
    }, py::arg("read_only") = false)
  .def(
    "value_ref", [](const BorrowedView<TestMessage> & message, std::size_t index) {
      auto & values = message.get().values;
      return BorrowedView<int>(&values.at(index), message.state()->elements_state(&values));
    });
  py::class_<TestLoan>(module, "Loan")
  .def(py::init<>())
  .def("view", &TestLoan::view)
  .def("return_loan", &TestLoan::return_loan);
}
