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

"""Constraint types for experimental messages.

These mirror the ``rosidl_runtime_cpp::StringConstraint`` and
``rosidl_runtime_cpp::SequenceConstraint<T>`` types from the C++ experimental
runtime, providing runtime-queryable bounds for variable-length message
members.  The classes are the native pybind11 bindings from the
``_primitives`` extension; this module re-exports them under the
``rosidl_runtime_cpython.constraints`` namespace.

``MessageConstraints`` mirrors ``rosidl_runtime_cpp::MessageConstraints<T>``:
a composite of blanket limits (``max_string_length`` / ``max_total_size`` /
``strict``) plus an optional ``type_specific`` slot carrying a per-message
``Msg.Constraints`` instance.  It is the single type ``rclpy`` accepts for
publisher / subscription / loan constraints.
"""

from rosidl_runtime_cpython._primitives import BoolSequenceConstraint
from rosidl_runtime_cpython._primitives import Float32SequenceConstraint
from rosidl_runtime_cpython._primitives import Float64SequenceConstraint
from rosidl_runtime_cpython._primitives import Int8SequenceConstraint
from rosidl_runtime_cpython._primitives import Int16SequenceConstraint
from rosidl_runtime_cpython._primitives import Int32SequenceConstraint
from rosidl_runtime_cpython._primitives import Int64SequenceConstraint
from rosidl_runtime_cpython._primitives import LongDoubleSequenceConstraint
from rosidl_runtime_cpython._primitives import StringConstraint
from rosidl_runtime_cpython._primitives import StringSequenceConstraint
from rosidl_runtime_cpython._primitives import UInt8SequenceConstraint
from rosidl_runtime_cpython._primitives import UInt16SequenceConstraint
from rosidl_runtime_cpython._primitives import UInt32SequenceConstraint
from rosidl_runtime_cpython._primitives import UInt64SequenceConstraint
from rosidl_runtime_cpython._primitives import WStringSequenceConstraint

class MessageConstraints:
    """Composite constraints for a message type.

    Mirrors ``rosidl_runtime_cpp::MessageConstraints<T>``: blanket limits
    applying to all variable-length members, plus an optional
    ``type_specific`` per-message ``Msg.Constraints`` instance.

    :param type_specific: per-message constraints object
        (e.g. a ``Msg.Constraints`` instance), or ``None`` for blanket
        limits only.
    :param max_string_length: blanket maximum length for any string member
        (characters, 0 = unlimited).
    :param max_total_size: blanket maximum total serialized size in bytes
        (0 = unlimited).
    :param strict: request full per-field validation after the payload-size
        check (False = cheap payload-size checks, with automatic full
        validation on failure for diagnostics).
    """

    def __init__(
        self,
        type_specific=None,
        max_string_length=0,
        max_total_size=0,
        strict=False,
    ):
        self.type_specific = type_specific
        self.max_string_length = max_string_length
        self.max_total_size = max_total_size
        self.strict = strict

    def __eq__(self, other):
        if not isinstance(other, MessageConstraints):
            return NotImplemented
        return (
            self.type_specific == other.type_specific
            and self.max_string_length == other.max_string_length
            and self.max_total_size == other.max_total_size
            and self.strict == other.strict
        )

    def __repr__(self):
        return (
            f'MessageConstraints(type_specific={self.type_specific!r}, '
            f'max_string_length={self.max_string_length!r}, '
            f'max_total_size={self.max_total_size!r}, '
            f'strict={self.strict!r})'
        )


__all__ = [
    'MessageConstraints',
    'StringConstraint',
    'BoolSequenceConstraint',
    'UInt8SequenceConstraint',
    'UInt16SequenceConstraint',
    'UInt32SequenceConstraint',
    'UInt64SequenceConstraint',
    'Int8SequenceConstraint',
    'Int16SequenceConstraint',
    'Int32SequenceConstraint',
    'Int64SequenceConstraint',
    'Float32SequenceConstraint',
    'Float64SequenceConstraint',
    'LongDoubleSequenceConstraint',
    'StringSequenceConstraint',
    'WStringSequenceConstraint',
]