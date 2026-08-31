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

"""Tests for the Phase 1A non-owning reference wrappers (ADR-011).

Covers the test-only .Make() factories (aliasing shared_ptr fixtures),
bounded string/sequence references, fixed arrays, bounds exposure
(is_bounded/max_size), over-bound rejection, write-through to the
underlying C++ storage, and wrapper lifetime.
"""

import gc

import numpy as np
import pytest

from rosidl_runtime_cpython._primitives import (
    Bool,
    BoolArray,
    BoolSequence,
    BoundedBoolSequence,
    BoundedFloat64Sequence,
    BoundedInt32Sequence,
    BoundedString,
    BoundedUInt8Sequence,
    BoundedWString,
    Float32,
    Float64,
    Float64Array,
    Int32,
    Int32Array,
    Int32Sequence,
    Int64,
    String,
    UInt8,
    UInt8Array,
    UInt8Sequence,
    UInt16,
    UInt32,
    UInt64,
    WString,
)


# ---------------------------------------------------------------------------
# Scalar .Make() factories
# ---------------------------------------------------------------------------

def test_scalar_make_constructs_with_value():
    assert UInt32.Make(5).value == 5
    assert Int32.Make(-7).value == -7
    assert Float64.Make(2.5).value == 2.5
    assert Bool.Make(True).value is True


@pytest.mark.parametrize('cls, value', [
    (Bool, True),
    (UInt8, 255),
    (UInt16, 65535),
    (UInt32, 2**32 - 1),
    (UInt64, 2**64 - 1),
    (Int32, -(2**31)),
    (Int64, -(2**63)),
    (Float32, 1.5),
    (Float64, -2.5),
])
def test_scalar_make_all_types(cls, value):
    assert cls.Make(value).value == value


def test_scalar_constructor_equals_make():
    # The Python constructor creates the same fixture as .Make().
    assert UInt32(5).value == UInt32.Make(5).value
    assert String('ab').as_builtin() == String.Make('ab').as_builtin()
    assert UInt8Sequence([1, 2]).as_builtin() == UInt8Sequence.Make([1, 2]).as_builtin()


def test_scalar_make_overflow_rejected():
    from rosidl_runtime_cpython._primitives import Int8
    with pytest.raises(OverflowError):
        UInt8.Make(256)
    with pytest.raises(OverflowError):
        Int8.Make(128)


def test_scalar_make_writes_through_numpy():
    # The wrapper is a non-owning view over the fixture's Scalar<T>; mutating
    # the underlying storage through numpy is visible through the wrapper.
    s = UInt32.Make(7)
    arr = s.numpy()
    arr[0] = 99
    assert s.value == 99


# ---------------------------------------------------------------------------
# String / WString .Make() factories
# ---------------------------------------------------------------------------

def test_string_make_unbounded():
    s = String.Make('hello')
    assert s.as_builtin() == 'hello'
    assert s.is_bounded() is False
    assert s.max_size() == 2**64 - 1


def test_wstring_make_utf16():
    s = WString.Make('héllo')
    assert s.as_builtin() == 'héllo'
    assert s.is_bounded() is False


def test_bounded_string_make():
    s = BoundedString.Make(5, 'hello')
    assert s.as_builtin() == 'hello'
    assert s.is_bounded() is True
    assert s.max_size() == 5


def test_bounded_wstring_make():
    s = BoundedWString.Make(3, 'abc')
    assert s.as_builtin() == 'abc'
    assert s.is_bounded() is True
    assert s.max_size() == 3


def test_bounded_string_over_bound_construction_rejected():
    with pytest.raises(ValueError):
        BoundedString.Make(5, 'toolong')


def test_bounded_string_over_bound_assign_rejected():
    s = BoundedString.Make(5, 'hello')
    with pytest.raises(ValueError):
        s.assign('toolong')


def test_bounded_string_set_within_bound():
    s = BoundedString.Make(5, 'hello')
    s.assign('world')
    assert s.as_builtin() == 'world'


def test_bounded_string_bad_bound_rejected():
    with pytest.raises(ValueError):
        BoundedString.Make(0, 'x')  # bound 0 is unbounded-only via String.Make
    with pytest.raises(ValueError):
        BoundedString.Make(10, 'x')


