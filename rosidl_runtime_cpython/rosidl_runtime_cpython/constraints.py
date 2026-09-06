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

__all__ = [
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