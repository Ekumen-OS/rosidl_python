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

from rosidl_pycommon import generate_files


def generate_cpython(generator_arguments_file, type_supports, experimental_type_supports=None):
    """
    Generate the CPython type support dispatch to handle ROS messages.

    This is the dispatch counterpart of ``rosidl_typesupport_cpp``: it
    generates, per package, a ``{pkg}__rosidl_typesupport_cpython`` library
    holding one dispatch handle per message.  Each dispatch handle carries a
    ``type_support_map_t`` over the available CPython typesupport
    implementations (e.g. ``rosidl_typesupport_xcdr_cpython``) and a
    dlsym-based dispatch function, so middleware can resolve a concrete
    implementation without knowing which language generated it.

    :param generator_arguments_file: Path location of the file containing the generator arguments
    :param type_supports: List of CPython type support implementations to be used
    :param experimental_type_supports: List of type supports with experimental message support
    """
    mapping = {}
    if experimental_type_supports:
        mapping['idl__experimental_type_support.cpp.em'] = \
            'experimental/detail/%s__type_support.cpp'
    return generate_files(
        generator_arguments_file, mapping,
        additional_context={
            'type_supports': type_supports,
            'experimental_type_supports': experimental_type_supports or [],
        })