# ---------------------------------------------------------------------------
# Sequence .Make() factories
# ---------------------------------------------------------------------------

def test_sequence_make_unbounded():
    s = UInt8Sequence.Make([1, 2, 3])
    assert s.as_builtin() == [1, 2, 3]
    assert s.is_bounded() is False
    assert s.max_size() == 2**64 - 1


def test_sequence_make_grows_freely():
    s = UInt8Sequence.Make([1])
    s.append(2)
    s.extend([3, 4])
    assert s.as_builtin() == [1, 2, 3, 4]


def test_bounded_sequence_make():
    s = BoundedUInt8Sequence.Make(3, [1, 2, 3])
    assert s.as_builtin() == [1, 2, 3]
    assert s.is_bounded() is True
    assert s.max_size() == 3


def test_bounded_sequence_over_bound_append_rejected():
    s = BoundedUInt8Sequence.Make(3, [1, 2, 3])
    with pytest.raises(ValueError):
        s.append(4)


def test_bounded_sequence_over_bound_extend_rejected():
    s = BoundedInt32Sequence.Make(2, [1, 2])
    with pytest.raises(ValueError):
        s.extend([3])


def test_bounded_sequence_bad_bound_rejected():
    with pytest.raises(ValueError):
        BoundedUInt8Sequence.Make(0, [])
    with pytest.raises(ValueError):
        BoundedUInt8Sequence.Make(10, [])


def test_bounded_sequence_all_element_types():
    assert BoundedBoolSequence.Make(2, [True, False]).as_builtin() == [True, False]
    assert BoundedFloat64Sequence.Make(2, [1.5, 2.5]).as_builtin() == [1.5, 2.5]


def test_sequence_make_writes_through_numpy():
    s = Int32Sequence.Make([1, 2, 3])
    arr = s.numpy()
    arr[1] = 99
    assert s.as_builtin() == [1, 99, 3]


# ---------------------------------------------------------------------------
# Arrays
# ---------------------------------------------------------------------------

def test_array_make_sizes_1_to_9():
    for n in range(1, 10):
        a = UInt8Array.Make(n, list(range(n)))
        assert a.size() == n
        assert a.as_builtin() == list(range(n))


def test_array_make_bad_size_rejected():
    with pytest.raises(ValueError):
        UInt8Array.Make(0, [])
    with pytest.raises(ValueError):
        UInt8Array.Make(10, [])


def test_array_make_wrong_value_count_rejected():
    with pytest.raises(ValueError):
        UInt8Array.Make(4, [1, 2, 3])


def test_array_getitem():
    a = Int32Array.Make(4, [10, 20, 30, 40])
    # Scalar indexing returns a Python builtin (numpy semantics are opt-in via
    # .numpy() / the buffer protocol).
    assert a[0] == 10
    assert a[-1] == 40
    assert isinstance(a[0], int)
    assert not isinstance(a[0], np.integer)
    # Slices return lists of builtins.
    sl = a[1:3]
    assert isinstance(sl, list)
    assert sl == [20, 30]
    assert a[::2] == [10, 30]


def test_array_setitem():
    a = Int32Array.Make(4, [1, 2, 3, 4])
    a[1] = 99
    assert a.as_builtin() == [1, 99, 3, 4]
    a[0:2] = [7, 8]
    assert a.as_builtin() == [7, 8, 3, 4]


def test_array_oob_raises_index_error():
    a = UInt8Array.Make(4, [1, 2, 3, 4])
    with pytest.raises(IndexError):
        a[100]
    with pytest.raises(IndexError):
        a[100] = 1


def test_array_iteration():
    a = UInt8Array.Make(3, [1, 2, 3])
    assert list(a) == [1, 2, 3]
    assert list(reversed(a)) == [3, 2, 1]
    assert 2 in a
    assert 9 not in a


def test_array_numpy_and_buffer():
    a = UInt8Array.Make(4, [1, 2, 3, 4])
    arr = a.numpy()
    assert isinstance(arr, np.ndarray)
    assert arr.dtype == np.uint8
    assert arr.tolist() == [1, 2, 3, 4]
    assert bytes(memoryview(a)) == bytes([1, 2, 3, 4])


