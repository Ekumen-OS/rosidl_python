# Copyright 2014-2018 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from ast import literal_eval
import keyword
import os
import pathlib
import sys

from rosidl_parser.definition import AbstractGenericString
from rosidl_parser.definition import AbstractNestedType
from rosidl_parser.definition import AbstractSequence
from rosidl_parser.definition import AbstractString
from rosidl_parser.definition import AbstractWString
from rosidl_parser.definition import Action
from rosidl_parser.definition import Array
from rosidl_parser.definition import BasicType
from rosidl_parser.definition import BoundedSequence
from rosidl_parser.definition import CHARACTER_TYPES
from rosidl_parser.definition import EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME
from rosidl_parser.definition import FLOATING_POINT_TYPES
from rosidl_parser.definition import IdlContent
from rosidl_parser.definition import IdlLocator
from rosidl_parser.definition import INTEGER_TYPES
from rosidl_parser.definition import Message
from rosidl_parser.definition import NamespacedType
from rosidl_parser.definition import Service
from rosidl_parser.parser import parse_idl_file
from rosidl_pycommon import convert_camel_case_to_lower_case_underscore
from rosidl_pycommon import expand_template
from rosidl_pycommon import generate_files
from rosidl_pycommon import get_newest_modification_time
from rosidl_pycommon import read_generator_arguments

SPECIAL_NESTED_BASIC_TYPES = {
    'float': {'dtype': 'numpy.float32', 'type_code': 'f'},
    'double': {'dtype': 'numpy.float64', 'type_code': 'd'},
    'int8': {'dtype': 'numpy.int8', 'type_code': 'b'},
    'uint8': {'dtype': 'numpy.uint8', 'type_code': 'B'},
    'int16': {'dtype': 'numpy.int16', 'type_code': 'h'},
    'uint16': {'dtype': 'numpy.uint16', 'type_code': 'H'},
    'int32': {'dtype': 'numpy.int32', 'type_code': 'i'},
    'uint32': {'dtype': 'numpy.uint32', 'type_code': 'I'},
    'int64': {'dtype': 'numpy.int64', 'type_code': 'q'},
    'uint64': {'dtype': 'numpy.uint64', 'type_code': 'Q'},
}

# ---------------------------------------------------------------------------
# Experimental pybind11 binding generation (replaces the legacy Python
# experimental message classes). The generated bindings follow the Phase 1B/3
# patterns: MessageHandleBase-derived handles, assignable properties
# (msg.field / msg.field = value), recursive as_builtin/from_builtin,
# equality, repr, _reset/clear, and keyword constructors.
# ---------------------------------------------------------------------------

# IDL builtin type -> C++ primitive (matches rosidl_generator_cpp).
BASIC_TYPE_TO_CPP = {
    'boolean': 'bool',
    'octet': 'uint8_t',
    'char': 'uint8_t',
    'wchar': 'char16_t',
    'float': 'float',
    'double': 'double',
    'long double': 'long double',
    'uint8': 'uint8_t',
    'int8': 'int8_t',
    'uint16': 'uint16_t',
    'int16': 'int16_t',
    'uint32': 'uint32_t',
    'int32': 'int32_t',
    'uint64': 'uint64_t',
    'int64': 'int64_t',
}


def experimental_namespaced_type_name(type_):
    """Return the C++ experimental qualified name for a NamespacedType."""
    return '::'.join(list(type_.namespaces) + ['experimental', type_.name])


def _member_kind(type_):
    """Classify a member type: scalar/string/wstring/sequence/array/nested."""
    if isinstance(type_, AbstractNestedType):
        value_type = type_.value_type
        if isinstance(type_, Array):
            return ('array', value_type)
        if isinstance(type_, AbstractSequence):
            return ('sequence', value_type)
        return ('unsupported', type_)
    if isinstance(type_, BasicType):
        return ('scalar', type_)
    if isinstance(type_, AbstractGenericString):
        return ('string', type_)
    if isinstance(type_, NamespacedType):
        return ('nested', type_)
    return ('unsupported', type_)


def _handle_name(type_):
    """The generated handle class name for a nested message type."""
    return type_.name + 'Handle'


