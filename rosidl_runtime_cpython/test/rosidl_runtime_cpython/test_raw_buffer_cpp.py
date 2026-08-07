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

"""Tests for rosidl_runtime_cpython._raw_buffer_cpp (pybind11 wrapper)."""

import ctypes

import pytest

from rosidl_runtime_cpython import _raw_buffer_cpp
from rosidl_runtime_cpython._raw_buffer import RawBuffer


# ctypes allocations must outlive the RawBuffers that view them.
_allocations = []


def test_from_region_wraps_external_memory():
    data = (ctypes.c_ubyte * 16)()
    _allocations.append(data)
    buf = _raw_buffer_cpp.from_region(ctypes.addressof(data), 16)
    assert isinstance(buf, RawBuffer)
    assert buf.address == ctypes.addressof(data)
    assert buf.capacity == 16
    assert not buf.is_owner          # non-owning view
    assert not buf.growing           # external buffers never grow
    # Writes through the view are visible in the underlying memory.
    buf[0] = 7
    assert data[0] == 7


def test_from_region_zero_size():
    data = (ctypes.c_ubyte * 1)()
    _allocations.append(data)
    buf = _raw_buffer_cpp.from_region(ctypes.addressof(data), 0)
    assert buf.capacity == 0
    assert len(buf) == 0


def test_from_region_null_address_raises_on_access():
    # The C API does not validate the address at creation; accessing the
    # buffer (buffer protocol) raises because there is no backing memory.
    buf = _raw_buffer_cpp.from_region(0, 16)
    assert buf.address == 0
    with pytest.raises(BufferError, match='no data'):
        memoryview(buf)
