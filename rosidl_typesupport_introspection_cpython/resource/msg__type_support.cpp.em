@# Included from rosidl_typesupport_introspection_cpython/resource/idl__experimental_type_support.cpp.em
@{
from rosidl_pycommon import convert_camel_case_to_lower_case_underscore
from rosidl_parser.definition import AbstractGenericString
from rosidl_parser.definition import AbstractNestedType
from rosidl_parser.definition import AbstractSequence
from rosidl_parser.definition import AbstractString
from rosidl_parser.definition import AbstractWString
from rosidl_parser.definition import Array
from rosidl_parser.definition import BasicType
from rosidl_parser.definition import BoundedSequence
from rosidl_parser.definition import NamespacedType

# Get optional force_experimental flag (set by the experimental wrapper template).
# When True, always generate experimental-message code paths regardless of namespace.
try:
    force_experimental
except NameError:
    force_experimental = False

msg_typename = message.structure.namespaced_type.name

# DDS type identity: experimental messages are alternate runtime
# representations of the same payload as their standard counterparts, so the
# DDS type name must NOT carry the experimental namespace component.  The
# introspection `message_namespace_` field is therefore the plain namespaces
# joined with :: (e.g. std_msgs::msg), identical to the standard
# representation.  The experimental component only appears in C symbol
# names and generated-code paths (code organization), never in type names.
dds_namespace = '::'.join(message.structure.namespaced_type.namespaces)

# Effective parent parts for C symbol names.
# Experimental messages use a single token (e.g. msg_experimental) to avoid
# breaking the 4-argument ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME macro.
effective_parent_parts = list(interface_path.parents[0].parts)
if force_experimental:
    effective_parent_parts = [effective_parent_parts[0] + '_experimental']

# Prefix for all generated symbols in this TU (unique per message).
function_prefix = '__'.join(
    [package_name] + effective_parent_parts + [msg_typename]
) + '__rosidl_typesupport_introspection_cpython'

# Symbol parts for a nested message introspection-cpython handle.
# Experimental messages only nest other experimental messages (they are
# separate runtime representations of the same payloads), so the nested handle
# is the nested type own experimental variant: <parent>_experimental.
def nested_symbol_parts(namespaced_type):
    ns = list(namespaced_type.namespaced_name())
    if len(ns) >= 2:
        ns[-2] = ns[-2] + '_experimental'
    return ns
}@
@{
header_files = [
    'stddef.h',
    'rosidl_runtime_c/message_type_support_struct.h',
    'rosidl_typesupport_interface/macros.h',
    'rosidl_typesupport_introspection_c/field_types.h',
    'rosidl_typesupport_introspection_c/message_introspection.h',
    'rosidl_typesupport_introspection_cpython/identifier.hpp',
    'rosidl_typesupport_introspection_cpython/python_helpers.hpp',
    'rosidl_typesupport_introspection_cpython/visibility_control.h',
]
}@
@[for header_file in header_files]@
@[    if header_file in include_directives]@
// already included above
// @
@[    else]@
@{include_directives.add(header_file)}@
@[    end if]@
@[    if '/' not in header_file]@
#include <@(header_file)>
@[    else]@
#include "@(header_file)"
@[    end if]@
@[end for]@

// pybind11 is used to implement the MessageMember function pointers; the
// Python interpreter is embedded (rclpy), and Python.h comes via pybind11.
#include <pybind11/pybind11.h>

#ifdef __cplusplus
extern "C"
{
#endif

@#<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
@# init / fini: Python message objects are fully constructed by their class;
@# there is no C memory layout to initialize or destroy.
@#>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void @(function_prefix)__@(msg_typename)_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  (void)message_memory;
  (void)_init;
}

void @(function_prefix)__@(msg_typename)_fini_function(void * message_memory)
{
  (void)message_memory;
}