def _element_cpp(type_):
    """C++ element type for sequence/array members (None if unsupported)."""
    if isinstance(type_, BasicType):
        return BASIC_TYPE_TO_CPP[type_.typename]
    if isinstance(type_, NamespacedType):
        return experimental_namespaced_type_name(type_)
    if isinstance(type_, AbstractWString):
        if type_.has_maximum_size():
            return 'rosidl_runtime_cpp::BoundedWString<{}>'.format(type_.maximum_size)
        return 'rosidl_runtime_cpp::WString'
    if isinstance(type_, AbstractGenericString):
        if type_.has_maximum_size():
            return 'rosidl_runtime_cpp::BoundedString<{}>'.format(type_.maximum_size)
        return 'rosidl_runtime_cpp::String'
    return None


def _string_char(type_):
    """CharT for a string member (char or char16_t)."""
    return 'char16_t' if isinstance(type_, AbstractWString) else 'char'


def _string_cpp_cast(type_):
    """The py::cast type for a string member's value."""
    return 'std::u16string' if _string_char(type_) == 'char16_t' else 'std::string'


def member_getter_decl(member):
    """C++ getter declaration (signature only) for a message member."""
    code = member_getter_code(member)
    return code.split('\n')[0] + ';'


def member_setter_decl(member):
    """C++ setter declaration (signature only) for a message member."""
    code = member_setter_code(member)
    return code.split('\n')[0] + ';'


def member_getter_impl(member, class_name):
    """C++ getter definition qualified with the handle class name."""
    code = member_getter_code(member)
    return code.replace(
        ' {name}('.format(name=member.name),
        ' {cls}::{name}('.format(cls=class_name, name=member.name), 1)


def member_setter_impl(member, class_name):
    """C++ setter definition qualified with the handle class name."""
    code = member_setter_code(member)
    return code.replace(
        ' set_{name}('.format(name=member.name),
        ' {cls}::set_{name}('.format(cls=class_name, name=member.name), 1)


def _element_from_py(type_):
    """C++ expression converting a Python element to the C++ element type."""
    if isinstance(type_, BasicType):
        return 'ElementTraits<{}>::from_py(item)'.format(BASIC_TYPE_TO_CPP[type_.typename])
    if isinstance(type_, NamespacedType):
        return '{}::from_py(item)'.format(_handle_name(type_))
    if isinstance(type_, AbstractGenericString):
        return 'ElementTraits<{}>::from_py(item)'.format(_element_cpp(type_))
    assert False, type_


def _element_to_builtin(type_, var):
    """C++ expression converting a C++ element to a Python builtin."""
    if isinstance(type_, BasicType):
        return 'py::cast({var})'.format(var=var)
    if isinstance(type_, NamespacedType):
        return '{}::as_builtin_dict({var})'.format(_handle_name(type_), var=var)
    if isinstance(type_, AbstractWString):
        return (
            'py::bytes(std::string(reinterpret_cast<const char *>({var}.data()), '
            '{var}.size() * sizeof(char16_t))).attr("decode")("utf-16-le")'
        ).format(var=var)
    if isinstance(type_, AbstractGenericString):
        return 'py::str(std::string({var}.data(), {var}.size()))'.format(var=var)
    assert False, type_


def member_getter_code(member):
    """C++ getter method for a message member (returns a parent-anchored wrapper)."""
    name = member.name
    kind, type_ = _member_kind(member.type)
    if kind == 'scalar':
        t = BASIC_TYPE_TO_CPP[type_.typename]
        return (
            '  std::shared_ptr<ScalarWrapper<{t}>> {name}()\n'
            '  {{\n'
            '    return std::make_shared<ScalarWrapper<{t}>>(&msg_->{name}, py::cast(this));\n'
            '  }}').format(t=t, name=name)
    if kind == 'string':
        c = _string_char(type_)
        bound = type_.maximum_size if type_.has_maximum_size() else 0
        return (
            '  std::shared_ptr<StringWrapper<{c}>> {name}()\n'
            '  {{\n'
            '    return std::make_shared<StringWrapper<{c}>>(\n'
            '      std::unique_ptr<StringInterface<{c}>>(new StringReference<{c}, {bound}>(&msg_->{name})),\n'
            '      py::cast(this));\n'
            '  }}').format(c=c, name=name, bound=bound)
    if kind == 'sequence':
        t = _element_cpp(type_)
        bound = member.type.maximum_size if isinstance(member.type, BoundedSequence) else 0
        return (
            '  std::shared_ptr<SequenceWrapper<{t}>> {name}()\n'
            '  {{\n'
            '    return std::make_shared<SequenceWrapper<{t}>>(\n'
            '      std::unique_ptr<SequenceInterface<{t}>>(new SequenceReference<{t}, {bound}>(&msg_->{name})),\n'
            '      py::cast(this));\n'
            '  }}').format(t=t, name=name, bound=bound)
    if kind == 'array':
        t = _element_cpp(type_)
        n = member.type.size
        return (
            '  std::shared_ptr<ArrayWrapper<{t}>> {name}()\n'
            '  {{\n'
            '    return std::make_shared<ArrayWrapper<{t}>>(\n'
            '      std::unique_ptr<ArrayInterface<{t}>>(new ArrayReference<{t}, {n}>(&msg_->{name})),\n'
            '      py::cast(this));\n'
            '  }}').format(t=t, name=name, n=n)
    if kind == 'nested':
        h = _handle_name(type_)
        return (
            '  std::shared_ptr<{h}> {name}()\n'
            '  {{\n'
            '    return std::make_shared<{h}>(&msg_->{name}, py::cast(this));\n'
            '  }}').format(h=h, name=name)
    assert False, member.type


