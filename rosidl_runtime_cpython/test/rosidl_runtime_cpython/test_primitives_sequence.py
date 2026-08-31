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

"""Tests for the native unbounded primitive sequence wrappers (Phase 1)."""

import numpy as np
import pytest

from rosidl_runtime_cpython._primitives import (
    BoolSequence,
    CharSequence,
    Float32Sequence,
    Float64Sequence,
    Int16Sequence,
    Int32Sequence,
    Int64Sequence,
    Int8Sequence,
    UInt16Sequence,
    UInt32Sequence,
    UInt64Sequence,
    UInt8Sequence,
    UInt8,
)


# ---------------------------------------------------------------------------
# Construction
# ---------------------------------------------------------------------------

def test_empty():
    s = Int32Sequence()
    assert len(s) == 0


def test_from_iterable():
    s = Int32Sequence([1, 2, 3])
    assert len(s) == 3
    assert list(s) == [1, 2, 3]


def test_aliases_are_same_class():
    assert CharSequence is UInt8Sequence


# ---------------------------------------------------------------------------
# Indexing / slicing
# ---------------------------------------------------------------------------

def test_getitem_int():
    s = Int32Sequence([10, 20, 30])
    assert s[0] == 10
    assert s[-1] == 30
    assert isinstance(s[0], int)


def test_getitem_out_of_range():
    s = Int32Sequence([1, 2])
    with pytest.raises(IndexError):
        _ = s[5]


def test_getitem_slice_returns_list():
    s = Int32Sequence([1, 2, 3, 4, 5])
    sl = s[1:3]
    assert isinstance(sl, list)
    assert sl == [2, 3]
    sl2 = s[::2]
    assert isinstance(sl2, list)
    assert sl2 == [1, 3, 5]


def test_setitem_int():
    s = Int32Sequence([1, 2, 3])
    s[1] = 99
    assert list(s) == [1, 99, 3]


def test_setitem_slice_sequence():
    s = Int32Sequence([1, 2, 3, 4, 5])
    s[1:4] = Int32Sequence([7, 8, 9])
    assert list(s) == [1, 7, 8, 9, 5]


def test_setitem_slice_numpy():
    s = Int32Sequence([1, 2, 3, 4, 5])
    s[1:4] = np.array([7, 8, 9], dtype=np.int32)
    assert list(s) == [1, 7, 8, 9, 5]


def test_setitem_slice_list():
    s = Int32Sequence([1, 2, 3, 4, 5])
    s[1:4] = [7, 8, 9]
    assert list(s) == [1, 7, 8, 9, 5]


def test_setitem_slice_step():
    s = Int32Sequence([1, 2, 3, 4, 5])
    s[::2] = Int32Sequence([7, 8, 9])
    assert list(s) == [7, 2, 8, 4, 9]


def test_setitem_slice_step_aliasing_self():
    # Source [1,2,3] at positions 0,1,2; writes to 0,2,4. Without a snapshot,
    # position 4 would read the overwritten value 2 instead of the original 3.
    s = Int32Sequence([1, 2, 3, 4, 5, 6])
    s[::2] = s.numpy()[0:3]
    assert list(s) == [1, 2, 2, 4, 3, 6]


def test_setitem_slice_contiguous_aliasing_self():
    # Contiguous overlap (s[1:4] = s[0:3]): numpy uses memmove semantics, so
    # the source is read before the destination is overwritten.
    s = Int32Sequence([1, 2, 3, 4, 5])
    s[1:4] = s.numpy()[0:3]
    assert list(s) == [1, 1, 2, 3, 5]


def test_setitem_slice_negative_step():
    s = Int32Sequence([1, 2, 3, 4, 5])
    s[::-1] = Int32Sequence([5, 4, 3, 2, 1])
    assert list(s) == [1, 2, 3, 4, 5]
    s[4:0:-2] = Int32Sequence([9, 8])
    assert list(s) == [1, 2, 8, 4, 9]


def test_setitem_slice_bool_numpy_path():
    s = BoolSequence([True, False, True, False])
    s[0:2] = BoolSequence([False, True])
    assert list(s) == [False, True, True, False]
    # Strided self-aliasing with bool snapshot: source [T,F,T] at positions
    # 0,1,2; writes to 0,2,4. Without a snapshot, position 4 would read the
    # overwritten value F instead of the original T.
    s = BoolSequence([True, False, True, False, True, False])
    s[::2] = s.numpy()[0:3]
    assert list(s) == [True, False, False, False, True, False]


