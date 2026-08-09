@# Included from rosidl_typesupport_cpython/resource/idl__type_support.cpp.em
@{
from rosidl_pycommon import convert_camel_case_to_lower_case_underscore

# Get optional force_experimental flag (set by experimental wrapper templates).
# When True, always generate experimental-message code paths regardless of namespace.
try:
    force_experimental
except NameError:
    force_experimental = False

# Get optional experimental_type_supports (the subset that support experimental messages).
try:
    experimental_type_supports
except NameError:
    experimental_type_supports = []

# Determine effective type supports for this dispatch file
if force_experimental and experimental_type_supports:
    effective_type_supports = experimental_type_supports
else:
    effective_type_supports = type_supports

# CPython dispatch always uses the multi-typesupport path: the dispatch handle
# carries a type_support_map_t whose symbol names are resolved via dlsym from
# the per-message implementation libraries (there is no per-message dispatch
# header to include directly, unlike introspection).
use_multi_typesupport = True

# Determine effective parent parts for C symbol names
# Experimental messages use "msg_experimental" as the parent folder
# to avoid breaking the 4-argument ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME macro
effective_parent_parts = list(interface_path.parents[0].parts)
if force_experimental:
    effective_parent_parts = [effective_parent_parts[0] + '_experimental']

header_files = [
    'cstddef',
    'rosidl_runtime_c/message_type_support_struct.h',
]
if use_multi_typesupport:
    header_files += [
        'rosidl_typesupport_c/type_support_map.h',
        'rosidl_typesupport_cpython/identifier.hpp',
        'rosidl_typesupport_cpython/message_type_support_dispatch.hpp',
    ]
header_files.append('rosidl_typesupport_cpython/visibility_control.h')
if use_multi_typesupport:
    header_files.append('rosidl_typesupport_interface/macros.h')
}@
@[for header_file in header_files]@
@[    if header_file in include_directives]@
// already included above
// @
@[    else]@
@{include_directives.add(header_file)}@
@[    end if]@
#include "@(header_file)"
@[end for]@
@
@[if use_multi_typesupport]@
@[  if force_experimental]@
@[    for ns in list(message.structure.namespaced_type.namespaces) + ['experimental']]@