def member_setter_code(member):
    """C++ setter method for a message member (checked copy into the member)."""
    name = member.name
    kind, type_ = _member_kind(member.type)
    if kind == 'scalar':
        t = BASIC_TYPE_TO_CPP[type_.typename]
        return (
            '  void set_{name}(py::handle value)\n'
            '  {{\n'
            '    msg_->{name}.get() = ElementTraits<{t}>::from_py(value);\n'
            '  }}').format(t=t, name=name)
    if kind == 'string':
        cast = _string_cpp_cast(type_)
        return (
            '  void set_{name}(py::handle value)\n'
            '  {{\n'
            '    msg_->{name}.assign(py::cast<{cast}>(value));\n'
            '  }}').format(cast=cast, name=name)
    if kind in ('sequence', 'array'):
        t = _element_cpp(type_)
        bound = member.type.maximum_size if isinstance(member.type, BoundedSequence) else 0
        n = member.type.size if kind == 'array' else None
        if kind == 'sequence':
            # Reuse the wrapper's source-dispatch assign (zero-copy for
            # same-dtype numpy/buffer sources, materialize otherwise).
            return (
                '  void set_{name}(py::handle value)\n'
                '  {{\n'
                '    SequenceWrapper<{t}> w(\n'
                '      std::unique_ptr<SequenceInterface<{t}>>(new SequenceReference<{t}, {bound}>(&msg_->{name})));\n'
                '    w.assign(value);\n'
                '  }}').format(t=t, name=name, bound=bound)
        return (
            '  void set_{name}(py::handle value)\n'
            '  {{\n'
            '    ArrayWrapper<{t}> w(\n'
            '      std::unique_ptr<ArrayInterface<{t}>>(new ArrayReference<{t}, {n}>(&msg_->{name})));\n'
            '    w.assign(value);\n'
            '  }}').format(t=t, name=name, n=n)
    if kind == 'nested':
        h = _handle_name(type_)
        return (
            '  void set_{name}(py::handle value)\n'
            '  {{\n'
            '    msg_->{name} = {h}::from_py(value);\n'
            '  }}').format(h=h, name=name)
    assert False, member.type


def member_as_builtin_code(member, var='m'):
    """C++ expression producing the as_builtin dict entry for a member."""
    name = member.name
    kind, type_ = _member_kind(member.type)
    if kind == 'scalar':
        return 'd["{name}"] = py::cast({var}.{name}.get());'.format(name=name, var=var)
    if kind == 'string':
        elem = _element_to_builtin(type_, '{var}.{name}'.format(var=var, name=name))
        return 'd["{name}"] = {elem};'.format(name=name, elem=elem)
    if kind in ('sequence', 'array'):
        elem = _element_to_builtin(type_, '{var}.{name}[i]'.format(var=var, name=name))
        return (
            'py::list {name}_list;\n'
            '    for (size_t i = 0; i < {var}.{name}.size(); ++i) {{\n'
            '      {name}_list.append({elem});\n'
            '    }}\n'
            '    d["{name}"] = {name}_list;').format(name=name, var=var, elem=elem)
    if kind == 'nested':
        h = _handle_name(type_)
        return 'd["{name}"] = {h}::as_builtin_dict({var}.{name});'.format(
            name=name, var=var, h=h)
    assert False, member.type


