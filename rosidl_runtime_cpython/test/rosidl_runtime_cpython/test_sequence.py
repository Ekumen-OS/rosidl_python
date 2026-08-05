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

"""Tests for rosidl_runtime_py.experimental.sequence."""


import numpy as np
import pytest

from rosidl_runtime_cpython._raw_buffer import RawBuffer
from rosidl_runtime_cpython.dtype import Dtype
from rosidl_runtime_cpython.sequence import BoundedSequence, Sequence
from rosidl_runtime_cpython.string import BoundedString, String


# ---------------------------------------------------------------------------
# Primitive mode — construction
# ---------------------------------------------------------------------------

def test_primitive_empty():
    s = Sequence(Dtype.INT32)
    assert len(s) == 0
    assert list(s) == []


def test_primitive_dtype_property():
    s = Sequence(Dtype.UINT8)
    assert s.dtype is Dtype.UINT8


def test_primitive_from_buffer_reads_existing_data():
    import struct
    buf = RawBuffer(struct.pack('<ii', 3, 7))
    s = Sequence(Dtype.INT32, buffer=buf)
    # Logical size starts at 0 — buffer is pre-allocated capacity only.
    assert len(s) == 0
    # Capacity is at least 2.
    assert s.capacity >= 2


def test_primitive_bad_buffer_type_raises():
    with pytest.raises(TypeError, match='RawBuffer'):
        Sequence(Dtype.INT32, buffer=b'\x00' * 8)


# ---------------------------------------------------------------------------
# Primitive mode — append / extend
# ---------------------------------------------------------------------------

def test_primitive_append():
    s = Sequence(Dtype.INT32)
    s.append(1)
    s.append(2)
    s.append(3)
    assert len(s) == 3
    assert list(s) == [1, 2, 3]


def test_primitive_extend():
    s = Sequence(Dtype.INT32)
    s.extend([10, 20, 30])
    assert len(s) == 3
    assert list(s) == [10, 20, 30]


def test_primitive_extend_empty():
    s = Sequence(Dtype.INT32)
    s.extend([])
    assert len(s) == 0


# ---------------------------------------------------------------------------
# Primitive mode — insert
# ---------------------------------------------------------------------------

def test_primitive_insert_middle():
    s = Sequence(Dtype.INT32)
    s.extend([1, 3])
    s.insert(1, 2)
    assert list(s) == [1, 2, 3]


def test_primitive_insert_at_start():
    s = Sequence(Dtype.INT32)
    s.extend([2, 3])
    s.insert(0, 1)
    assert list(s) == [1, 2, 3]


def test_primitive_insert_past_end_appends():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2])
    s.insert(100, 3)
    assert list(s) == [1, 2, 3]


# ---------------------------------------------------------------------------
# Primitive mode — pop / remove
# ---------------------------------------------------------------------------

def test_primitive_pop_last():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3])
    v = s.pop()
    assert v == 3
    assert list(s) == [1, 2]


def test_primitive_pop_index():
    s = Sequence(Dtype.INT32)
    s.extend([10, 20, 30])
    v = s.pop(1)
    assert v == 20
    assert list(s) == [10, 30]


def test_primitive_pop_empty_raises():
    with pytest.raises(IndexError):
        Sequence(Dtype.INT32).pop()


def test_primitive_remove():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 1, 3])
    s.remove(1)
    assert list(s) == [2, 1, 3]


def test_primitive_remove_missing_raises():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2])
    with pytest.raises(ValueError):
        s.remove(99)


# ---------------------------------------------------------------------------
# Primitive mode — clear / resize
# ---------------------------------------------------------------------------

def test_primitive_clear():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3])
    s.clear()
    assert len(s) == 0


def test_primitive_resize_grow():
    s = Sequence(Dtype.INT32)
    s.resize(3)
    assert len(s) == 3
    assert list(s) == [0, 0, 0]


def test_primitive_resize_grow_with_fill():
    s = Sequence(Dtype.INT32)
    s.resize(3, fill=7)
    assert list(s) == [7, 7, 7]


def test_primitive_resize_shrink():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3, 4])
    s.resize(2)
    assert list(s) == [1, 2]


