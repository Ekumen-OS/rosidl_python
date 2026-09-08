@# generated from rosidl_generator_py/resource/msg__experimental_impl.cpp.em
@# generated code does not contain a copyright notice
@{
from rosidl_pycommon import convert_camel_case_to_lower_case_underscore
from rosidl_parser.definition import AbstractGenericString
from rosidl_parser.definition import AbstractNestedType
from rosidl_parser.definition import AbstractSequence
from rosidl_parser.definition import AbstractString
from rosidl_parser.definition import AbstractWString
from rosidl_parser.definition import Action
from rosidl_parser.definition import Array
from rosidl_parser.definition import BasicType
from rosidl_parser.definition import EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME
from rosidl_parser.definition import Message
from rosidl_parser.definition import NamespacedType
from rosidl_parser.definition import Service

pkg = package_name
interface_stem = convert_camel_case_to_lower_case_underscore(interface_path.stem)

bind_messages = []  # list of (message, include)
for message in content.get_elements_of_type(Message):
    bind_messages.append((message, None))
for service in content.get_elements_of_type(Service):
    bind_messages.append((service.request_message, None))
    bind_messages.append((service.response_message, None))
for action in content.get_elements_of_type(Action):
    bind_messages.append((action.goal, None))
    bind_messages.append((action.result, None))
    bind_messages.append((action.feedback, None))
}@
#include "@(pkg)/@(interface_path.parent)/experimental/detail/@(interface_stem)__cpython_binding.hpp"

