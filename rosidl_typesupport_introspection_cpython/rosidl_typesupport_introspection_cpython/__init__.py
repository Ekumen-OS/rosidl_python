# Copyright 2026 Ekumen Inc.
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

"""Generate the C-based introspection type support for experimental Python messages."""

from rosidl_pycommon import generate_files


def generate_cpython_introspection(generator_arguments_file: str):
    """
    Generate the introspection type support (C MessageMembers) per message.

    Only experimental message entries are emitted: the CPython introspection
    descriptors describe the experimental Python message classes (containers
    from rosidl_runtime_cpython).  Standard messages keep resolving through the
    C typesupport, whose dispatch is unchanged.

    The generated ``{pkg}__rosidl_typesupport_introspection_cpython`` library
    is resolved by the CPython dispatch map (and, through it, by middleware
    looking for an introspection typesupport) via the per-message
    ``rosidl_typesupport_introspection_cpython__get_message_type_support_handle__
    <pkg>__msg_experimental__<Name>`` symbol.

    :param generator_arguments_file: The path to the file containing the
        arguments for the generator.
    :type generator_arguments_file: str
    """
    mapping = {
        'idl__experimental_type_support.cpp.em':
        'experimental/detail/%s__type_support.cpp',
    }
    return generate_files(generator_arguments_file, mapping)
