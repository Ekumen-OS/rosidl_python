# Copyright 2026 Ekumen, Inc.
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

"""Tests for the generic MessageConstraints composite type.

Mirrors ``rosidl_runtime_cpp::MessageConstraints<T>``: blanket limits
(``max_string_length`` / ``max_total_size`` / ``strict``) plus an optional
``type_specific`` slot carrying a per-message ``Msg.Constraints`` instance.
"""

from rosidl_runtime_cpython.constraints import MessageConstraints


def test_defaults_match_zero_initialized_c_struct():
    c = MessageConstraints()
    assert c.type_specific is None
    assert c.max_string_length == 0
    assert c.max_total_size == 0
    assert c.strict is False


def test_keyword_construction():
    sentinel = object()
    c = MessageConstraints(
        type_specific=sentinel,
        max_string_length=100,
        max_total_size=1024,
        strict=True,
    )
    assert c.type_specific is sentinel
    assert c.max_string_length == 100
    assert c.max_total_size == 1024
    assert c.strict is True


def test_mutable_attributes():
    c = MessageConstraints()
    c.max_string_length = 50
    c.max_total_size = 512
    c.strict = True
    c.type_specific = 'cs'
    assert c.max_string_length == 50
    assert c.max_total_size == 512
    assert c.strict is True
    assert c.type_specific == 'cs'


def test_equality():
    assert MessageConstraints() == MessageConstraints()
    assert MessageConstraints(max_string_length=10) == \
        MessageConstraints(max_string_length=10)
    assert MessageConstraints(max_string_length=10) != \
        MessageConstraints(max_string_length=11)
    assert MessageConstraints(strict=True) != MessageConstraints(strict=False)
    assert MessageConstraints() != 'not constraints'


def test_repr_contains_fields():
    r = repr(MessageConstraints(max_string_length=10, strict=True))
    assert 'max_string_length=10' in r
    assert 'strict=True' in r