namespace rosidl_runtime_cpython
{

@[for message, _ in bind_messages]@
@{
msg = message.structure.namespaced_type.name
msg_underscore = convert_camel_case_to_lower_case_underscore(msg)
msg_cpp = '::'.join(
    list(message.structure.namespaced_type.namespaces) + ['experimental', msg])
# Only direct messages carry a MessageTypeBridge: service/action constituents
# are deferred (their C++ typesupport handles are not generated yet).
has_bridge = 'msg' in message.structure.namespaced_type.namespaces
}@
@[if message.structure.namespaced_type.name in supported_messages]@
std::shared_ptr<@(msg)Handle> @(msg)Handle::create(py::kwargs kwargs)
{
  auto init = rosidl_runtime_cpp::MessageInitialization::ALL;
  for (auto item : kwargs) {
    if (py::cast<std::string>(item.first) == "_init") {
      init = to_init(item.second);
    }
  }
  auto h = std::make_shared<@(msg)Handle>(std::make_unique<@(msg_cpp)>());
  h->msg_->_reset(init);
  for (auto item : kwargs) {
    std::string key = py::cast<std::string>(item.first);
    if (key == "_init") {
      continue;
    }
    bool matched = false;
@[for member in message.structure.members]@
@[  if member.name != EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME]@
    @(member_create_code(member))
@[  end if]@
@[end for]@
    if (!matched) {
      throw py::type_error("@(msg)() got an unexpected keyword argument '" + key + "'");
    }
  }
  return h;
}

@(msg_cpp) @(msg)Handle::from_py(py::handle h)
{
  if (py::isinstance<@(msg)Handle>(h)) {
    auto & handle = py::cast<@(msg)Handle &>(h);
    return *handle.get();
  }
  if (py::isinstance<py::dict>(h)) {
    py::dict d = py::cast<py::dict>(h);
    @(msg_cpp) m;
@[for member in message.structure.members]@
@[  if member.name != EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME]@
    @(member_from_builtin_code(member, 'm'))
@[  end if]@
@[end for]@
    return m;
  }
  throw py::type_error("expected a @(msg) or a dict");
}

@[for member in message.structure.members]@
@[  if member.name != EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME]@
@(member_getter_impl(member, msg + 'Handle'))

@(member_setter_impl(member, msg + 'Handle'))

@[  end if]@
@[end for]@
py::object @(msg)Handle::as_builtin()
{
  return as_builtin_dict(*msg_);
}

py::object @(msg)Handle::as_builtin_dict(const @(msg_cpp) & m)
{
  py::dict d;
@[for member in message.structure.members]@
@[  if member.name != EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME]@
  @(member_as_builtin_code(member))
@[  end if]@
@[end for]@
  return d;
}

void @(msg)Handle::from_builtin(py::handle h)
{
  py::dict d = py::cast<py::dict>(h);
@[for member in message.structure.members]@
@[  if member.name != EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME]@
  @(member_from_builtin_code(member))
@[  end if]@
@[end for]@
}

bool @(msg)Handle::eq(py::handle other) const
{
  if (!py::isinstance<@(msg)Handle>(other)) {
    return false;
  }
  return *msg_ == *py::cast<const @(msg)Handle &>(other).get();
}

void @(msg)Handle::reset(py::handle init)
{
  msg_->_reset(to_init(init));
}

void @(msg)Handle::clear()
{
  msg_->_reset(rosidl_runtime_cpp::MessageInitialization::ZERO);
}

std::string @(msg)Handle::repr()
{
  return std::string("@(msg)(") +
@{
repr_members = [m for m in message.structure.members
                if m.name != EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME]
}@
@[for i, member in enumerate(repr_members)]@
    "@(member.name)=" + @(member_repr_code(member, '(*msg_)')) +@[if i < len(repr_members) - 1]@ ", " +@[end if]@
@[end for]@
    ")";
}

@[if has_bridge]@
// Per-message-type bridge to the C++ typesupport (ADR: MessageTypeBridge).
// wrap_cpp_message is ALWAYS non-owning (loans: the middleware owns the
// storage); unwrap_cpp_message returns a raw pointer (no ownership transfer).
// All functions set CPython error flags and return nullptr on failure.
// Positional initialization in MessageTypeBridge declaration order
// (get_cpp_typesupport, wrap_cpp_message, unwrap_cpp_message,
// wrap_cpp_constraints, unwrap_cpp_constraints): designated initializers
// need C++20 but the generated code targets C++17.
const MessageTypeBridge @(msg)Handle::cpython_bridge = {
  // Generic C++ typesupport dispatch — never specialized to one
  // implementation; the middleware resolves the concrete one.
  []() -> const rosidl_message_type_support_t * {
    return rosidl_typesupport_cpp::get_message_type_support_handle<@(msg_cpp)>();
  },
  // Non-owning wrap: used on borrow/take-loaned paths.
  [](void * msg_ptr) -> PyObject * {
    auto handle = std::make_shared<@(msg)Handle>(
      static_cast<@(msg_cpp) *>(msg_ptr));
    return py::cast(handle).release().ptr();
  },
  // Raw pointer extraction: no ownership transfer.
  [](PyObject * py) -> void * {
    return rosidl_runtime_cpython::unwrap_message_handle(py);
  },
  // Copy the C++ Constraints value into a Python-owned object.
  [](void * cs) -> PyObject * {
    auto * cpp_cs = static_cast<@(msg_cpp)::Constraints *>(cs);
    return py::cast(*cpp_cs).release().ptr();
  },
  // Raw C++ Constraints pointer: no ownership transfer.
  [](PyObject * py) -> void * {
    py::handle h = py::reinterpret_borrow<py::object>(py);
    return const_cast<@(msg_cpp)::Constraints *>(
      py::cast<const @(msg_cpp)::Constraints *>(h));
  },
};

// Address of cpython_bridge as an integer; exposed to Python as the
// read-only class attribute __cpython_bridge__.
const uintptr_t @(msg)Handle::cpython_bridge_address =
  reinterpret_cast<uintptr_t>(&@(msg)Handle::cpython_bridge);
@[end if]@

void register_@(msg_underscore)(py::module_ & m)
{
@[if msg in element_messages]@
  register_@(msg_underscore)_containers(m);
@[end if]@
  py::class_<@(msg)Handle, MessageHandleInterface, std::shared_ptr<@(msg)Handle>> cls(m, "@(msg)");
@[for constant in message.constants]@
  cls.def_property_readonly_static("@(constant.name)",
    [](py::object) { return @(constant_to_cpp(constant)); });
@[end for]@
@[if has_bridge]@
  // Read-only class attribute exposing the MessageTypeBridge address (a
  // static const uintptr_t on the handle class).  rclpy resolves the bridge
  // from the class (or an instance) via this attribute; instances additionally
  // expose it through MessageHandleInterface::type_bridge().
  cls.def_readonly_static("__cpython_bridge__", &@(msg)Handle::cpython_bridge_address);
@[end if]@
  register_@(msg_underscore)_constraints(cls);
  cls
    .def(py::init(&@(msg)Handle::create))
@[for member in message.structure.members]@
@[  if member.name != EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME]@
    .def_property("@(member.name)", &@(msg)Handle::@(member.name), &@(msg)Handle::set_@(member.name))
@[  end if]@
@[end for]@
    .def("as_builtin", &@(msg)Handle::as_builtin)
    .def("from_builtin", &@(msg)Handle::from_builtin, py::arg("value"))
    .def("__eq__", &@(msg)Handle::eq)
    .def("_reset", &@(msg)Handle::reset, py::arg("_init"))
    .def("clear", &@(msg)Handle::clear)
    .def("__repr__", &@(msg)Handle::repr);
}

// Per-message Constraints class (per-member constraint fields).
void register_@(msg_underscore)_constraints(
  py::class_<@(msg)Handle, MessageHandleInterface, std::shared_ptr<@(msg)Handle>> & cls)
{
  py::class_<@(msg_cpp)::Constraints>(cls, "Constraints")
    .def(py::init<>())
@[for member in message.structure.members]@
@[  if member.name != EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME]@
@[    if member_constraint_type(member)]@
    .def_readwrite("@(member.name)", &@(msg_cpp)::Constraints::@(member.name))
@[    end if]@
@[  end if]@
@[end for]@
    .def("__eq__", [](const @(msg_cpp)::Constraints & self, py::handle other) {
      if (!py::isinstance<@(msg_cpp)::Constraints>(other)) {
        return false;
      }
      return self == py::cast<const @(msg_cpp)::Constraints &>(other);
    })
    .def("__repr__", [](const @(msg_cpp)::Constraints &) {
      return std::string("@(msg).Constraints()");
    });
// Sequence constraint for sequences/arrays of this message type. Registered
  // unconditionally: a message used as a sequence element only in another
  // package still needs its SequenceConstraint<Msg> bound here.
  py::class_<rosidl_runtime_cpp::SequenceConstraint<@(msg_cpp)>>(
    cls, "@(msg)SequenceConstraint")
    .def(py::init<>())
    .def_readonly("size", &rosidl_runtime_cpp::SequenceConstraint<@(msg_cpp)>::size)
    .def_readonly("element", &rosidl_runtime_cpp::SequenceConstraint<@(msg_cpp)>::element)
    .def("__eq__", [](const rosidl_runtime_cpp::SequenceConstraint<@(msg_cpp)> & self,
      py::handle other) {
      if (!py::isinstance<rosidl_runtime_cpp::SequenceConstraint<@(msg_cpp)>>(other)) {
        return false;
      }
      return self == py::cast<const rosidl_runtime_cpp::SequenceConstraint<@(msg_cpp)> &>(other);
    })
    .def("__repr__", [](const rosidl_runtime_cpp::SequenceConstraint<@(msg_cpp)> & self) {
      return "@(msg)SequenceConstraint(size=" + std::to_string(self.size) + ")";
    });
}

@[if msg in element_messages]@
// Message-element container wrappers (sequences/arrays of this message
// type). Registered so getters returning SequenceWrapper<Msg> /
// ArrayWrapper<Msg> resolve; pybind11 shares the types across modules.
void register_@(msg_underscore)_containers(py::module_ & m)
{
  py::class_<SequenceIterator<@(msg_cpp)>>(m, "@(msg)SequenceIterator")
    .def("__iter__", &SequenceIterator<@(msg_cpp)>::iter)
    .def("__next__", &SequenceIterator<@(msg_cpp)>::next);

  py::class_<SequenceWrapper<@(msg_cpp)>, std::shared_ptr<SequenceWrapper<@(msg_cpp)>>>(
    m, "@(msg)SequenceWrapper")
    .def("__len__", &SequenceWrapper<@(msg_cpp)>::len)
    .def("__getitem__", &SequenceWrapper<@(msg_cpp)>::getitem)
    .def("__setitem__", &SequenceWrapper<@(msg_cpp)>::setitem)
    .def("__delitem__", &SequenceWrapper<@(msg_cpp)>::delitem)
    .def("__iter__", &SequenceWrapper<@(msg_cpp)>::iter)
    .def("__reversed__", &SequenceWrapper<@(msg_cpp)>::reversed)
    .def("__contains__", &SequenceWrapper<@(msg_cpp)>::contains)
    .def("__eq__", &SequenceWrapper<@(msg_cpp)>::eq)
    .def("__ne__", &SequenceWrapper<@(msg_cpp)>::ne)
    .def("__add__", &SequenceWrapper<@(msg_cpp)>::add)
    .def("__radd__", &SequenceWrapper<@(msg_cpp)>::radd)
    .def("__iadd__", &SequenceWrapper<@(msg_cpp)>::iadd)
    .def("__mul__", &SequenceWrapper<@(msg_cpp)>::mul)
    .def("__rmul__", &SequenceWrapper<@(msg_cpp)>::rmul)
    .def("__imul__", &SequenceWrapper<@(msg_cpp)>::imul)
    .def("index", &SequenceWrapper<@(msg_cpp)>::index, py::arg("value"))
    .def("count", &SequenceWrapper<@(msg_cpp)>::count, py::arg("value"))
    .def("reverse", &SequenceWrapper<@(msg_cpp)>::reverse)
    .def("copy", &SequenceWrapper<@(msg_cpp)>::copy)
    .def("append", &SequenceWrapper<@(msg_cpp)>::append, py::arg("value"))
    .def("extend", &SequenceWrapper<@(msg_cpp)>::extend, py::arg("values"))
    .def("insert", &SequenceWrapper<@(msg_cpp)>::insert, py::arg("index"), py::arg("value"))
    .def("pop", &SequenceWrapper<@(msg_cpp)>::pop, py::arg("index") = py::int_(-1))
    .def("remove", &SequenceWrapper<@(msg_cpp)>::remove, py::arg("value"))
    .def("clear", &SequenceWrapper<@(msg_cpp)>::clear)
    .def("resize", &SequenceWrapper<@(msg_cpp)>::resize, py::arg("new_size"))
    .def("assign", &SequenceWrapper<@(msg_cpp)>::assign, py::arg("values"))
    .def("as_builtin", &SequenceWrapper<@(msg_cpp)>::as_builtin)
    .def("from_builtin", &SequenceWrapper<@(msg_cpp)>::from_builtin, py::arg("value"))
    .def("max_size", &SequenceWrapper<@(msg_cpp)>::max_size)
    .def("is_bounded", &SequenceWrapper<@(msg_cpp)>::is_bounded)
    .def("__repr__", &SequenceWrapper<@(msg_cpp)>::repr);

  py::class_<ArrayIterator<@(msg_cpp)>>(m, "@(msg)ArrayIterator")
    .def("__iter__", &ArrayIterator<@(msg_cpp)>::iter)
    .def("__next__", &ArrayIterator<@(msg_cpp)>::next);

  py::class_<ArrayWrapper<@(msg_cpp)>, std::shared_ptr<ArrayWrapper<@(msg_cpp)>>>(
    m, "@(msg)ArrayWrapper")
    .def("size", &ArrayWrapper<@(msg_cpp)>::size)
    .def("__len__", &ArrayWrapper<@(msg_cpp)>::len)
    .def("__getitem__", &ArrayWrapper<@(msg_cpp)>::getitem)
    .def("__setitem__", &ArrayWrapper<@(msg_cpp)>::setitem)
    .def("__iter__", &ArrayWrapper<@(msg_cpp)>::iter)
    .def("__reversed__", &ArrayWrapper<@(msg_cpp)>::reversed)
    .def("__contains__", &ArrayWrapper<@(msg_cpp)>::contains)
    .def("__eq__", &ArrayWrapper<@(msg_cpp)>::eq)
    .def("__ne__", &ArrayWrapper<@(msg_cpp)>::ne)
    .def("index", &ArrayWrapper<@(msg_cpp)>::index, py::arg("value"))
    .def("count", &ArrayWrapper<@(msg_cpp)>::count, py::arg("value"))
    .def("reverse", &ArrayWrapper<@(msg_cpp)>::reverse)
    .def("fill", &ArrayWrapper<@(msg_cpp)>::fill, py::arg("value"))
    .def("assign", &ArrayWrapper<@(msg_cpp)>::assign, py::arg("value"))
    .def("as_builtin", &ArrayWrapper<@(msg_cpp)>::as_builtin)
    .def("from_builtin", &ArrayWrapper<@(msg_cpp)>::from_builtin, py::arg("value"))
    .def("__repr__", &ArrayWrapper<@(msg_cpp)>::repr);
}

@[end if]@
@[else]@
void register_@(msg_underscore)(py::module_ &)
{
}

@[end if]@
@[end for]@
}  // namespace rosidl_runtime_cpython