// generated from rosidl_generator_py/resource/idl__region.cpp.em
// with input from @(package_name):@(interface_path)
// generated code does not contain a copyright notice
@{
from rosidl_generator_cpp import MSG_TYPE_TO_CPP
from rosidl_parser.definition import AbstractSequence, AbstractString, AbstractWString
from rosidl_parser.definition import Array, BasicType, Message, NamespacedType
from rosidl_pycommon import convert_camel_case_to_lower_case_underscore

messages = content.get_elements_of_type(Message)
stem = convert_camel_case_to_lower_case_underscore(interface_path.stem)
module_name = '_' + stem + '_region'
parts = [package_name] + list(interface_path.parent.parts)
region_header = '/'.join(parts + ['views', stem + '__region.hpp'])

def view_type(t):
    return '::' + '::'.join(t.namespaces + ['views', 'Region' + t.name])

def handle_type(t, view_type=view_type):
    return 'rosidl_runtime_cpython::MessageView<{}>'.format(view_type(t))

def element_get(t, BasicType=BasicType, AbstractWString=AbstractWString,
                AbstractString=AbstractString, NamespacedType=NamespacedType,
                handle_type=handle_type):
    if isinstance(t, BasicType):
        return 'return py::cast(item);'
    if isinstance(t, AbstractWString):
        return 'return py::cast(item.to_string());'
    if isinstance(t, AbstractString):
        return 'return py::str(std::string(item.data(), item.size()));'
    if isinstance(t, NamespacedType):
        return ('return py::cast(std::make_shared<{}>'
                '(&item, item_state, read_only, owner));').format(handle_type(t))
    raise ValueError('unsupported Python region element: {}'.format(t))

def element_set(t, BasicType=BasicType, AbstractWString=AbstractWString,
                AbstractString=AbstractString, NamespacedType=NamespacedType,
                MSG_TYPE_TO_CPP=MSG_TYPE_TO_CPP, handle_type=handle_type):
    if isinstance(t, BasicType):
        return 'item = py::cast<{}>(value);'.format(MSG_TYPE_TO_CPP[t.typename])
    if isinstance(t, AbstractWString):
        return 'item.assign(py::cast<std::u16string>(value));'
    if isinstance(t, AbstractString):
        return 'item.assign(py::cast<std::string>(value));'
    if isinstance(t, NamespacedType):
        return ('auto child = py::cast(std::make_shared<{}>'
                '(&item, state->elements_state(container), false, owner)); '
                'rosidl_runtime_cpython::assign_fields(child, value);').format(handle_type(t))
    raise ValueError('unsupported Python region element: {}'.format(t))

nested_types = set()
for message in messages:
    for member in message.structure.members:
        t = member.type.value_type if isinstance(member.type, (Array, AbstractSequence)) else member.type
        if isinstance(t, NamespacedType):
            nested_types.add((tuple(t.namespaces), t.name))
}@
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <string>
#include <utility>

#include "@(region_header)"
#include "rosidl_runtime_cpython/bridge.hpp"
#include "rosidl_runtime_cpython/message_view.hpp"
#include "rosidl_typesupport_cpp/message_type_support.hpp"

namespace py = pybind11;
using rosidl_runtime_cpython::BorrowedViewState;

@[for message in messages]@
@{
t = message.structure.namespaced_type
name = t.name
alias = 'Handle' + name
view = view_type(t)
}@
using @(alias) = @(handle_type(t));
using Constraints@(name) = @(view)::Constraints;

static const auto bridge@(name) = rosidl_runtime_cpython::make_message_type_bridge<@(view)>(
  []() {return rosidl_typesupport_cpp::get_message_type_support_handle<@(view)>();},
  [](void * message, std::shared_ptr<BorrowedViewState> state, bool read_only) -> PyObject * {
    return py::cast(std::make_shared<@(alias)>(
      static_cast<@(view) *>(message), std::move(state), read_only)).release().ptr();
  },
  [](PyObject * object) -> const void * {
    if (!py::isinstance<Constraints@(name)>(py::handle(object))) {
      throw py::type_error("Expected @(name) constraints");
    }
    return py::cast<const Constraints@(name) *>(py::handle(object));
  },
  []() {return rosidl_typesupport_cpp::get_message_type_support_handle<@(view)::PlainMessage>();});

