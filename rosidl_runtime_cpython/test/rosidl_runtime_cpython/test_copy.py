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

"""Tests for rosidl_runtime_py.experimental copy.deepcopy_into."""


import pytest

from rosidl_runtime_cpython._raw_buffer import RawBuffer
from rosidl_runtime_cpython.copy import deepcopy_into
from rosidl_runtime_cpython.dtype import Dtype
from rosidl_runtime_cpython.sequence import Sequence
from rosidl_runtime_cpython.string import String


def make_string_pool(n, capacity=16):
    return [String(buffer=RawBuffer(capacity, growing=False)) for _ in range(n)]


# ---------------------------------------------------------------------------
# Pool-backed object sequences
# ---------------------------------------------------------------------------

def test_deepcopy_pool_preserves_identity():
    pool_dst = make_string_pool(3)
    dst = Sequence(String, element_pool=pool_dst)
    dst.resize(3)
    src = Sequence(String)
    src.assign(['a', 'b', 'c'])
    deepcopy_into(dst, src)
    assert dst[0] is pool_dst[0]        # identity preserved, not rebuilt
    assert dst[1] is pool_dst[1]
    assert str(dst[0]) == 'a'
    assert str(dst[1]) == 'b'
    assert str(src[0]) == 'a'           # source untouched


def test_deepcopy_pool_overflow_raises():
    pool_dst = make_string_pool(2)
    dst = Sequence(String, element_pool=pool_dst)
    dst.resize(2)
    src = Sequence(String)
    src.assign(['a', 'b', 'c'])
    with pytest.raises(BufferError, match='capacity'):
        deepcopy_into(dst, src)


def test_deepcopy_pool_shorter_src_clears_excess():
    pool_dst = make_string_pool(3)
    dst = Sequence(String, element_pool=pool_dst)
    dst.assign(['x', 'y', 'z'])
    src = Sequence(String)
    src.assign(['a', 'b'])
    deepcopy_into(dst, src)
    assert len(dst) == 2
    assert str(dst[0]) == 'a'
    assert str(dst[1]) == 'b'
    assert len(pool_dst[2]) == 0


def test_deepcopy_pool_empty_source():
    pool_dst = make_string_pool(2)
    dst = Sequence(String, element_pool=pool_dst)
    dst.assign(['a', 'b'])
    src = Sequence(String)
    deepcopy_into(dst, src)
    assert len(dst) == 0
    assert len(pool_dst[0]) == 0


def test_deepcopy_pool_shallow_source_still_deep():
    # Source elements are *containers*; the copy must duplicate their content,
    # never alias them.
    src0 = String()
    src0.assign('hello')
    src1 = String()
    src1.assign('world')
    src = Sequence(String)
    src.extend([src0, src1])

    pool_dst = make_string_pool(2)
    dst = Sequence(String, element_pool=pool_dst)
    dst.resize(2)
    deepcopy_into(dst, src)

    src0.assign('MUTATED')
    assert str(dst[0]) == 'hello'       # dst did not alias src element
    assert str(dst[1]) == 'world'


def test_deepcopy_pool_self_copy_is_noop():
    pool = make_string_pool(3)
    dst = Sequence(String, element_pool=pool)
    dst.assign(['a', 'b', 'c'])
    deepcopy_into(dst, dst)
    assert list(dst) == ['a', 'b', 'c']


# ---------------------------------------------------------------------------
# External-backed strings
# ---------------------------------------------------------------------------

def test_deepcopy_external_string_to_managed():
    src = String(buffer=RawBuffer(64, growing=False))
    src.assign('hi')
    dst = String()
    deepcopy_into(dst, src)
    assert str(dst) == 'hi'
    assert len(dst) == 2


def test_deepcopy_external_string_to_external():
    src = String(buffer=RawBuffer(64, growing=False))
    src.assign('hello')
    dst = String(buffer=RawBuffer(64, growing=False))
    deepcopy_into(dst, src)
    assert str(dst) == 'hello'
    assert dst is not src


def test_deepcopy_external_string_over_capacity_raises():
    src = String()
    src.assign('hello')
    dst = String(buffer=RawBuffer(4, growing=False))
    with pytest.raises(BufferError, match='capacity'):
        deepcopy_into(dst, src)


# ---------------------------------------------------------------------------
# Unchanged paths (regression)
# ---------------------------------------------------------------------------

def test_deepcopy_primitive_sequence_unchanged():
    dst = Sequence(Dtype.INT32)
    dst.resize(3)
    src = Sequence(Dtype.INT32)
    src.assign([10, 20, 30])
    deepcopy_into(dst, src)
    assert list(dst) == [10, 20, 30]


def test_deepcopy_managed_object_sequence_rebuilds():
    # Managed receivers get fresh managed elements (no identity guarantee).
    src0 = String()
    src0.assign('a')
    src = Sequence(String)
    src.append(src0)
    dst = Sequence(String)
    deepcopy_into(dst, src)
    assert len(dst) == 1
    assert dst[0] is not src[0]         # rebuilt element, not the source
    assert str(dst[0]) == 'a'
