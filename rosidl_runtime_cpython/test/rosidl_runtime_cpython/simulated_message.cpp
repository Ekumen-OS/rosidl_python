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

/// Test-only simulated experimental messages (Phase 1B): proves the
/// MessageHandleBase pattern — top-level ownership via unique_ptr with a
/// custom deleter, nested message views (non-owning, parent-anchored), and
/// field getters returning non-owning wrappers — before the generator exists.
///
/// The wrapper classes (ScalarWrapper, StringWrapper, SequenceWrapper,
/// ArrayWrapper) are registered by the _primitives module; import it first so
/// pybind11 resolves the shared types across modules (spike 1).

#include <cstdint>
#include <memory>

#include "array.hpp"
#include "message_handle.hpp"
#include "scalar.hpp"
#include "sequence.hpp"
#include "string.hpp"

namespace rosidl_runtime_cpython
{

// ----------------------------------------------------------------------------
// Simulated generated experimental messages (mirror the Image/Header shape)
// ----------------------------------------------------------------------------

struct SimHeader
{
  Scalar<uint64_t> stamp{0};
  rosidl_runtime_cpp::String frame_id;
  Scalar<uint32_t> seq{0};

  static inline int alive = 0;
  SimHeader() { ++alive; }
  ~SimHeader() { --alive; }

  // Field-wise equality (the generator emits this for message types; used by
  // contains/remove on message-element containers).
  bool operator==(const SimHeader & o) const
  {
    return stamp.get() == o.stamp.get() &&
      frame_id.view() == o.frame_id.view() &&
      seq.get() == o.seq.get();
  }
};

struct SimMessage
{
  SimHeader header;
  Scalar<uint32_t> width{0};
  Scalar<uint32_t> height{0};
  rosidl_runtime_cpp::String encoding;
  rosidl_runtime_cpp::Sequence<uint8_t> pixels;
  rosidl_runtime_cpp::Array<uint8_t, 4> data;
  rosidl_runtime_cpp::Sequence<SimHeader> headers;  // message-element sequence
  rosidl_runtime_cpp::Array<SimHeader, 2> pair;     // message-element array

  static inline int alive = 0;
  SimMessage() { ++alive; }
  ~SimMessage() { --alive; }
};

// ----------------------------------------------------------------------------
// Generated-style message handles (field getters anchor wrappers to `self`)
// ----------------------------------------------------------------------------

class SimHeaderHandle : public MessageHandleBase<SimHeader>
{
public:
  using MessageHandleBase<SimHeader>::MessageHandleBase;

  std::shared_ptr<ScalarWrapper<uint64_t>> stamp()
  {
    return std::make_shared<ScalarWrapper<uint64_t>>(&msg_->stamp, py::cast(this));
  }

  void set_stamp(py::handle value)
  {
    msg_->stamp.get() = ElementTraits<uint64_t>::from_py(value);
  }

  std::shared_ptr<StringWrapper<char>> frame_id()
  {
    return std::make_shared<StringWrapper<char>>(
      std::unique_ptr<StringInterface<char>>(new StringReference<char, 0>(&msg_->frame_id)),
      py::cast(this));
  }

  void set_frame_id(py::handle value)
  {
    msg_->frame_id.assign(py::cast<std::string>(value));
  }

  std::shared_ptr<ScalarWrapper<uint32_t>> seq()
  {
    return std::make_shared<ScalarWrapper<uint32_t>>(&msg_->seq, py::cast(this));
  }

  void set_seq(py::handle value)
  {
    msg_->seq.get() = ElementTraits<uint32_t>::from_py(value);
  }

  // Recursive builtin representation (dict).
  py::object as_builtin()
  {
    return as_builtin_dict(*msg_);
  }

  static py::object as_builtin_dict(const SimHeader & m)
  {
    py::dict d;
    d["stamp"] = py::cast(m.stamp.get());
    d["frame_id"] = py::str(std::string(m.frame_id.data(), m.frame_id.size()));
    d["seq"] = py::cast(m.seq.get());
    return d;
  }

  // Build a SimHeader from a Python object: a SimHeaderHandle (copy) or a
  // dict of field values.
  static SimHeader from_py(py::handle h)
  {
    if (py::isinstance<SimHeaderHandle>(h)) {
      auto & handle = py::cast<SimHeaderHandle &>(h);
      return *handle.get();
    }
    if (py::isinstance<py::dict>(h)) {
      py::dict d = py::cast<py::dict>(h);
      SimHeader m;
      m.stamp.get() = py::cast<uint64_t>(d["stamp"]);
      m.frame_id.assign(py::cast<std::string>(d["frame_id"]));
      m.seq.get() = py::cast<uint32_t>(d["seq"]);
      return m;
    }
    throw py::type_error("expected a SimHeader or a dict");
  }
};

// Element conversion for message elements (ADR-011): to_py produces a
// parent-anchored SimHeaderHandle view; to_builtin a dict; from_py copies a
// SimHeader out of a SimHeaderHandle or dict. Equality uses operator==.
template<>
struct ElementTraits<SimHeader>
{
  static py::object to_py(SimHeader & v, py::object parent)
  {
    return py::cast(std::make_shared<SimHeaderHandle>(&v, std::move(parent)));
  }

