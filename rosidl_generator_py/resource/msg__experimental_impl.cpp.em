@# generated from rosidl_generator_py/resource/msg__experimental_impl.cpp.em
@# generated code does not contain a copyright notice
@{
from rosidl_pycommon import convert_camel_case_to_lower_case_underscore
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

void register_@(msg_underscore)(py::module_ & m)
{
@[if msg in element_messages]@
  register_@(msg_underscore)_containers(m);
@[end if]@
  py::class_<@(msg)Handle, std::shared_ptr<@(msg)Handle>>(m, "@(msg)")
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
    .def("__iter__", &SequenceWrapper<@(msg_cpp)>::iter)
    .def("__reversed__", &SequenceWrapper<@(msg_cpp)>::reversed)
    .def("__contains__", &SequenceWrapper<@(msg_cpp)>::contains)
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