namespace @(ns)
{
@[    end for]@
@[  else]@
@[    for ns in message.structure.namespaced_type.namespaces]@

namespace @(ns)
{
@[    end for]@
@[  end if]@

namespace rosidl_typesupport_cpython
{

typedef struct _@(message.structure.namespaced_type.name)_type_support_ids_t
{
  const char * typesupport_identifier[@(len(effective_type_supports))];
} _@(message.structure.namespaced_type.name)_type_support_ids_t;

static const _@(message.structure.namespaced_type.name)_type_support_ids_t _@(message.structure.namespaced_type.name)_message_typesupport_ids = {
  {
@# TODO(dirk-thomas) use identifier symbol again
@[for type_support in sorted(effective_type_supports)]@
    "@(type_support)",  // ::@(type_support)::typesupport_identifier,
@[end for]@
  }
};

typedef struct _@(message.structure.namespaced_type.name)_type_support_symbol_names_t
{
  const char * symbol_name[@(len(effective_type_supports))];
} _@(message.structure.namespaced_type.name)_type_support_symbol_names_t;

#define STRINGIFY_(s) #s
#define STRINGIFY(s) STRINGIFY_(s)

static const _@(message.structure.namespaced_type.name)_type_support_symbol_names_t _@(message.structure.namespaced_type.name)_message_typesupport_symbol_names = {
  {
@[for type_support in sorted(effective_type_supports)]@
    STRINGIFY(ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(@(type_support), @(', '.join([package_name] + effective_parent_parts)), @(message.structure.namespaced_type.name))),
@[end for]@
  }
};

typedef struct _@(message.structure.namespaced_type.name)_type_support_data_t
{
  void * data[@(len(effective_type_supports))];
} _@(message.structure.namespaced_type.name)_type_support_data_t;

static _@(message.structure.namespaced_type.name)_type_support_data_t _@(message.structure.namespaced_type.name)_message_typesupport_data = {
  {
@[for type_support in sorted(effective_type_supports)]@
    0,  // will store the shared library later
@[end for]@
  }
};

static const type_support_map_t _@(message.structure.namespaced_type.name)_message_typesupport_map = {
  @(len(effective_type_supports)),
  "@(package_name)",
  &_@(message.structure.namespaced_type.name)_message_typesupport_ids.typesupport_identifier[0],
  &_@(message.structure.namespaced_type.name)_message_typesupport_symbol_names.symbol_name[0],
  &_@(message.structure.namespaced_type.name)_message_typesupport_data.data[0],
};

// Default fallbacks for types that lack generated type hash/description.
// Internal linkage: each TU (standard and experimental) defines its own copy.
static const rosidl_type_hash_t *
@(message.structure.namespaced_type.name)_default_get_type_hash(const rosidl_message_type_support_t *)
{
  static const rosidl_type_hash_t zero_hash = {ROSIDL_TYPE_HASH_VERSION_UNSET, {0}};
  return &zero_hash;
}
static const rosidl_runtime_c__type_description__TypeDescription *
@(message.structure.namespaced_type.name)_default_get_type_description(const rosidl_message_type_support_t *)
{
  return nullptr;
}
static const rosidl_runtime_c__type_description__TypeSource__Sequence *
@(message.structure.namespaced_type.name)_default_get_type_description_sources(const rosidl_message_type_support_t *)
{
  return nullptr;
}

static const rosidl_message_type_support_t @(message.structure.namespaced_type.name)_message_type_support_handle = {
  ::rosidl_typesupport_cpython::typesupport_identifier,
  reinterpret_cast<const type_support_map_t *>(&_@(message.structure.namespaced_type.name)_message_typesupport_map),
  ::rosidl_typesupport_cpython::get_message_typesupport_handle_function,
  &@(message.structure.namespaced_type.name)_default_get_type_hash,
  &@(message.structure.namespaced_type.name)_default_get_type_description,
  &@(message.structure.namespaced_type.name)_default_get_type_description_sources,
};

}  // namespace rosidl_typesupport_cpython
@[  if force_experimental]@
@[    for ns in reversed(list(message.structure.namespaced_type.namespaces) + ['experimental'])]@

}  // namespace @(ns)
@[    end for]@
@[  else]@
@[    for ns in reversed(message.structure.namespaced_type.namespaces)]@

}  // namespace @(ns)
@[    end for]@
@[  end if]@

@[else]@
@{
if force_experimental:
    include_parts = [package_name] + list(interface_path.parents[0].parts) + [
        'experimental', 'detail', convert_camel_case_to_lower_case_underscore(interface_path.stem)]
else:
    include_parts = [package_name] + list(interface_path.parents[0].parts) + [
        'detail', convert_camel_case_to_lower_case_underscore(interface_path.stem)]
include_base = '/'.join(include_parts)
header_file = include_base + '__' + list(effective_type_supports)[0] + '.hpp'
}@
@[  if header_file in include_directives]@
// already included above
// @
@[  else]@
@{include_directives.add(header_file)}@
@[  end if]@
#include "@(header_file)"

@[end if]@
#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_CPYTHON_PUBLIC
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_cpython, @(', '.join([package_name] + effective_parent_parts)), @(message.structure.namespaced_type.name))() {
@[if use_multi_typesupport]@
@[  if force_experimental]@
  return &::@('::'.join([package_name] + list(interface_path.parents[0].parts) + ['experimental']))::rosidl_typesupport_cpython::@(message.structure.namespaced_type.name)_message_type_support_handle;
@[  else]@
  return &::@('::'.join([package_name] + list(interface_path.parents[0].parts)))::rosidl_typesupport_cpython::@(message.structure.namespaced_type.name)_message_type_support_handle;
@[  end if]@
@[else]@
  return ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(@(list(effective_type_supports)[0]), @(', '.join([package_name] + effective_parent_parts)), @(message.structure.namespaced_type.name))();
@[end if]@
}

#ifdef __cplusplus
}
#endif
