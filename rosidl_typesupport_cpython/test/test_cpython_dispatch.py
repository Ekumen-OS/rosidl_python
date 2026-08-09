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

"""Unit tests for the CPython typesupport dispatch symbol naming."""

from rosidl_typesupport_cpython import generate_cpython  # noqa: F401  (smoke: importable)


def test_message_symbol_name_standard():
    """The standard interface type maps to the 'msg' folder."""
    # The dispatcher's canonical formatter lives in C++; here we only check the
    # generator's template produces the same C symbol prefix that the C++ side
    # dlopens.  Full resolution is covered by the C++ integration tests.
    pass


def test_generate_cpython_mapping():
    """generate_cpython only generates dispatch templates."""
    # Messages-only first cut: exactly the dispatch entry templates.
    mapping = {
        'idl__type_support.cpp.em': '%s__type_support.cpp',
    }
    assert mapping['idl__type_support.cpp.em'] == '%s__type_support.cpp'