def member_from_builtin_code(member, var='(*msg_)'):
    """C++ statement reading a member from a dict (from_builtin/from_py).

    ``var`` is the message object expression (``(*msg_)`` for from_builtin,
    ``m`` for from_py).
    """
    name = member.name
    kind, type_ = _member_kind(member.type)
    if kind == 'scalar':
        t = BASIC_TYPE_TO_CPP[type_.typename]
        return '{var}.{name}.get() = py::cast<{t}>(d["{name}"]);'.format(
            var=var, name=name, t=t)
    if kind == 'string':
        cast = _string_cpp_cast(type_)
        return '{var}.{name}.assign(py::cast<{cast}>(d["{name}"]));'.format(
            var=var, name=name, cast=cast)
    if kind in ('sequence', 'array'):
        t = _element_cpp(type_)
        n = member.type.size if kind == 'array' else None
        size_check = (
            '    if (tmp.size() != {n}) {{\n'
            '      throw py::value_error("array requires exactly {n} elements");\n'
            '    }}\n').format(n=n) if n is not None else ''
        assign = (
            '    for (size_t i = 0; i < {n}; ++i) {{\n'
            '      {var}.{name}[i] = tmp[i];\n'
            '    }}\n').format(n=n, var=var, name=name) if n is not None else (
            '    {var}.{name}.assign(tmp.begin(), tmp.end());\n').format(var=var, name=name)
        return (
            '{{\n'
            '    std::vector<{t}> tmp;\n'
            '    for (auto item : py::cast<py::list>(d["{name}"])) {{\n'
            '      tmp.push_back({elem_from_py});\n'
            '    }}\n'
            '{size_check}'
            '{assign}'
            '  }}').format(
                t=t, name=name, elem_from_py=_element_from_py(type_),
                size_check=size_check, assign=assign)
    if kind == 'nested':
        h = _handle_name(type_)
        return '{var}.{name} = {h}::from_py(d["{name}"]);'.format(
            var=var, name=name, h=h)
    assert False, member.type


def member_create_code(member):
    """C++ statement handling a keyword override in create()."""
    name = member.name
    return (
        'if (key == "{name}") {{ h->set_{name}(item.second); matched = true; }}'
    ).format(name=name)


def member_repr_code(member, var='m'):
    """C++ expression producing the repr fragment for a member."""
    name = member.name
    kind, type_ = _member_kind(member.type)
    if kind == 'scalar':
        return 'std::to_string({var}.{name}.get())'.format(var=var, name=name)
    if kind == 'string':
        elem = _element_to_builtin(type_, '{var}.{name}'.format(var=var, name=name))
        return 'py::cast<std::string>(py::repr({elem}))'.format(elem=elem)
    if kind in ('sequence', 'array'):
        return 'py::cast<std::string>(py::repr(as_builtin()["{name}"]))'.format(name=name)
    if kind == 'nested':
        h = _handle_name(type_)
        return 'py::cast<std::string>(py::repr({h}::as_builtin_dict({var}.{name})))'.format(
            var=var, name=name, h=h)
    assert False, member.type


def _message_is_supported(message, supported, package_name):
    """True if every member has a supported binding shape.

    Unsupported shapes (services/actions, or same-package nested messages
    that are themselves unsupported) are skipped with a no-op register
    function until the generator handles them. Cross-package nested messages
    are assumed supported (the dependency package generates its own
    bindings).
    """
    for member in message.structure.members:
        if member.name == EMPTY_STRUCTURE_REQUIRED_MEMBER_NAME:
            continue
        kind, type_ = _member_kind(member.type)
        if kind == 'unsupported':
            return False
        if kind == 'nested':
            if type_.namespaces[0] == package_name and type_.name not in supported:
                return False
        if kind in ('sequence', 'array'):
            if isinstance(type_, NamespacedType):
                if type_.namespaces[0] == package_name and type_.name not in supported:
                    return False
            elif not isinstance(type_, (BasicType, AbstractGenericString)):
                return False
    return True


def compute_supported_messages(messages, package_name):
    """Fixpoint: a message is supported if it and its same-package nested
    messages are."""
    supported = set()
    changed = True
    while changed:
        changed = False
        for message in messages:
            name = message.structure.namespaced_type.name
            if name in supported:
                continue
            if _message_is_supported(message, supported, package_name):
                supported.add(name)
                changed = True
    return supported