# ---------------------------------------------------------------------------
# Primitive mode — getitem / setitem / delitem
# ---------------------------------------------------------------------------

def test_primitive_getitem_int():
    s = Sequence(Dtype.INT32)
    s.extend([10, 20, 30])
    assert s[1] == 20


def test_primitive_getitem_negative():
    s = Sequence(Dtype.INT32)
    s.extend([10, 20, 30])
    assert s[-1] == 30


def test_primitive_getitem_slice():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3, 4, 5])
    assert list(s[1:4]) == [2, 3, 4]


def test_primitive_setitem():
    s = Sequence(Dtype.INT32)
    s.extend([0, 0, 0])
    s[1] = 99
    assert s[1] == 99


def test_primitive_delitem_int():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3])
    del s[1]
    assert list(s) == [1, 3]


def test_primitive_delitem_slice():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3, 4])
    del s[1:3]
    assert list(s) == [1, 4]


def test_primitive_getitem_out_of_range_raises():
    s = Sequence(Dtype.INT32)
    s.append(1)
    with pytest.raises(IndexError):
        _ = s[5]


# ---------------------------------------------------------------------------
# Primitive mode — iter / contains / reversed
# ---------------------------------------------------------------------------

def test_primitive_iter_python_scalars():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3])
    items = list(s)
    assert all(isinstance(x, int) for x in items)
    assert items == [1, 2, 3]


def test_primitive_contains():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3])
    assert 2 in s
    assert 9 not in s


def test_primitive_reversed():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3])
    assert list(reversed(s)) == [3, 2, 1]


# ---------------------------------------------------------------------------
# Primitive mode — equality
# ---------------------------------------------------------------------------

def test_primitive_eq_sequence():
    a = Sequence(Dtype.INT32)
    b = Sequence(Dtype.INT32)
    a.extend([1, 2, 3])
    b.extend([1, 2, 3])
    assert a == b


def test_primitive_eq_list():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2])
    assert s == [1, 2]


def test_primitive_neq_different_size():
    a = Sequence(Dtype.INT32)
    b = Sequence(Dtype.INT32)
    a.extend([1, 2])
    b.extend([1, 2, 3])
    assert a != b


# ---------------------------------------------------------------------------
# Primitive mode — numpy / buffer
# ---------------------------------------------------------------------------

def test_primitive_numpy_view():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2, 3])
    arr = s.numpy()
    assert isinstance(arr, np.ndarray)
    assert list(arr) == [1, 2, 3]


def test_primitive_buffer_protocol():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2])
    mv = memoryview(s)
    assert len(mv) == s._buffer.size


def test_primitive_capacity_property():
    s = Sequence(Dtype.INT32)
    s.append(1)
    assert s.capacity >= 1


# ---------------------------------------------------------------------------
# Object mode — construction
# ---------------------------------------------------------------------------

def test_object_empty():
    s = Sequence(list)
    assert len(s) == 0


def test_object_dtype_property():
    s = Sequence(dict)
    assert s.dtype is dict


def test_object_buffer_raises():
    with pytest.raises(TypeError, match='buffer='):
        Sequence(list, buffer=RawBuffer(16))


# ---------------------------------------------------------------------------
# Object mode — mutation
# ---------------------------------------------------------------------------

def test_object_append():
    s = Sequence(object)
    s.append('hello')
    s.append(42)
    assert len(s) == 2
    assert list(s) == ['hello', 42]


def test_object_extend():
    s = Sequence(object)
    s.extend(['a', 'b', 'c'])
    assert list(s) == ['a', 'b', 'c']


def test_object_insert():
    s = Sequence(object)
    s.extend([1, 3])
    s.insert(1, 2)
    assert list(s) == [1, 2, 3]


def test_object_pop():
    s = Sequence(object)
    s.extend(['x', 'y', 'z'])
    v = s.pop()
    assert v == 'z'
    assert len(s) == 2


def test_object_remove():
    s = Sequence(object)
    s.extend([1, 2, 1])
    s.remove(1)
    assert list(s) == [2, 1]


def test_object_clear():
    s = Sequence(object)
    s.extend([1, 2, 3])
    s.clear()
    assert len(s) == 0


def test_object_resize_grow():
    s = Sequence(object)
    s.resize(3, fill='default')
    assert list(s) == ['default', 'default', 'default']


