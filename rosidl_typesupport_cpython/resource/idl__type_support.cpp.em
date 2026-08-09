// generated from rosidl_typesupport_cpython/resource/idl__type_support.cpp.em
// with input from @(package_name):@(interface_path)
// generated code does not contain a copyright notice
@
@#######################################################################
@# EmPy template for generating <idl>__type_support.c files
@#
@# Context:
@#  - package_name (string)
@#  - interface_path (Path relative to the directory named after the package)
@#  - content (IdlContent, list of elements, e.g. Messages or Services)
@#  - type_supports (list of strings, the names of the CPython type support packages)
@#######################################################################
@{
include_directives = set()
}@
@#######################################################################
@# Handle message
@#######################################################################
@{
from rosidl_parser.definition import Message
}@
@[for message in content.get_elements_of_type(Message)]@

@{
TEMPLATE(
    'msg__type_support.cpp.em',
    package_name=package_name, interface_path=interface_path, message=message,
    include_directives=include_directives, type_supports=type_supports)
}@
@[end for]@
@
@#######################################################################
@# Handle service
@#
@# First cut: messages only (no services/actions).  Service dispatch handles
@# will be added together with the service typesupport codegen.
@#######################################################################