def test_setitem_slice_length_mismatch():
    s = Int32Sequence([1, 2, 3, 4, 5])
    with pytest.raises(ValueError):
        s[1:4] = [7, 8]


# ---------------------------------------------------------------------------
# Iteration / contains
# ---------------------------------------------------------------------------

def test_iter():
    s = Int32Sequence([1, 2, 3])
    assert list(s) == [1, 2, 3]


def test_iter_keeps_wrapper_alive():
    import gc
    s = Int32Sequence([1, 2, 3])
    it = iter(s)
    del s
    gc.collect()
    assert list(it) == [1, 2, 3]


def test_reversed():
    s = Int32Sequence([1, 2, 3])
    assert list(reversed(s)) == [3, 2, 1]


def test_contains():
    s = Int32Sequence([1, 2, 3])
    assert 2 in s
    assert 9 not in s


# ---------------------------------------------------------------------------
# Mutation
# ---------------------------------------------------------------------------

def test_append():
    s = Int32Sequence([1, 2])
    s.append(3)
    assert list(s) == [1, 2, 3]


def test_extend_sequence():
    s = Int32Sequence([1, 2])
    s.extend(Int32Sequence([3, 4]))
    assert list(s) == [1, 2, 3, 4]


def test_extend_self():
    # s.extend(s) must not dangle the source on reallocation.
    s = Int32Sequence([1, 2, 3, 4])
    s.extend(s)
    assert list(s) == [1, 2, 3, 4, 1, 2, 3, 4]


def test_extend_numpy():
    s = Int32Sequence([1, 2])
    s.extend(np.array([3, 4], dtype=np.int32))
    assert list(s) == [1, 2, 3, 4]


def test_assign_self():
    # s.assign(s) keeps contents (snapshot before clear).
    s = Int32Sequence([1, 2, 3])
    s.assign(s)
    assert list(s) == [1, 2, 3]


def test_from_builtin_self():
    s = Int32Sequence([1, 2, 3])
    s.from_builtin(s)
    assert list(s) == [1, 2, 3]


def test_non_contiguous_numpy_rejected():
    s = Int32Sequence([1, 2, 3, 4, 5, 6])
    arr = np.arange(6, dtype=np.int32)[::2]  # non-contiguous view
    with pytest.raises(TypeError):
        s.extend(arr)
    with pytest.raises(TypeError):
        s[0:3] = arr


def test_negative_resize_rejected():
    s = Int32Sequence([1, 2, 3])
    with pytest.raises(ValueError):
        s.resize(-1)


def test_tuple_slice_assignment():
    s = Int32Sequence([1, 2, 3, 4, 5])
    s[1:4] = (7, 8, 9)
    assert list(s) == [1, 7, 8, 9, 5]


def test_uint64_exact():
    s = UInt64Sequence([2**63, 2**64 - 1])
    assert s[0] == 2**63
    assert s[1] == 2**64 - 1


def test_numpy_keeps_wrapper_alive():
    import gc
    s = Int32Sequence([1, 2, 3])
    arr = s.numpy()
    del s
    gc.collect()
    assert list(arr) == [1, 2, 3]


def test_contains_type_mismatch_returns_false():
    s = BoolSequence([True, False])
    assert 2 not in s  # int in bool sequence -> False, not TypeError


def test_extend_list():
    s = Int32Sequence([1, 2])
    s.extend([3, 4])
    assert list(s) == [1, 2, 3, 4]


def test_insert():
    s = Int32Sequence([1, 3])
    s.insert(1, 2)
    assert list(s) == [1, 2, 3]
    s.insert(0, 0)
    assert list(s) == [0, 1, 2, 3]
    s.insert(99, 9)
    assert list(s) == [0, 1, 2, 3, 9]


def test_pop():
    s = Int32Sequence([1, 2, 3])
    assert s.pop() == 3
    assert list(s) == [1, 2]
    assert s.pop(0) == 1
    assert list(s) == [2]


def test_remove():
    s = Int32Sequence([1, 2, 3, 2])
    s.remove(2)
    assert list(s) == [1, 3, 2]
    with pytest.raises(ValueError):
        s.remove(99)


def test_clear():
    s = Int32Sequence([1, 2, 3])
    s.clear()
    assert len(s) == 0


def test_resize():
    s = Int32Sequence([1, 2, 3])
    s.resize(5)
    assert len(s) == 5
    s.resize(2)
    assert list(s) == [1, 2]


def test_assign():
    s = Int32Sequence([1, 2, 3])
    s.assign([7, 8])
    assert list(s) == [7, 8]