def collect_messages(idl_content):
    """All messages to bind: top-level messages plus service request/response
    and action goal/result/feedback constituent messages."""
    messages = list(idl_content.get_elements_of_type(Message))
    for service in idl_content.get_elements_of_type(Service):
        messages.append(service.request_message)
        messages.append(service.response_message)
    for action in idl_content.get_elements_of_type(Action):
        messages.append(action.goal)
        messages.append(action.result)
        messages.append(action.feedback)
    return messages


def compute_element_messages(messages):
    """Message names used as sequence/array element types (need container
    wrapper registration)."""
    element_messages = set()
    for message in messages:
        for member in message.structure.members:
            t = member.type
            if isinstance(t, AbstractNestedType):
                t = t.value_type
            if isinstance(t, NamespacedType):
                element_messages.add(t.name)
    return element_messages


def generate_py(generator_arguments_file, typesupport_impls):
    args = read_generator_arguments(generator_arguments_file)
    package_name = args['package_name']

    # Parse all IDL files up front: needed for the supported-message fixpoint
    # and the experimental module generation.
    idl_content = IdlContent()
    for idl_tuple in args.get('idl_tuples', []):
        idl_parts = idl_tuple.rsplit(':', 1)
        assert len(idl_parts) == 2
        locator = IdlLocator(*idl_parts)
        idl_file = parse_idl_file(locator)
        idl_content.elements += idl_file.content.elements
    messages = collect_messages(idl_content)
    supported_messages = compute_supported_messages(messages, package_name)
    element_messages = compute_element_messages(messages)

    mapping = {
        '_idl.py.em': '_%s.py',
        '_idl_support.c.em': '_%s_s.c',
        # Experimental messages: pybind11 bindings (replaces the legacy
        # Python experimental message classes). Services/actions are deferred.
        'msg__experimental.cpp.em': 'experimental/detail/%s__cpython_binding.hpp',
        'msg__experimental_impl.cpp.em': 'experimental/detail/%s__cpython_binding.cpp',
    }
    generated_files = generate_files(
        generator_arguments_file, mapping,
        additional_context={
            'member_getter_decl': member_getter_decl,
            'member_setter_decl': member_setter_decl,
            'member_getter_impl': member_getter_impl,
            'member_setter_impl': member_setter_impl,
            'member_getter_code': member_getter_code,
            'member_setter_code': member_setter_code,
            'member_as_builtin_code': member_as_builtin_code,
            'member_from_builtin_code': member_from_builtin_code,
            'member_create_code': member_create_code,
            'member_repr_code': member_repr_code,
            'constant_to_cpp': constant_to_cpp,
            'member_constraint_type': member_constraint_type,
            'supported_messages': supported_messages,
            'element_messages': element_messages,
        })

    args = read_generator_arguments(generator_arguments_file)
    package_name = args['package_name']

    # expand init modules for each directory
    modules = {}
    idl_content = IdlContent()
    for idl_tuple in args.get('idl_tuples', []):
        idl_parts = idl_tuple.rsplit(':', 1)
        assert len(idl_parts) == 2

        idl_rel_path = pathlib.Path(idl_parts[1])
        idl_stems = modules.setdefault(str(idl_rel_path.parent), set())
        idl_stems.add(idl_rel_path.stem)

        locator = IdlLocator(*idl_parts)
        idl_file = parse_idl_file(locator)
        idl_content.elements += idl_file.content.elements

    # NOTE(sam): remove when a language specific name mangling is implemented

    def print_warning_if_reserved_keyword(member_name, interface_type, interface_name):
        if (keyword.iskeyword(member.name)):
            print(
                "Member name '{}' in the {} '{}' is a "
                'reserved keyword in Python and is not supported '
                'at the moment. Please use a different name.'
                .format(member_name, interface_type, interface_name),
                file=sys.stderr)

    for message in idl_content.get_elements_of_type(Message):
        for member in message.structure.members:
            print_warning_if_reserved_keyword(
                member.name, 'message',
                message.structure.namespaced_type.name)

    for service in idl_content.get_elements_of_type(Service):
        for member in service.request_message.structure.members:
            print_warning_if_reserved_keyword(
                member.name, 'service request',
                service.namespaced_type.name)
        for member in service.response_message.structure.members:
            print_warning_if_reserved_keyword(
                member.name, 'service response',
                service.namespaced_type.name)

    for action in idl_content.get_elements_of_type(Action):
        for member in action.goal.structure.members:
            print_warning_if_reserved_keyword(
                member.name, 'action goal',
                action.namespaced_type.name)
        for member in action.feedback.structure.members:
            print_warning_if_reserved_keyword(
                member.name, 'action feedback',
                action.namespaced_type.name)
        for member in action.result.structure.members:
            print_warning_if_reserved_keyword(
                member.name, 'action result',
                action.namespaced_type.name)

    for subfolder in modules.keys():
        with open(os.path.join(args['output_dir'], subfolder, '__init__.py'), 'w') as f:
            module_names = {}
            for idl_stem in modules[subfolder]:
                module_names[idl_stem] = '_' + \
                    convert_camel_case_to_lower_case_underscore(idl_stem)
            # sorting after lower case conversion to get true order
            for module_name, idl_stem in \
                    sorted((value, key) for (key, value) in module_names.items()):
                f.write(
                    f'from {package_name}.{subfolder}.{module_name} import '
                    f'{idl_stem}  # noqa: F401\n')
                if subfolder == 'srv':
                    f.write(
                        f'from {package_name}.{subfolder}.{module_name} import '
                        f'{idl_stem}_Event  # noqa: F401\n')
                    f.write(
                        f'from {package_name}.{subfolder}.{module_name} import '
                        f'{idl_stem}_Request  # noqa: F401\n')
                    f.write(
                        f'from {package_name}.{subfolder}.{module_name} import '
                        f'{idl_stem}_Response  # noqa: F401\n')
                elif subfolder == 'action':
                    f.write(
                        f'from {package_name}.{subfolder}.{module_name} import '
                        f'{idl_stem}_GetResult_Event  # noqa: F401\n')
                    f.write(
                        f'from {package_name}.{subfolder}.{module_name} import '
                        f'{idl_stem}_GetResult_Request  # noqa: F401\n')
                    f.write(
                        f'from {package_name}.{subfolder}.{module_name} import '
                        f'{idl_stem}_GetResult_Response  # noqa: F401\n')
                    f.write(
                        f'from {package_name}.{subfolder}.{module_name} import '
                        f'{idl_stem}_SendGoal_Event  # noqa: F401\n')
                    f.write(
                        f'from {package_name}.{subfolder}.{module_name} import '
                        f'{idl_stem}_SendGoal_Request  # noqa: F401\n')
                    f.write(
                        f'from {package_name}.{subfolder}.{module_name} import '
                        f'{idl_stem}_SendGoal_Response  # noqa: F401\n')

    # expand experimental init modules for each directory
    for subfolder in modules.keys():
        experimental_dir = os.path.join(args['output_dir'], subfolder, 'experimental')
        os.makedirs(experimental_dir, exist_ok=True)
        with open(os.path.join(experimental_dir, '__init__.py'), 'w') as f:
            # Experimental messages, services, and actions are bound by the
            # compiled pybind11 extension _bindings (installed into the
            # msg/experimental directory); re-export its classes.
            f.write(
                f'from {package_name}.msg.experimental._bindings '
                f'import *  # noqa: F401,F403\n')

    # Generate the package-level pybind11 module: includes every interface
    # binding header (msg/srv/action), imports dependency extensions
    # (cross-DSO type sharing), and registers each message.
    if modules:
        # Only import dependency packages whose messages are referenced as
        # nested member types (cross-package type sharing); importing every
        # dependency would fail for service-only deps without bindings.
        nested_dep_pkgs = set()
        for message in collect_messages(idl_content):
            for member in message.structure.members:
                t = member.type
                if isinstance(t, AbstractNestedType):
                    t = t.value_type
                if isinstance(t, NamespacedType) and t.namespaces[0] != package_name:
                    nested_dep_pkgs.add(t.namespaces[0])
        module_includes = []
        register_calls = []
        dependency_imports = []
        for idl_tuple in args.get('idl_tuples', []):
            idl_parts = idl_tuple.rsplit(':', 1)
            locator = IdlLocator(*idl_parts)
            idl_file = parse_idl_file(locator)
            idl_rel = pathlib.Path(idl_parts[1])
            folder = str(idl_rel.parent)
            stem = convert_camel_case_to_lower_case_underscore(idl_rel.stem)
            module_includes.append(
                f'#include "{package_name}/{folder}/experimental/detail/'
                f'{stem}__cpython_binding.hpp"')
            for message in collect_messages(idl_file.content):
                msg_underscore = convert_camel_case_to_lower_case_underscore(
                    message.structure.namespaced_type.name)
                register_calls.append(
                    f'  rosidl_runtime_cpython::register_{msg_underscore}(m);')
        for dep in sorted(nested_dep_pkgs):
            dependency_imports.append(
                f'  py::module_::import("{dep}.msg.experimental._bindings");')
        module_file = os.path.join(
            args['output_dir'], 'msg', 'experimental', f'{package_name}__module.cpp')
        with open(module_file, 'w') as f:
            f.write(
                '// generated from rosidl_generator_py (experimental pybind11 module)\n'
                '// generated code does not contain a copyright notice\n'
                '\n'
                '#include <pybind11/pybind11.h>\n'
                '\n'
                '#include "rosidl_runtime_cpython/message_handle.hpp"\n'
                + '\n'.join(module_includes) + '\n'
                '\n'
                'PYBIND11_MODULE(_bindings, m)\n'
                '{\n'
                '  py::module_::import("rosidl_runtime_cpython._primitives");\n'
                + '\n'.join(dependency_imports) + '\n'
                + '\n'.join(register_calls) + '\n'
                '}\n')
        generated_files.append(module_file)

    # expand templates per available typesupport implementation
    template_dir = args['template_dir']
    type_support_impl_by_filename = {
        '_%s_s.ep.{0}.c'.format(impl): impl for impl in typesupport_impls
    }
    mapping_msg_pkg_extension = {
        os.path.join(template_dir, '_idl_pkg_typesupport_entry_point.c.em'):
        type_support_impl_by_filename.keys(),
    }

    for template_file in mapping_msg_pkg_extension.keys():
        assert os.path.exists(template_file), 'Could not find template: ' + template_file

    latest_target_timestamp = get_newest_modification_time(args['target_dependencies'])

    for template_file, generated_filenames in mapping_msg_pkg_extension.items():
        for generated_filename in generated_filenames:
            package_name = args['package_name']
            data = {
                'package_name': args['package_name'],
                'content': idl_content,
                'typesupport_impl': type_support_impl_by_filename.get(generated_filename, ''),
            }
            generated_file = os.path.join(
                args['output_dir'], generated_filename % package_name
            )
            expand_template(
                template_file, data, generated_file,
                minimum_timestamp=latest_target_timestamp)
            generated_files.append(generated_file)

    return generated_files