@#<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
@# Message member array.  Function pointers are only set for array/sequence
@# members (matching the C introspection convention); they are implemented by
@# the shared pybind11 helpers treating void * as a type-erased PyObject *.
@#>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static rosidl_typesupport_introspection_c__MessageMember @(function_prefix)__@(msg_typename)_message_member_array[@(len(message.structure.members))] = {
@{
for index, member in enumerate(message.structure.members):
    type_ = member.type
    if isinstance(type_, AbstractNestedType):
        type_ = type_.value_type

    print('  {')

    # const char * name_
    print('    "%s",  // name' % member.name)
    if isinstance(type_, BasicType):
        # uint8_t type_id_
        print('    rosidl_typesupport_introspection_c__ROS_TYPE_%s,  // type' % type_.typename.replace(' ', '_').upper())
        # size_t string_upper_bound
        print('    0,  // upper bound of string')
        # const rosidl_message_type_support_t * members_
        print('    NULL,  // members of sub message')
    elif isinstance(type_, AbstractGenericString):
        # uint8_t type_id_
        if isinstance(type_, AbstractString):
            print('    rosidl_typesupport_introspection_c__ROS_TYPE_STRING,  // type')
        elif isinstance(type_, AbstractWString):
            print('    rosidl_typesupport_introspection_c__ROS_TYPE_WSTRING,  // type')
        else:
            assert False, 'Unknown type: ' + str(type_)
        # size_t string_upper_bound
        print('    %u,  // upper bound of string' % (type_.maximum_size if type_.has_maximum_size() else 0))
        # const rosidl_message_type_support_t * members_
        print('    NULL,  // members of sub message')
    else:
        # uint8_t type_id_
        print('    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type')
        # size_t string_upper_bound
        print('    0,  // upper bound of string')
        # const rosidl_message_type_support_t * members_
        print('    NULL,  // members of sub message (initialized lazily)')
    # bool is_key_
    print('    %s,  // is key' % ('true' if member.has_annotation('key') else 'false'))
    # bool is_array_
    print('    %s,  // is array' % ('true' if isinstance(member.type, AbstractNestedType) else 'false'))
    # size_t array_size_
    print('    %u,  // array size' % (member.type.size if isinstance(member.type, Array) else (member.type.maximum_size if isinstance(member.type, BoundedSequence) else 0)))
    # bool is_upper_bound_
    print('    %s,  // is upper bound' % ('true' if isinstance(member.type, BoundedSequence) else 'false'))
    # uint32_t offset_
    print('    0,  // bytes offset in struct (Python objects have no C layout)')
    # void * default_value_
    print('    NULL,  // default value')  # Python class constructors provide defaults

    # size_t(const void *) size_function
    print('    %s,  // size() function pointer' % ('rosidl_typesupport_introspection_cpython::detail::cpython_size_function' if isinstance(member.type, AbstractNestedType) else 'NULL'))
    # const void *(const void *, size_t) get_const_function
    print('    %s,  // get_const(index) function pointer' % ('rosidl_typesupport_introspection_cpython::detail::cpython_get_const_element' if isinstance(member.type, AbstractNestedType) else 'NULL'))
    # void *(void *, size_t) get_function
    print('    %s,  // get(index) function pointer' % ('rosidl_typesupport_introspection_cpython::detail::cpython_get_element' if isinstance(member.type, AbstractNestedType) else 'NULL'))
    # void(const void *, size_t, void *) fetch_function
    print('    %s,  // fetch(index, &value) function pointer' % ('rosidl_typesupport_introspection_cpython::detail::cpython_fetch_function' if isinstance(member.type, AbstractNestedType) else 'NULL'))
    # void(void *, size_t, const void *) assign_function
    print('    %s,  // assign(index, value) function pointer' % ('rosidl_typesupport_introspection_cpython::detail::cpython_assign_function' if isinstance(member.type, AbstractNestedType) else 'NULL'))
    # bool(void *, size_t) resize_function
    print('    %s  // resize(size) function pointer' % ('rosidl_typesupport_introspection_cpython::detail::cpython_resize_function' if isinstance(member.type, AbstractSequence) else 'NULL'))

    if index < len(message.structure.members) - 1:
        print('  },')
    else:
        print('  }')
}@
};