def test_object_resize_shrink():
    s = Sequence(object)
    s.extend([1, 2, 3, 4])
    s.resize(2)
    assert list(s) == [1, 2]


def test_object_delitem():
    s = Sequence(object)
    s.extend(['a', 'b', 'c'])
    del s[1]
    assert list(s) == ['a', 'c']


def test_object_delitem_slice():
    s = Sequence(object)
    s.extend([0, 1, 2, 3])
    del s[1:3]
    assert list(s) == [0, 3]


def test_object_contains_identity():
    s = Sequence(object)
    obj = object()
    s.append(obj)
    assert obj in s
    assert object() not in s


def test_object_setitem():
    s = Sequence(object)
    s.extend([None, None])
    s[0] = 'replaced'
    assert s[0] == 'replaced'


# ---------------------------------------------------------------------------
# Object mode — numpy / buffer
# ---------------------------------------------------------------------------

def test_object_numpy_returns_object_view():
    s = Sequence(object)
    s.extend([1, 'two', 3.0])
    arr = s.numpy()
    assert arr.dtype == object
    assert list(arr) == [1, 'two', 3.0]


def test_object_buffer_protocol_raises():
    s = Sequence(object)
    with pytest.raises(TypeError, match='__buffer__'):
        memoryview(s)


# ---------------------------------------------------------------------------
# BoundedSequence
# ---------------------------------------------------------------------------

def test_bounded_max_size():
    s = BoundedSequence(Dtype.INT32, 5)
    assert s.max_size == 5


def test_bounded_append_at_limit():
    s = BoundedSequence(Dtype.INT32, 3)
    s.append(1)
    s.append(2)
    s.append(3)
    assert len(s) == 3


def test_bounded_append_over_limit_raises():
    s = BoundedSequence(Dtype.INT32, 2)
    s.append(1)
    s.append(2)
    with pytest.raises(ValueError, match='upper bound'):
        s.append(3)


def test_bounded_extend_over_limit_raises():
    s = BoundedSequence(Dtype.INT32, 2)
    with pytest.raises(ValueError, match='upper bound'):
        s.extend([1, 2, 3])


def test_bounded_resize_over_limit_raises():
    s = BoundedSequence(Dtype.INT32, 2)
    with pytest.raises(ValueError, match='upper bound'):
        s.resize(3)


def test_bounded_insert_over_limit_raises():
    s = BoundedSequence(Dtype.INT32, 1)
    s.append(1)
    with pytest.raises(ValueError, match='upper bound'):
        s.insert(0, 0)


def test_bounded_zero_upper_bound_raises():
    with pytest.raises(ValueError, match='positive integer'):
        BoundedSequence(Dtype.INT32, 0)


def test_bounded_object_mode():
    s = BoundedSequence(object, 2)
    s.append('a')
    s.append('b')
    with pytest.raises(ValueError, match='upper bound'):
        s.append('c')


# ---------------------------------------------------------------------------
# Representation
# ---------------------------------------------------------------------------

def test_repr_sequence():
    s = Sequence(Dtype.INT32)
    s.extend([1, 2])
    r = repr(s)
    assert 'Sequence' in r
    assert 'Dtype.INT32' in r
    assert '[1, 2]' in r


def test_repr_bounded():
    s = BoundedSequence(Dtype.INT32, 10)
    s.append(5)
    r = repr(s)
    assert 'BoundedSequence' in r
    assert '10' in r


# ---------------------------------------------------------------------------
# Primitive mode — external (non-owning) buffer
# ---------------------------------------------------------------------------

def test_primitive_external_append_within_capacity():
    s = Sequence(Dtype.INT32, buffer=RawBuffer(8 * 4, growing=False))  # capacity 8
    assert not s._buffer.growing
    s.append(1)
    s.append(2)
    assert list(s) == [1, 2]


def test_primitive_external_append_over_capacity_raises():
    s = Sequence(Dtype.INT32, buffer=RawBuffer(16, growing=False))  # capacity 4
    s.extend([1, 2, 3, 4])
    with pytest.raises(BufferError, match='capacity'):
        s.append(5)