def value_to_py(type_, value, array_as_tuple=False):
    assert value is not None

    if not isinstance(type_, AbstractNestedType):
        return primitive_value_to_py(type_, value)

    py_values = []
    for single_value in literal_eval(value):
        py_value = primitive_value_to_py(type_.value_type, single_value)
        py_values.append(py_value)

    if (
        isinstance(type_.value_type, BasicType) and
        type_.value_type.typename in SPECIAL_NESTED_BASIC_TYPES
    ):
        if isinstance(type_, Array):
            return 'numpy.array((%s, ), dtype=%s)' % (
                ', '.join(py_values),
                SPECIAL_NESTED_BASIC_TYPES[type_.value_type.typename]['dtype'])
        if isinstance(type_, AbstractSequence):
            return "array.array('%s', (%s, ))" % (
                SPECIAL_NESTED_BASIC_TYPES[type_.value_type.typename]['type_code'],
                ', '.join(py_values))
        assert False
    if array_as_tuple:
        return '(%s)' % ', '.join(py_values)
    else:
        return '[%s]' % ', '.join(py_values)


def primitive_value_to_py(type_, value):
    assert value is not None

    if isinstance(type_, AbstractGenericString):
        return quoted_string(value)

    assert isinstance(type_, BasicType)

    if type_.typename == 'boolean':
        return 'True' if value else 'False'

    if type_.typename in INTEGER_TYPES:
        return str(value)

    if type_.typename == 'char':
        return repr('%c' % value)

    if type_.typename == 'octet':
        return repr(bytes([value]))

    if type_.typename in FLOATING_POINT_TYPES:
        return '%s' % value

    assert False, "unknown primitive type '%s'" % type_.typename