static const rosidl_typesupport_introspection_c__MessageMembers @(function_prefix)__@(msg_typename)_message_members = {
  "@(dds_namespace)",  // message namespace (no experimental in DDS type names)
  "@(msg_typename)",  // message name
  @(len(message.structure.members)),  // number of fields
  0,  // size of message in memory (Python objects are not C-layout)
@[  if message.structure.has_any_member_with_annotation('key') ]@
  true,  // has_any_key_member_
@[  else]@
  false,  // has_any_key_member_
@[  end if]@
  @(function_prefix)__@(msg_typename)_message_member_array,  // message members
  @(function_prefix)__@(msg_typename)_init_function,  // function to initialize message memory (memory has to be allocated)
  @(function_prefix)__@(msg_typename)_fini_function  // function to terminate message instance (will not free memory)
};

@#<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
@# Default fallbacks for types that lack generated type hash/description.
@# Internal linkage: each TU defines its own copy.
@#>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static const rosidl_type_hash_t *
@(msg_typename)_default_get_type_hash(const rosidl_message_type_support_t *)
{
  static const rosidl_type_hash_t zero_hash = {ROSIDL_TYPE_HASH_VERSION_UNSET, {0}};
  return &zero_hash;
}
static const rosidl_runtime_c__type_description__TypeDescription *
@(msg_typename)_default_get_type_description(const rosidl_message_type_support_t *)
{
  return NULL;
}
static const rosidl_runtime_c__type_description__TypeSource__Sequence *
@(msg_typename)_default_get_type_description_sources(const rosidl_message_type_support_t *)
{
  return NULL;
}

static rosidl_message_type_support_t @(function_prefix)__@(msg_typename)_message_type_support_handle = {
  rosidl_typesupport_introspection_cpython::typesupport_identifier,
  &@(function_prefix)__@(msg_typename)_message_members,
  get_message_typesupport_handle_function,
  &@(msg_typename)_default_get_type_hash,
  &@(msg_typename)_default_get_type_description,
  &@(msg_typename)_default_get_type_description_sources,
};

@#<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
@# Forward declarations of nested message introspection symbols.  Nested
@# experimental messages resolve to their own experimental introspection
@# handles at runtime (lazy members_ init below); the declarations let the
@# compiler see the symbols, which the linker resolves against the nested
@# package introspection libraries (linked via the generator's dependency
@# target loop).
@#>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
@{
nested_symbols = set()
for member in message.structure.members:
    type_ = member.type
    if isinstance(type_, AbstractNestedType):
        type_ = type_.value_type
    if isinstance(type_, NamespacedType):
        nested_symbols.add(tuple(nested_symbol_parts(type_)))
}@
@[for parts in sorted(nested_symbols)]@
ROSIDL_TYPESUPPORT_INTROSPECTION_CPYTHON_PUBLIC
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_cpython, @(', '.join(parts)))();
@[end for]@

ROSIDL_TYPESUPPORT_INTROSPECTION_CPYTHON_PUBLIC
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_cpython, @(', '.join([package_name] + effective_parent_parts)), @(msg_typename))() {
@[for i, member in enumerate(message.structure.members)]@
@{
type_ = member.type
if isinstance(type_, AbstractNestedType):
    type_ = type_.value_type
}@
@[    if isinstance(type_, NamespacedType)]@
  @(function_prefix)__@(msg_typename)_message_member_array[@(i)].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_cpython, @(', '.join(nested_symbol_parts(type_))))();
@[    end if]@
@[end for]@
  return &@(function_prefix)__@(msg_typename)_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif
