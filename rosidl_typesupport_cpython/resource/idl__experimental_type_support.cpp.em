@# generated from rosidl_typesupport_cpython/resource/idl__experimental_type_support.cpp.em
@# with input from @(package_name):@(interface_path)
@# generated code does not contain a copyright notice
@{
from rosidl_parser.definition import Message
include_directives = set()
}@
@#######################################################################
@# Handle messages
@#######################################################################
@{
for message in content.get_elements_of_type(Message):
    TEMPLATE(
        'msg__type_support.cpp.em',
        package_name=package_name, interface_path=interface_path,
        message=message, include_directives=include_directives,
        type_supports=type_supports,
        force_experimental=True,
        experimental_type_supports=experimental_type_supports)
}@

@# First cut: messages only.  Services/actions will be added together with
@# the service typesupport codegen.