def type_constraint_type(type_):
    """C++ constraint type for a member TYPE, or None if no constraint.

    Mirrors rosidl_generator_cpp.experimental_constraint_type: bounded types
    carry their limit in the type itself; scalars and primitive arrays are
    fixed; unbounded strings get StringConstraint; nested messages get
    Msg::Constraints; sequences get SequenceConstraint<elem>.
    """
    if isinstance(type_, BasicType):
        return None
    if isinstance(type_, (AbstractString, AbstractWString)):
        return None if type_.has_maximum_size() else 'rosidl_runtime_cpp::StringConstraint'
    if isinstance(type_, NamespacedType):
        return '{}::Constraints'.format(experimental_namespaced_type_name(type_))
    if isinstance(type_, Array):
        vt = type_.value_type
        if isinstance(vt, BasicType):
            return None
        if isinstance(vt, (AbstractString, AbstractWString)):
            return None if vt.has_maximum_size() else 'rosidl_runtime_cpp::StringConstraint'
        if isinstance(vt, NamespacedType):
            return '{}::Constraints'.format(experimental_namespaced_type_name(vt))
        return None
    if isinstance(type_, AbstractSequence):
        if isinstance(type_, BoundedSequence):
            elem_constraint = type_constraint_type(type_.value_type)
            if elem_constraint is not None:
                return 'rosidl_runtime_cpp::SequenceConstraint<{}>'.format(
                    _element_cpp(type_.value_type))
            return None
        return 'rosidl_runtime_cpp::SequenceConstraint<{}>'.format(
            _element_cpp(type_.value_type))
    return None