def test_array_from_builtin():
    a = UInt8Array.Make(4, [0, 0, 0, 0])
    a.from_builtin([5, 6, 7, 8])
    assert a.as_builtin() == [5, 6, 7, 8]
    with pytest.raises(ValueError):
        a.from_builtin([1, 2])


def test_array_float_and_bool():
    assert Float64Array.Make(2, [1.5, 2.5]).as_builtin() == [1.5, 2.5]
    assert BoolArray.Make(2, [True, False]).as_builtin() == [True, False]


def test_array_writes_through_numpy():
    a = UInt8Array.Make(4, [1, 2, 3, 4])
    arr = a.numpy()
    arr[0] = 99
    assert a.as_builtin() == [99, 2, 3, 4]


def test_array_indexing_numpy_semantics():
    # Documented design (ADR-003 amendment): numpy semantics apply ONLY to
    # same-dtype numpy-array sources on slice assignment. Different-dtype
    # arrays and list/scalar sources are strict (lossy float->int rejected);
    # reads return Python builtins/lists.
    a = Int32Array.Make(2, [0, 0])
    with pytest.raises(TypeError):
        a[0] = 1.9  # strict: lossy float to integer rejected
    a[0] = 1
    assert a[0] == 1
    with pytest.raises(ValueError):
        a[:] = [7]  # strict: length must match exactly (no broadcasting)
    a[:] = [7, 8]
    assert a.as_builtin() == [7, 8]
    # A different-dtype numpy array is rejected (no implicit truncation); the
    # user casts explicitly to get numpy semantics.
    with pytest.raises(TypeError):
        a[:] = np.array([1.9, 2.9], dtype=np.float64)
    a[:] = np.array([1.9, 2.9], dtype=np.float64).astype(np.int32)
    assert a.as_builtin() == [1, 2]
    # A same-dtype numpy-array source gets numpy semantics (broadcasting).
    a[:] = np.array([9], dtype=np.int32)  # numpy broadcasts a length-1 source
    assert a.as_builtin() == [9, 9]
    # Scalar indexing returns a Python builtin; iteration yields builtins.
    assert isinstance(a[0], int)
    assert all(isinstance(x, int) for x in list(a))


# ---------------------------------------------------------------------------
# Lifetime (aliasing shared_ptr fixtures, ADR-011)
# ---------------------------------------------------------------------------

def test_fixture_wrapper_destroyed_cleanly():
    w = UInt32.Make(5)
    del w
    gc.collect()


def test_numpy_view_keeps_fixture_alive():
    s = UInt32.Make(7)
    arr = s.numpy()
    del s
    gc.collect()
    assert arr[0] == 7


def test_sequence_numpy_view_keeps_fixture_alive():
    s = UInt8Sequence.Make([1, 2, 3])
    arr = s.numpy()
    del s
    gc.collect()
    assert arr.tolist() == [1, 2, 3]


def test_array_numpy_view_keeps_fixture_alive():
    a = UInt8Array.Make(3, [1, 2, 3])
    arr = a.numpy()
    del a
    gc.collect()
    assert arr.tolist() == [1, 2, 3]


def test_string_numpy_view_keeps_fixture_alive():
    s = String.Make('abc')
    arr = s.numpy()
    del s
    gc.collect()
    assert arr.tolist() == [ord('a'), ord('b'), ord('c')]


def test_string_slice_valid_utf8():
    # Slices return str (builtin-return policy). Valid UTF-8 slices work.
    s = String.Make('café')
    assert s[0:2] == 'ca'
    assert s[:] == 'café'


def test_string_non_utf8_bytes_use_buffer():
    # Known limitation (documented): str-return operations (slices, __str__,
    # as_builtin) decode as UTF-8 and raise UnicodeDecodeError on non-UTF-8
    # byte content. Byte-level access is preserved via buffer/numpy, which
    # expose the raw code units.
    s = String.Make('café')
    assert bytes(memoryview(s)) == b'caf\xc3\xa9'
    assert s.numpy().tolist() == [ord('c'), ord('a'), ord('f'), 0xC3, 0xA9]
    with pytest.raises(UnicodeDecodeError):
        s[3:4]  # lone leading byte of the é sequence is not valid UTF-8


def test_iterator_keeps_fixture_alive():
    s = UInt8Sequence.Make([1, 2, 3])
    it = iter(s)
    del s
    gc.collect()
    assert list(it) == [1, 2, 3]