  static py::object to_builtin(const SimHeader & v)
  {
    return SimHeaderHandle::as_builtin_dict(v);
  }

  static SimHeader from_py(py::handle h)
  {
    return SimHeaderHandle::from_py(h);
  }
};

class SimMessageHandle : public MessageHandleBase<SimMessage>
{
public:
  using MessageHandleBase<SimMessage>::MessageHandleBase;

  std::shared_ptr<SimHeaderHandle> header()
  {
    return std::make_shared<SimHeaderHandle>(&msg_->header, py::cast(this));
  }

  void set_header(py::handle value)
  {
    msg_->header = SimHeaderHandle::from_py(value);
  }

  std::shared_ptr<ScalarWrapper<uint32_t>> width()
  {
    return std::make_shared<ScalarWrapper<uint32_t>>(&msg_->width, py::cast(this));
  }

  void set_width(py::handle value)
  {
    msg_->width.get() = ElementTraits<uint32_t>::from_py(value);
  }

  std::shared_ptr<ScalarWrapper<uint32_t>> height()
  {
    return std::make_shared<ScalarWrapper<uint32_t>>(&msg_->height, py::cast(this));
  }

  void set_height(py::handle value)
  {
    msg_->height.get() = ElementTraits<uint32_t>::from_py(value);
  }

  std::shared_ptr<StringWrapper<char>> encoding()
  {
    return std::make_shared<StringWrapper<char>>(
      std::unique_ptr<StringInterface<char>>(new StringReference<char, 0>(&msg_->encoding)),
      py::cast(this));
  }

  void set_encoding(py::handle value)
  {
    msg_->encoding.assign(py::cast<std::string>(value));
  }

  std::shared_ptr<SequenceWrapper<uint8_t>> pixels()
  {
    return std::make_shared<SequenceWrapper<uint8_t>>(
      std::unique_ptr<SequenceInterface<uint8_t>>(new SequenceReference<uint8_t, 0>(&msg_->pixels)),
      py::cast(this));
  }

  void set_pixels(py::handle value)
  {
    std::vector<uint8_t> tmp;
    for (auto item : py::iter(value)) {
      tmp.push_back(ElementTraits<uint8_t>::from_py(item));
    }
    msg_->pixels.assign(tmp.begin(), tmp.end());
  }

  std::shared_ptr<ArrayWrapper<uint8_t>> data()
  {
    return std::make_shared<ArrayWrapper<uint8_t>>(
      std::unique_ptr<ArrayInterface<uint8_t>>(new ArrayReference<uint8_t, 4>(&msg_->data)),
      py::cast(this));
  }

  void set_data(py::handle value)
  {
    std::vector<uint8_t> tmp;
    for (auto item : py::iter(value)) {
      tmp.push_back(ElementTraits<uint8_t>::from_py(item));
    }
    if (tmp.size() != 4) {
      throw py::value_error("array requires exactly 4 elements");
    }
    for (size_t i = 0; i < 4; ++i) {
      msg_->data[i] = tmp[i];
    }
  }

  std::shared_ptr<SequenceWrapper<SimHeader>> headers()
  {
    return std::make_shared<SequenceWrapper<SimHeader>>(
      std::unique_ptr<SequenceInterface<SimHeader>>(new SequenceReference<SimHeader, 0>(&msg_->headers)),
      py::cast(this));
  }

  void set_headers(py::handle value)
  {
    std::vector<SimHeader> tmp;
    for (auto item : py::iter(value)) {
      tmp.push_back(SimHeaderHandle::from_py(item));
    }
    msg_->headers.assign(tmp.begin(), tmp.end());
  }

  std::shared_ptr<ArrayWrapper<SimHeader>> pair()
  {
    return std::make_shared<ArrayWrapper<SimHeader>>(
      std::unique_ptr<ArrayInterface<SimHeader>>(new ArrayReference<SimHeader, 2>(&msg_->pair)),
      py::cast(this));
  }

  void set_pair(py::handle value)
  {
    std::vector<SimHeader> tmp;
    for (auto item : py::iter(value)) {
      tmp.push_back(SimHeaderHandle::from_py(item));
    }
    if (tmp.size() != 2) {
      throw py::value_error("array requires exactly 2 elements");
    }
    for (size_t i = 0; i < 2; ++i) {
      msg_->pair[i] = tmp[i];
    }
  }
};

}  // namespace rosidl_runtime_cpython