def test_primitive_external_assign_over_capacity_raises():
    s = Sequence(Dtype.INT32, buffer=RawBuffer(16, growing=False))  # capacity 4
    with pytest.raises(BufferError, match='capacity'):
        s.assign([1, 2, 3, 4, 5])


def test_primitive_external_buffer_protocol_content_length():
    s = Sequence(Dtype.INT32, buffer=RawBuffer(32, growing=False))
    s.append(7)
    mv = memoryview(s)
    assert len(mv) == 4  # one int32, not the 32-byte capacity


# ---------------------------------------------------------------------------
# Object mode — external element pool
# ---------------------------------------------------------------------------

def make_string_pool(n, capacity=16):
    return [String(buffer=RawBuffer(capacity, growing=False)) for _ in range(n)]


def test_pool_capacity_equals_pool_length():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    assert s.capacity == 3
    assert len(s) == 0


def test_pool_bad_type_raises():
    with pytest.raises(TypeError, match='element_pool must be a list'):
        Sequence(String, element_pool=(String(),))  # tuple, not list


def test_pool_element_type_mismatch_raises():
    with pytest.raises(TypeError, match='element_pool entries'):
        Sequence(String, element_pool=['not a string'])


def test_pool_append_preserves_identity():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.append('hello')
    assert s[0] is pool[0]
    assert str(s[0]) == 'hello'
    assert str(pool[0]) == 'hello'


def test_pool_append_overflow_raises():
    pool = make_string_pool(2)
    s = Sequence(String, element_pool=pool)
    s.append('a')
    s.append('b')
    with pytest.raises(BufferError, match='capacity'):
        s.append('c')


def test_pool_assign_deep_copies_into_pool():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['hello', 'world'])
    assert len(s) == 2
    assert s[0] is pool[0]
    assert str(s[0]) == 'hello'
    assert str(s[1]) == 'world'


def test_pool_assign_clears_excess():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b', 'c'])
    s.assign(['x'])
    assert len(s) == 1
    assert str(s[0]) == 'x'
    assert len(pool[1]) == 0
    assert len(pool[2]) == 0


def test_pool_assign_overflow_raises():
    pool = make_string_pool(2)
    s = Sequence(String, element_pool=pool)
    with pytest.raises(BufferError, match='capacity'):
        s.assign(['a', 'b', 'c'])


def test_pool_resize_grow_then_shrink():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.resize(2)
    assert len(s) == 2
    s.resize(3)
    assert len(s) == 3
    s.resize(1)
    assert len(s) == 1
    assert len(pool[1]) == 0
    assert len(pool[2]) == 0


def test_pool_resize_overflow_raises():
    pool = make_string_pool(2)
    s = Sequence(String, element_pool=pool)
    with pytest.raises(BufferError, match='capacity'):
        s.resize(3)


def test_pool_extend():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.extend(['a', 'b'])
    assert list(s) == ['a', 'b']
    assert s[0] is pool[0]


def test_pool_insert_shifts_content():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'c'])
    s.insert(1, 'b')
    assert list(s) == ['a', 'b', 'c']
    assert s[0] is pool[0]
    assert s[1] is pool[1]
    assert s[2] is pool[2]
    assert str(s[1]) == 'b'


def test_pool_setitem_copies_into_pool_element():
    pool = make_string_pool(2)
    s = Sequence(String, element_pool=pool)
    s.assign(['hello', 'world'])
    s[0] = 'goodbye'
    assert s[0] is pool[0]
    assert str(s[0]) == 'goodbye'
    assert str(s[1]) == 'world'


def test_pool_setitem_slice():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b', 'c'])
    s[0:2] = ['x', 'y']
    assert list(s) == ['x', 'y', 'c']
    assert s[0] is pool[0]


def test_pool_setitem_slice_length_mismatch_raises():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b', 'c'])
    with pytest.raises(ValueError, match='slice'):
        s[0:2] = ['x']


def test_pool_setitem_type_mismatch_raises():
    pool = make_string_pool(2)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b'])
    with pytest.raises(TypeError, match='cannot assign'):
        s[0] = 42


def test_pool_delitem_shifts_content():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b', 'c'])
    del s[1]
    assert list(s) == ['a', 'c']
    assert s[0] is pool[0]
    assert s[1] is pool[1]
    assert len(pool[2]) == 0