# ---------------------------------------------------------------------------
# Coercion (ADR-003)
# ---------------------------------------------------------------------------

def test_overflow_rejected():
    s = UInt8Sequence()
    with pytest.raises(OverflowError):
        s.append(256)
    with pytest.raises(OverflowError):
        s.append(-1)


def test_float_to_int_rejected():
    s = Int32Sequence()
    with pytest.raises(TypeError):
        s.append(3.5)


def test_aliased_uint8_accepts_char_and_bytes():
    s = UInt8Sequence()
    s.append('c')
    assert s[-1] == 99
    s.append(b'\xFE')
    assert s[-1] == 254
    s[0] = 'A'
    assert s[0] == 65


def test_bool_sequence():
    s = BoolSequence([True, False])
    assert list(s) == [True, False]
    assert s[0] is True
    with pytest.raises(TypeError):
        s.append(1)  # bool-as-int rejected


# ---------------------------------------------------------------------------
# numpy / buffer
# ---------------------------------------------------------------------------

def test_numpy():
    s = Int32Sequence([1, 2, 3])
    arr = s.numpy()
    assert isinstance(arr, np.ndarray)
    assert arr.dtype == np.int32
    assert list(arr) == [1, 2, 3]


def test_numpy_is_live_view():
    s = Int32Sequence([1, 2, 3])
    arr = s.numpy()
    s[0] = 99
    assert arr[0] == 99


def test_buffer_protocol():
    s = Int32Sequence([1, 2, 3])
    mv = memoryview(s)
    assert len(mv) == 3


# ---------------------------------------------------------------------------
# as_builtin / from_builtin
# ---------------------------------------------------------------------------

def test_as_builtin():
    s = Int32Sequence([1, 2, 3])
    assert s.as_builtin() == [1, 2, 3]
    assert isinstance(s.as_builtin(), list)


def test_from_builtin_roundtrip():
    s = Int32Sequence()
    s.from_builtin([1, 2, 3])
    assert s.as_builtin() == [1, 2, 3]


def test_repr():
    assert repr(Int32Sequence([1, 2])) == 'Int32Sequence([1, 2])'

# ---------------------------------------------------------------------------
# Phase 3A: list-semantics methods
# ---------------------------------------------------------------------------

def test_sequence_eq_ne_ordering():
    s = Int32Sequence([1, 2, 3])
    assert s == Int32Sequence([1, 2, 3])
    assert s == [1, 2, 3]
    assert s != Int32Sequence([1, 2, 4])
    assert s != [1, 2]
    assert s < Int32Sequence([1, 2, 4])
    assert s <= [1, 2, 3]
    assert Int32Sequence([1, 2, 4]) > s
    assert [1, 2, 3] >= s


def test_sequence_delitem():
    s = Int32Sequence([0, 1, 2, 3, 4, 5])
    del s[1]
    assert s.as_builtin() == [0, 2, 3, 4, 5]
    del s[1:3]
    assert s.as_builtin() == [0, 4, 5]
    s = Int32Sequence([0, 1, 2, 3, 4, 5])
    del s[::2]
    assert s.as_builtin() == [1, 3, 5]
    s = Int32Sequence([0, 1, 2, 3, 4, 5])
    del s[::-2]
    assert s.as_builtin() == [0, 2, 4]


def test_sequence_concat_repeat():
    s = Int32Sequence([1, 2])
    assert (s + [3, 4]).as_builtin() == [1, 2, 3, 4]
    assert ([0] + s).as_builtin() == [0, 1, 2]
    assert (s * 2).as_builtin() == [1, 2, 1, 2]
    assert (2 * s).as_builtin() == [1, 2, 1, 2]
    s2 = Int32Sequence([1, 2])
    s2 += [3]
    assert s2.as_builtin() == [1, 2, 3]
    s3 = Int32Sequence([1, 2])
    s3 *= 2
    assert s3.as_builtin() == [1, 2, 1, 2]
    s4 = Int32Sequence([1, 2])
    s4 *= 0
    assert s4.as_builtin() == []


def test_sequence_index_count_sort_reverse_copy():
    s = Int32Sequence([3, 1, 2, 1])
    assert s.index(1) == 1
    assert s.count(1) == 2
    s.sort()
    assert s.as_builtin() == [1, 1, 2, 3]
    s.reverse()
    assert s.as_builtin() == [3, 2, 1, 1]
    c = s.copy()
    assert c.as_builtin() == [3, 2, 1, 1]
    assert c is not s
