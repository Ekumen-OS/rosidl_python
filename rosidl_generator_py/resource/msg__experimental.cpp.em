@# generated from rosidl_generator_py/resource/msg__experimental.cpp.em
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

# All messages to bind: top-level messages plus service request/response and
# action goal/result/feedback constituent messages. Each carries the header
# that defines its struct (per-message for msg, the service/action header for
# srv/action constituents).
bind_messages = []  # list of (message, include)
for message in content.get_elements_of_type(Message):
    msg_underscore = convert_camel_case_to_lower_case_underscore(
        message.structure.namespaced_type.name)
    bind_messages.append(
        (message, '{pkg}/msg/experimental/{stem}.hpp'.format(
            pkg=pkg, stem=msg_underscore)))
for service in content.get_elements_of_type(Service):
    srv_underscore = convert_camel_case_to_lower_case_underscore(
        service.namespaced_type.name)
    srv_include = '{pkg}/srv/experimental/{stem}.hpp'.format(
        pkg=pkg, stem=srv_underscore)
    bind_messages.append((service.request_message, srv_include))
    bind_messages.append((service.response_message, srv_include))
for action in content.get_elements_of_type(Action):
    act_underscore = convert_camel_case_to_lower_case_underscore(
        action.namespaced_type.name)
    act_include = '{pkg}/action/experimental/{stem}.hpp'.format(
        pkg=pkg, stem=act_underscore)
    bind_messages.append((action.goal, act_include))
    bind_messages.append((action.result, act_include))
    bind_messages.append((action.feedback, act_include))

# Collect nested message types (for cross-package binding includes) and
# bounded string element types (need ElementTraits per bound).
nested_types = []
bounded_string_elems = set()
msg_includes = set()
for message, msg_include in bind_messages:
    msg_includes.add(msg_include)
    for member in message.structure.members:
        t = member.type
        if isinstance(t, AbstractNestedType):
            t = t.value_type
        if isinstance(t, NamespacedType):
            nested_types.append(t)
        if isinstance(t, AbstractGenericString) and t.has_maximum_size():
            bounded_string_elems.add((t.maximum_size, isinstance(t, AbstractWString)))
}@
#ifndef @(pkg.upper())__@(str(interface_path.parent).upper())__EXPERIMENTAL__DETAIL__@(interface_stem.upper())__CPYTHON_BINDING_HPP_
#define @(pkg.upper())__@(str(interface_path.parent).upper())__EXPERIMENTAL__DETAIL__@(interface_stem.upper())__CPYTHON_BINDING_HPP_

@[for msg_include in sorted(msg_includes)]@
#include "@(msg_include)"
@[end for]@
@[for t in nested_types]@
#include "@('/'.join(t.namespaces))/experimental/detail/@(convert_camel_case_to_lower_case_underscore(t.name))__cpython_binding.hpp"
@[end for]@

#include "@(pkg)/msg/rosidl_generator_py__visibility_control.hpp"

#include "rosidl_runtime_cpython/message_handle.hpp"
#include "rosidl_runtime_cpython/scalar.hpp"
#include "rosidl_runtime_cpython/string.hpp"
#include "rosidl_runtime_cpython/sequence.hpp"
#include "rosidl_runtime_cpython/array.hpp"

namespace rosidl_runtime_cpython
{

@[for message, msg_include in bind_messages]@
@{
msg = message.structure.namespaced_type.name
msg_underscore = convert_camel_case_to_lower_case_underscore(msg)
msg_cpp = '::'.join(
    list(message.structure.namespaced_type.namespaces) + ['experimental', msg])
}@
@[if message.structure.namespaced_type.name in supported_messages]@
// Default visibility: methods referenced cross-package (as_builtin_dict,
// from_py, register_*) must be exported from the bindings library.
class ROSIDL_GENERATOR_PY_PUBLIC_@(pkg) @(msg)Handle : public MessageHandleBase<@(msg_cpp)>
{
public:
  using Base = MessageHandleBase<@(msg_cpp)>;
  using Base::Base;