def test_pool_delitem_slice():
    pool = make_string_pool(5)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b', 'c', 'd', 'e'])
    del s[1:3]
    assert list(s) == ['a', 'd', 'e']
    assert s[0] is pool[0]
    assert s[1] is pool[1]
    assert s[2] is pool[2]
    assert len(pool[3]) == 0
    assert len(pool[4]) == 0


def test_pool_clear_preserves_capacity():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b', 'c'])
    s.clear()
    assert len(s) == 0
    assert s.capacity == 3
    assert len(pool[0]) == 0


def test_pool_identity_after_multiple_mutations():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b', 'c'])
    s[0] = 'x'
    del s[2]
    assert s[0] is pool[0]
    assert s[1] is pool[1]
    assert str(s[0]) == 'x'


def test_pool_bounded_sequence_bound_checked_first():
    pool = make_string_pool(4)
    s = BoundedSequence(String, 2, element_pool=pool)
    s.append('a')
    s.append('b')
    with pytest.raises(ValueError, match='upper bound'):
        s.append('c')  # bound (2) fails before pool capacity (4)


def test_pool_bounded_sequence_pool_capacity_after_bound():
    pool = make_string_pool(2)
    s = BoundedSequence(String, 4, element_pool=pool)
    s.append('a')
    s.append('b')
    with pytest.raises(BufferError, match='capacity'):
        s.append('c')  # bound allows 4, pool only holds 2


def test_pool_bounded_string_elements():
    pool = [BoundedString(10, buffer=RawBuffer(16, growing=False)) for _ in range(2)]
    s = Sequence(BoundedString, element_pool=pool)
    s.append('hello')
    assert s[0] is pool[0]
    assert str(s[0]) == 'hello'
    with pytest.raises(ValueError, match='upper bound'):
        s.append('x' * 11)


def test_pool_bytes_assignment():
    pool = make_string_pool(2)
    s = Sequence(String, element_pool=pool)
    s.resize(2)                       # resize-then-mutate pattern
    s[0:2] = [b'abc', b'def']
    assert str(s[0]) == 'abc'
    assert str(s[1]) == 'def'


def test_pool_pop_returns_deep_copy():
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b', 'c'])
    v = s.pop()                       # last element
    assert str(v) == 'c'
    assert v is not pool[2]           # fresh managed copy, not the pool elem
    assert len(s) == 2
    assert len(pool[2]) == 0          # pool slot cleared by the delete
    v0 = s.pop(0)                     # first element (content-shift path)
    assert str(v0) == 'a'
    assert len(s) == 1
    assert str(s[0]) == 'b'


def test_pool_numpy_view_is_read_only():
    pool = make_string_pool(2)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b'])
    arr = s.numpy()
    with pytest.raises(ValueError, match='read-only'):
        arr[0] = 'x'                  # must not rebind a pool slot
    assert s[0] is pool[0]
    assert str(s[0]) == 'a'


def test_pool_numpy_view_reflects_live_content():
    pool = make_string_pool(2)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b'])
    arr = s.numpy()
    assert arr[0] is pool[0]
    assert list(arr) == s[:]


def test_empty_pool_falls_back_to_managed():
    s = Sequence(String, element_pool=[])
    assert s._element_pool is None
    assert s.capacity == 0
    s.append('managed')               # unbounded: append succeeds
    assert list(s) == ['managed']


def test_pool_resize_grow_cleared_then_reassigned():
    # Shrink must clear pool slots so a later grow never exposes stale data.
    pool = make_string_pool(3)
    s = Sequence(String, element_pool=pool)
    s.assign(['a', 'b', 'c'])
    s.resize(1)
    assert len(pool[1]) == 0
    assert len(pool[2]) == 0
    s.resize(3)
    assert str(s[0]) == 'a'
    assert str(s[1]) == ''
    assert str(s[2]) == ''


def test_pool_primitive_external_resize_zero_fills_new_slots():
    s = Sequence(Dtype.INT32, buffer=RawBuffer(4 * 4, growing=False))  # capacity 4
    s.assign([1, 2])
    s.resize(4)                       # grow on non-owning buffer
    assert list(s) == [1, 2, 0, 0]