@[end for]@
PYBIND11_MODULE(@(module_name), module)
{
  rosidl_runtime_cpython::bind_sequence_view(module);
@[for namespaces, name in sorted(nested_types)]@
@[if list(namespaces) != parts or convert_camel_case_to_lower_case_underscore(name) != stem]@
  py::module_::import("@('.'.join(namespaces)).views._@(convert_camel_case_to_lower_case_underscore(name))_region");
@[end if]@
@[end for]@
@[for message in messages]@
@{
t = message.structure.namespaced_type
name = t.name
alias = 'Handle' + name
view = view_type(t)
}@
  auto constraints@(name) = py::class_<Constraints@(name)>(module, "@(name)Constraints")
    .def(py::init<>());
@[for member in message.structure.members]@
@{
field_type = member.type
field_name = member.name
}@
@[if isinstance(field_type, (AbstractString, AbstractWString)) or (isinstance(field_type, (Array, AbstractSequence)) and isinstance(field_type.value_type, (AbstractString, AbstractWString)))]@
  constraints@(name).def_property(
    "@(field_name)_size",
    [](const Constraints@(name) & constraints) {return constraints.@(field_name)@('.element.size' if isinstance(field_type, AbstractSequence) else '.size');},
    [](Constraints@(name) & constraints, std::size_t value) {constraints.@(field_name)@('.element.size' if isinstance(field_type, AbstractSequence) else '.size') = value;});
@[end if]@
@[if isinstance(field_type, AbstractSequence)]@
  constraints@(name).def_property(
    "@(field_name)_length",
    [](const Constraints@(name) & constraints) {return constraints.@(field_name).size;},
    [](Constraints@(name) & constraints, std::size_t value) {constraints.@(field_name).size = value;});
@[end if]@
@[if isinstance(field_type, NamespacedType)]@
  constraints@(name).def_readwrite("@(field_name)", &Constraints@(name)::@(field_name));
@[elif isinstance(field_type, AbstractSequence) and isinstance(field_type.value_type, NamespacedType)]@
  constraints@(name).def_property(
    "@(field_name)_element",
    [](Constraints@(name) & constraints) -> auto & {return constraints.@(field_name).element;},
    [](Constraints@(name) & constraints,
    const decltype(std::declval<Constraints@(name) &>().@(field_name).element) & value) {
      constraints.@(field_name).element = value;
    }, py::return_value_policy::reference_internal);
@[elif isinstance(field_type, Array) and isinstance(field_type.value_type, NamespacedType)]@
  constraints@(name).def_readwrite("@(field_name)_element", &Constraints@(name)::@(field_name));
@[end if]@
@[end for]@
  auto cls@(name) = py::class_<@(alias), std::shared_ptr<@(alias)>>(module, "@(name)");
  cls@(name).def(py::init([]() {
      auto owner = std::make_shared<@(view)>();
      return std::make_shared<@(alias)>(
        owner.get(), std::make_shared<BorrowedViewState>(), false, owner);
    }));
  cls@(name).attr("__region_type_bridge__") =
    py::capsule(&bridge@(name), rosidl_runtime_cpython::kMessageTypeBridgeCapsule);
  cls@(name).attr("Constraints") = constraints@(name);
@[for member in message.structure.members]@
@{
field_type = member.type
field_name = member.name
}@
@[if isinstance(field_type, BasicType)]@
  cls@(name).def_property(
    "@(field_name)",
    [](const @(alias) & self) {return self.get().@(field_name)();},
    [](const @(alias) & self, @(MSG_TYPE_TO_CPP[field_type.typename]) value) {
      self.require_mutable();
      self.get().@(field_name)() = value;
    });
@[elif isinstance(field_type, AbstractWString)]@
  cls@(name).def_property(
    "@(field_name)",
    [](const @(alias) & self) {return self.get().@(field_name)().to_string();},
    [](const @(alias) & self, const std::u16string & value) {
      self.require_mutable();
      self.get().@(field_name)().assign(value);
    });
@[elif isinstance(field_type, AbstractString)]@
  cls@(name).def_property(
    "@(field_name)",
    [](const @(alias) & self) {
      auto & string = self.get().@(field_name)();
      return std::string(string.data(), string.size());
    },
    [](const @(alias) & self, const std::string & value) {
      self.require_mutable();
      self.get().@(field_name)().assign(value);
    });
@[elif isinstance(field_type, NamespacedType)]@
  cls@(name).def_property(
    "@(field_name)", [](const @(alias) & self) {
      return std::make_shared<@(handle_type(field_type))>(
        &self.get().@(field_name)(), self.state(), self.read_only(), self.owner());
    }, [](const @(alias) & self, py::handle values) {
      self.require_mutable();
      auto child = py::cast(std::make_shared<@(handle_type(field_type))>(
        &self.get().@(field_name)(), self.state(), false, self.owner()));
      rosidl_runtime_cpython::assign_fields(child, values);
    });
@[elif isinstance(field_type, (Array, AbstractSequence))]@
  auto get_@(name)_@(field_name) = [](const @(alias) & self) {
      auto & container = self.get().@(field_name)();
      auto state = self.state();
      auto owner = self.owner();
      return rosidl_runtime_cpython::make_sequence_view<@('true' if isinstance(field_type, AbstractSequence) else 'false')>(
        container, state, self.read_only(), owner,
        [](auto & item, const std::shared_ptr<BorrowedViewState> & item_state,
        bool read_only, const std::shared_ptr<void> & owner) -> py::object {
          (void)item_state; (void)read_only; (void)owner;
          @(element_get(field_type.value_type))
        },
        [state, owner, container = &container](auto & item, py::handle value) {
          (void)state; (void)owner; (void)container;
          @(element_set(field_type.value_type))
        });
    };
  cls@(name).def_property(
    "@(field_name)", get_@(name)_@(field_name),
    [get_@(name)_@(field_name)](const @(alias) & self, py::iterable values) {
      get_@(name)_@(field_name)(self).assign(values@(', true' if isinstance(field_type.value_type, NamespacedType) else ''));
    });
@[else]@
@{raise ValueError('unsupported Python region field: {}'.format(field_type))}@
@[end if]@
@[end for]@
@[end for]@
}