PYBIND11_MODULE(_simulated, m)
{
  using namespace rosidl_runtime_cpython;
  m.doc() = "Test-only simulated experimental messages (Phase 1B)";

  // The field getters return wrapper types registered by _primitives; import
  // it so this module is self-contained regardless of caller import order.
  py::module_::import("rosidl_runtime_cpython._primitives");

  // Message-element sequence/array wrappers (no buffer/numpy: message
  // elements are not numpy-compatible). The generator would emit these.
  py::class_<SequenceIterator<SimHeader>>(m, "SimHeaderSequenceIterator")
    .def("__iter__", &SequenceIterator<SimHeader>::iter)
    .def("__next__", &SequenceIterator<SimHeader>::next);

  py::class_<SequenceWrapper<SimHeader>, std::shared_ptr<SequenceWrapper<SimHeader>>>(
    m, "SimHeaderSequence")
    .def("__len__", &SequenceWrapper<SimHeader>::len)
    .def("__getitem__", &SequenceWrapper<SimHeader>::getitem)
    .def("__setitem__", &SequenceWrapper<SimHeader>::setitem)
    .def("__iter__", &SequenceWrapper<SimHeader>::iter)
    .def("__reversed__", &SequenceWrapper<SimHeader>::reversed)
    .def("__contains__", &SequenceWrapper<SimHeader>::contains)
    .def("append", &SequenceWrapper<SimHeader>::append, py::arg("value"))
    .def("extend", &SequenceWrapper<SimHeader>::extend, py::arg("values"))
    .def("insert", &SequenceWrapper<SimHeader>::insert, py::arg("index"), py::arg("value"))
    .def("pop", &SequenceWrapper<SimHeader>::pop, py::arg("index") = py::int_(-1))
    .def("remove", &SequenceWrapper<SimHeader>::remove, py::arg("value"))
    .def("clear", &SequenceWrapper<SimHeader>::clear)
    .def("resize", &SequenceWrapper<SimHeader>::resize, py::arg("new_size"))
    .def("assign", &SequenceWrapper<SimHeader>::assign, py::arg("values"))
    .def("as_builtin", &SequenceWrapper<SimHeader>::as_builtin)
    .def("from_builtin", &SequenceWrapper<SimHeader>::from_builtin, py::arg("value"))
    .def("max_size", &SequenceWrapper<SimHeader>::max_size)
    .def("is_bounded", &SequenceWrapper<SimHeader>::is_bounded)
    .def("__repr__", &SequenceWrapper<SimHeader>::repr);

  py::class_<ArrayIterator<SimHeader>>(m, "SimHeaderArrayIterator")
    .def("__iter__", &ArrayIterator<SimHeader>::iter)
    .def("__next__", &ArrayIterator<SimHeader>::next);

  py::class_<ArrayWrapper<SimHeader>, std::shared_ptr<ArrayWrapper<SimHeader>>>(
    m, "SimHeaderArray")
    .def("size", &ArrayWrapper<SimHeader>::size)
    .def("__len__", &ArrayWrapper<SimHeader>::len)
    .def("__getitem__", &ArrayWrapper<SimHeader>::getitem)
    .def("__setitem__", &ArrayWrapper<SimHeader>::setitem)
    .def("__iter__", &ArrayWrapper<SimHeader>::iter)
    .def("__reversed__", &ArrayWrapper<SimHeader>::reversed)
    .def("__contains__", &ArrayWrapper<SimHeader>::contains)
    .def("as_builtin", &ArrayWrapper<SimHeader>::as_builtin)
    .def("from_builtin", &ArrayWrapper<SimHeader>::from_builtin, py::arg("value"))
    .def("__repr__", &ArrayWrapper<SimHeader>::repr);

py::class_<SimHeaderHandle, std::shared_ptr<SimHeaderHandle>>(m, "SimHeader")
    .def(py::init([]() {
      return std::make_shared<SimHeaderHandle>(std::make_unique<SimHeader>());
    }))
    .def_property("stamp", &SimHeaderHandle::stamp, &SimHeaderHandle::set_stamp)
    .def_property("frame_id", &SimHeaderHandle::frame_id, &SimHeaderHandle::set_frame_id)
    .def_property("seq", &SimHeaderHandle::seq, &SimHeaderHandle::set_seq)
    .def("as_builtin", &SimHeaderHandle::as_builtin)
    .def_static("alive_count", []() { return SimHeader::alive; });

  py::class_<SimMessageHandle, std::shared_ptr<SimMessageHandle>>(m, "SimMessage")
    .def(py::init([]() {
      return std::make_shared<SimMessageHandle>(std::make_unique<SimMessage>());
    }))
    .def_property("header", &SimMessageHandle::header, &SimMessageHandle::set_header)
    .def_property("width", &SimMessageHandle::width, &SimMessageHandle::set_width)
    .def_property("height", &SimMessageHandle::height, &SimMessageHandle::set_height)
    .def_property("encoding", &SimMessageHandle::encoding, &SimMessageHandle::set_encoding)
    .def_property("pixels", &SimMessageHandle::pixels, &SimMessageHandle::set_pixels)
    .def_property("data", &SimMessageHandle::data, &SimMessageHandle::set_data)
    .def_property("headers", &SimMessageHandle::headers, &SimMessageHandle::set_headers)
    .def_property("pair", &SimMessageHandle::pair, &SimMessageHandle::set_pair)
    .def_static("alive_count", []() { return SimMessage::alive; });
}