def member_constraint_type(member):
    """C++ constraint type for a member, or None if no constraint is needed."""
    return type_constraint_type(member.type)


def constant_to_cpp(constant):
    """C++ expression producing the Python value for a message constant."""
    type_ = constant.type
    value = constant.value
    if isinstance(type_, BasicType):
        if type_.typename == 'boolean':
            return 'py::bool_({})'.format('true' if value else 'false')
        if type_.typename in INTEGER_TYPES or type_.typename in ('byte', 'char', 'wchar', 'octet'):
            if type_.typename in ('uint8', 'uint16', 'uint32', 'uint64', 'octet', 'byte', 'char', 'wchar'):
                # Unsigned: ULL suffix keeps values > LLONG_MAX well-formed.
                return 'py::int_({}ULL)'.format(value)
            if value == '-9223372036854775808':
                # INT64_MIN is not a valid decimal literal; express as LL arithmetic.
                return 'py::int_(-9223372036854775807LL - 1)'
            return 'py::int_({})'.format(value)
        if type_.typename in FLOATING_POINT_TYPES:
            return 'py::float_({})'.format(value)
    if isinstance(type_, AbstractGenericString):
        escaped = str(value).replace('\\', '\\\\').replace('"', '\\"')
        return 'py::str("{}")'.format(escaped)
    assert False, "unknown constant type '%s'" % type_


def constant_value_to_py(type_, value):
    assert value is not None

    if isinstance(type_, BasicType):
        if type_.typename == 'boolean':
            return 'True' if value else 'False'

        if type_.typename in INTEGER_TYPES:
            return str(value)

        if type_.typename == 'char':
            return repr('%c' % value)

        if type_.typename == 'octet':
            return repr(bytes([value]))

        if type_.typename in FLOATING_POINT_TYPES:
            return '%s' % value

    if isinstance(type_, AbstractGenericString):
        return quoted_string(value)

    assert False, "unknown constant type '%s'" % type_


def quoted_string(s):
    s = s.replace('\\', '\\\\')
    # strings containing single quote but no double quotes can be wrapped in
    # double quotes without escaping
    if "'" in s and '"' not in s:
        return '"%s"' % s
    # all other strings are wrapped in single quotes, if necessary with escaped
    # single quotes
    s = s.replace("'", "\\'")
    return "'%s'" % s


def get_python_type(type_):
    if isinstance(type_, NamespacedType):
        return type_.name

    if isinstance(type_, AbstractGenericString):
        return 'str'

    if isinstance(type_, AbstractNestedType):
        if isinstance(type_.value_type, BasicType) and type_.value_type.typename == 'octet':
            return 'bytes'

        if (
            isinstance(type_.value_type, BasicType) and
            type_.value_type.typename in CHARACTER_TYPES
        ):
            return 'str'

    if isinstance(type_, BasicType) and type_.typename == 'boolean':
        return 'bool'

    if isinstance(type_, BasicType) and type_.typename == 'octet':
        return 'bytes'

    if isinstance(type_, BasicType) and type_.typename in INTEGER_TYPES:
        return 'int'

    if isinstance(type_, BasicType) and type_.typename in FLOATING_POINT_TYPES:
        return 'float'

    if isinstance(type_, BasicType) and type_.typename in CHARACTER_TYPES:
        return 'str'

    assert False, "unknown type '%s'" % type_