  // Keyword constructor: _init policy + field overrides.
  static std::shared_ptr<@(msg)Handle> create(py::kwargs kwargs);

  // Build a @(msg) from a Python object: a @(msg)Handle (copy) or a dict.
  static @(msg_cpp) from_py(py::handle h);

@[for member in message.structure.members]@
@[  if member.name != EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME]@
  @(member_getter_decl(member))

  @(member_setter_decl(member))

@[  end if]@
@[end for]@
  py::object as_builtin();

  static py::object as_builtin_dict(const @(msg_cpp) & m);

  void from_builtin(py::handle h);

  bool eq(py::handle other) const;

  void reset(py::handle init);

  void clear();

  std::string repr();
};

ROSIDL_GENERATOR_PY_PUBLIC_@(pkg) void register_@(msg_underscore)(py::module_ & m);
@[if msg in element_messages]@

// Message-element container wrappers (sequences/arrays of this message type).
ROSIDL_GENERATOR_PY_PUBLIC_@(pkg) void register_@(msg_underscore)_containers(py::module_ & m);
@[end if]@

// Per-message Constraints class.
ROSIDL_GENERATOR_PY_PUBLIC_@(pkg) void register_@(msg_underscore)_constraints(
  py::class_<@(msg)Handle, std::shared_ptr<@(msg)Handle>> & cls);

// Element conversion for message-element containers (sequences/arrays of
// this message type): to_py produces a parent-anchored handle view;
// to_builtin a dict; from_py copies out of a handle or dict.
template<>
struct ElementTraits<@(msg_cpp)>
{
  static py::object to_py(@(msg_cpp) & v, py::object parent)
  {
    return py::cast(std::make_shared<@(msg)Handle>(&v, std::move(parent)));
  }

  static py::object to_builtin(const @(msg_cpp) & v)
  {
    return @(msg)Handle::as_builtin_dict(v);
  }

  static @(msg_cpp) from_py(py::handle h)
  {
    return @(msg)Handle::from_py(h);
  }
};

@[else]@
// Unsupported message shape; binding deferred until the generator handles it.
ROSIDL_GENERATOR_PY_PUBLIC_@(pkg) void register_@(msg_underscore)(py::module_ &);
@[end if]@

@[end for]@
@[for bound, is_wstring in sorted(bounded_string_elems)]@
// Element conversion for bounded string elements in sequences/arrays.
template<>
struct ElementTraits<rosidl_runtime_cpp::Bounded@('W' if is_wstring else '')String<@(bound)>>
{
  static py::object to_py(rosidl_runtime_cpp::Bounded@('W' if is_wstring else '')String<@(bound)> & v, py::object parent)
  {
    return py::cast(std::make_shared<StringWrapper<@('char16_t' if is_wstring else 'char')>>(
      std::unique_ptr<StringInterface<@('char16_t' if is_wstring else 'char')>>(
        new StringReference<@('char16_t' if is_wstring else 'char'), @(bound)>(&v)),
      std::move(parent)));
  }

  static py::object to_builtin(const rosidl_runtime_cpp::Bounded@('W' if is_wstring else '')String<@(bound)> & v)
  {
@[if is_wstring]@
    std::string bytes(
      reinterpret_cast<const char *>(v.data()), v.size() * sizeof(char16_t));
    return py::bytes(bytes).attr("decode")("utf-16-le");
@[else]@
    return py::str(std::string(v.data(), v.size()));
@[end if]@
  }

  static rosidl_runtime_cpp::Bounded@('W' if is_wstring else '')String<@(bound)> from_py(py::handle h)
  {
    rosidl_runtime_cpp::Bounded@('W' if is_wstring else '')String<@(bound)> s;
    s.assign(py::cast<@('std::u16string' if is_wstring else 'std::string')>(h));
    return s;
  }
};

@[end for]@
}  // namespace rosidl_runtime_cpython

#endif  // @(pkg.upper())__@(str(interface_path.parent).upper())__EXPERIMENTAL__DETAIL__@(interface_stem.upper())__CPYTHON_BINDING_HPP_