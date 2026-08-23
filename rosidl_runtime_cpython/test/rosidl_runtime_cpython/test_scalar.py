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

"""Tests for rosidl_runtime_py.experimental.scalar."""

import numpy as np
import pytest

from rosidl_runtime_cpython._raw_buffer import RawBuffer
from rosidl_runtime_cpython.dtype import Dtype
from rosidl_runtime_cpython.scalar import Scalar


# ---------------------------------------------------------------------------
# RawBuffer — indexing
# ---------------------------------------------------------------------------

def test_rawbuffer_len():
    buf = RawBuffer(4)
    assert len(buf) == 4


def test_rawbuffer_getitem():
    buf = RawBuffer(3)
    buf[0] = 0xAA
    buf[1] = 0xBB
    buf[2] = 0xCC
    assert buf[0] == 0xAA
    assert buf[1] == 0xBB
    assert buf[2] == 0xCC


def test_rawbuffer_negative_index():
    buf = RawBuffer(3)
    buf[2] = 0xFF
    assert buf[-1] == 0xFF


def test_rawbuffer_index_out_of_range_raises():
    buf = RawBuffer(2)
    with pytest.raises(IndexError):
        _ = buf[5]


def test_rawbuffer_setitem_out_of_range_raises():
    buf = RawBuffer(2)
    with pytest.raises(IndexError):
        buf[5] = 0


def test_rawbuffer_setitem_bad_value_raises():
    buf = RawBuffer(1)
    with pytest.raises(ValueError):
        buf[0] = 256
    with pytest.raises(ValueError):
        buf[0] = -1


def test_rawbuffer_setitem_bad_type_raises():
    buf = RawBuffer(1)
    with pytest.raises(TypeError):
        buf[0] = 'x'


# ---------------------------------------------------------------------------
# Construction
# ---------------------------------------------------------------------------

def test_default_value_is_zero():
    s = Scalar(Dtype.INT32)
    assert s.value == 0


@pytest.mark.parametrize('dtype, value', [
    (Dtype.INT32, 42),
    (Dtype.FLOAT64, 3.14),
    (Dtype.BOOL, True),
    (Dtype.UINT8, 255),
    (Dtype.INT64, -(2**62)),
])
def test_initial_value(dtype, value):
    s = Scalar(dtype, value)
    if dtype in (Dtype.FLOAT32, Dtype.FLOAT64, Dtype.LONG_DOUBLE):
        assert abs(s.value - value) < 1e-5
    else:
        assert s.value == value


def test_dtype_property():
    s = Scalar(Dtype.UINT16, 7)
    assert s.dtype is Dtype.UINT16


def test_construction_from_buffer():
    buf = RawBuffer(4)
    s = Scalar(Dtype.INT32, buffer=buf)
    assert s.dtype is Dtype.INT32
    # buffer is re-used, not copied
    assert s._buffer is buf


def test_buffer_ignores_value_argument():
    buf = RawBuffer(4)
    # Write little-endian 99 (0x63000000) directly via indexing.
    buf[0] = 99
    buf[1] = 0
    buf[2] = 0
    buf[3] = 0
    s = Scalar(Dtype.INT32, 0, buffer=buf)
    # The value in the buffer (99) is preserved; the value= arg is ignored.
    assert s.value == 99


# ---------------------------------------------------------------------------
# Value access
# ---------------------------------------------------------------------------

def test_value_setter():
    s = Scalar(Dtype.INT32, 0)
    s.value = 123
    assert s.value == 123


def test_value_setter_clamps_for_integer_types():
    # numpy silently wraps overflow for integer types.
    s = Scalar(Dtype.UINT8, 0)
    s.value = 256   # wraps to 0
    assert s.value == 0


# ---------------------------------------------------------------------------
# Numeric protocol
# ---------------------------------------------------------------------------

def test_int_conversion():
    s = Scalar(Dtype.INT32, 7)
    assert int(s) == 7
    assert isinstance(int(s), int)


def test_float_conversion():
    s = Scalar(Dtype.FLOAT64, 2.5)
    assert float(s) == 2.5
    assert isinstance(float(s), float)


def test_bool_conversion_false():
    s = Scalar(Dtype.BOOL, False)
    assert not bool(s)


def test_bool_conversion_true():
    s = Scalar(Dtype.INT32, 1)
    assert bool(s)


def test_index_for_integer_dtype():
    s = Scalar(Dtype.INT32, 3)
    lst = [0, 1, 2, 3, 4]
    assert lst[s] == 3


def test_index_raises_for_float_dtype():
    s = Scalar(Dtype.FLOAT64, 1.0)
    with pytest.raises(TypeError):
        _ = [0][s]


# ---------------------------------------------------------------------------
# Equality and hashing
# ---------------------------------------------------------------------------

def test_eq_same_dtype_same_value():
    assert Scalar(Dtype.INT32, 5) == Scalar(Dtype.INT32, 5)


def test_eq_same_dtype_different_value():
    assert Scalar(Dtype.INT32, 5) != Scalar(Dtype.INT32, 6)


def test_eq_different_dtype():
    # UINT8 and INT8 with value 1 should compare unequal (dtype differs).
    assert Scalar(Dtype.UINT8, 1) != Scalar(Dtype.INT8, 1)


def test_eq_with_python_scalar():
    s = Scalar(Dtype.INT32, 42)
    assert s == 42
    assert 42 == s


def test_hash_equal_scalars():
    a = Scalar(Dtype.INT32, 10)
    b = Scalar(Dtype.INT32, 10)
    assert hash(a) == hash(b)


def test_hash_in_set():
    s = {Scalar(Dtype.INT32, 1), Scalar(Dtype.INT32, 1), Scalar(Dtype.INT32, 2)}
    assert len(s) == 2


# ---------------------------------------------------------------------------
# numpy / buffer protocols
# ---------------------------------------------------------------------------

def test_numpy_returns_length_1_array():
    s = Scalar(Dtype.INT32, 7)
    arr = s.numpy()
    assert isinstance(arr, np.ndarray)
    assert arr.shape == (1,)
    assert arr[0] == 7


def test_numpy_is_live_view():
    s = Scalar(Dtype.INT32, 0)
    arr = s.numpy()
    s.value = 99
    assert arr[0] == 99


def test_numpy_mutation_reflected_in_scalar():
    s = Scalar(Dtype.INT32, 0)
    s.numpy()[0] = 55
    assert s.value == 55


def test_buffer_protocol():
    s = Scalar(Dtype.INT32, 0)
    mv = memoryview(s)
    assert len(mv) == Dtype.INT32.itemsize


# ---------------------------------------------------------------------------
# Error cases
# ---------------------------------------------------------------------------

def test_non_dtype_raises():
    with pytest.raises(TypeError, match='dtype must be a Dtype'):
        Scalar('int32')


def test_bad_buffer_type_raises():
    with pytest.raises(TypeError, match='buffer must be a RawBuffer'):
        Scalar(Dtype.INT32, buffer=b'\x00\x00\x00\x00')


def test_undersized_buffer_raises():
    with pytest.raises(ValueError, match='buffer size'):
        Scalar(Dtype.INT32, buffer=RawBuffer(2))


# ---------------------------------------------------------------------------
# Representation
# ---------------------------------------------------------------------------

def test_repr():
    s = Scalar(Dtype.INT32, 7)
    assert repr(s) == 'Scalar(Dtype.INT32, 7)'


def test_str():
    s = Scalar(Dtype.FLOAT64, 1.5)
    assert str(s) == '1.5'


# ---------------------------------------------------------------------------
# Binary arithmetic
# ---------------------------------------------------------------------------

def test_add_scalar():
    r = Scalar(Dtype.INT32, 5) + Scalar(Dtype.INT32, 3)
    assert isinstance(r, Scalar)
    assert r.dtype is Dtype.INT32
    assert r.value == 8


def test_add_python_int():
    r = Scalar(Dtype.INT32, 5) + 3
    assert isinstance(r, Scalar)
    assert r.dtype is Dtype.INT32
    assert r.value == 8


def test_radd_python_int():
    r = 3 + Scalar(Dtype.INT32, 5)
    assert isinstance(r, Scalar)
    assert r.dtype is Dtype.INT32
    assert r.value == 8


def test_sub():
    assert (Scalar(Dtype.INT32, 5) - 3).value == 2
    assert (3 - Scalar(Dtype.INT32, 5)).value == -2


def test_mul():
    assert (Scalar(Dtype.INT32, 5) * 3).value == 15
    assert (3 * Scalar(Dtype.INT32, 5)).value == 15


def test_truediv_promotes_to_float():
    r = Scalar(Dtype.INT32, 5) / 2
    assert isinstance(r, Scalar)
    assert r.dtype is Dtype.FLOAT64
    assert r.value == 2.5


def test_rtruediv():
    r = 6 / Scalar(Dtype.INT32, 2)
    assert r.dtype is Dtype.FLOAT64
    assert r.value == 3.0


def test_floordiv():
    assert (Scalar(Dtype.INT32, 5) // 2).value == 2
    assert (5 // Scalar(Dtype.INT32, 2)).value == 2


def test_mod():
    assert (Scalar(Dtype.INT32, 5) % 2).value == 1
    assert (5 % Scalar(Dtype.INT32, 2)).value == 1


def test_pow():
    assert (Scalar(Dtype.INT32, 2) ** 10).value == 1024
    assert (2 ** Scalar(Dtype.INT32, 3)).value == 8


def test_pow_negative_exponent_promotes_to_float():
    # Python: 2 ** -1 == 0.5 (numpy would raise for int ** negative).
    r = Scalar(Dtype.INT32, 2) ** -1
    assert r.dtype is Dtype.FLOAT64
    assert r.value == 0.5


def test_rpow_negative_exponent_promotes_to_float():
    r = 2 ** Scalar(Dtype.INT32, -1)
    assert r.dtype is Dtype.FLOAT64
    assert r.value == 0.5


def test_pow_complex_exponent_raises_cleanly():
    # Complex has no ROS dtype; the result must be a TypeError, not a crash
    # in the negative-exponent check.
    with pytest.raises(TypeError, match='unsupported result dtype'):
        Scalar(Dtype.INT32, 2) ** complex(1, 0)


def test_float_arithmetic():
    r = Scalar(Dtype.FLOAT64, 2.5) + 1.5
    assert r.dtype is Dtype.FLOAT64
    assert r.value == 4.0


def test_bool_arithmetic_numpy_semantics():
    # numpy: bool + bool is logical OR (not Python's int promotion).
    r = Scalar(Dtype.BOOL, True) + Scalar(Dtype.BOOL, True)
    assert r.dtype is Dtype.BOOL
    assert r.value is True


def test_bool_plus_int_promotes():
    # numpy promotes bool + Python int to the default integer dtype (int64).
    r = Scalar(Dtype.BOOL, True) + 1
    assert r.dtype is Dtype.INT64
    assert r.value == 2


def test_result_is_new_scalar():
    a = Scalar(Dtype.INT32, 5)
    b = a + 3
    assert a.value == 5  # original unchanged
    assert b.value == 8


# ---------------------------------------------------------------------------
# Type promotion
# ---------------------------------------------------------------------------

def test_promotion_uint8_int32():
    r = Scalar(Dtype.UINT8, 5) + Scalar(Dtype.INT32, 3)
    assert r.dtype is Dtype.INT32
    assert r.value == 8


def test_promotion_int32_float64():
    r = Scalar(Dtype.INT32, 5) + 3.0
    assert r.dtype is Dtype.FLOAT64
    assert r.value == 8.0


def test_promotion_uint8_overflow_wraps():
    # numpy wraps unsigned overflow (C semantics).
    r = Scalar(Dtype.UINT8, 200) + Scalar(Dtype.UINT8, 100)
    assert r.dtype is Dtype.UINT8
    assert r.value == 44


def test_promotion_char_to_uint8():
    # Arithmetic on a char scalar yields the canonical numeric dtype.
    r = Scalar(Dtype.CHAR, 65) + 1
    assert r.dtype is Dtype.UINT8
    assert r.value == 66


# ---------------------------------------------------------------------------
# Bitwise arithmetic
# ---------------------------------------------------------------------------

def test_bitwise_and():
    assert (Scalar(Dtype.INT32, 12) & 10).value == 8
    assert (10 & Scalar(Dtype.INT32, 12)).value == 8


def test_bitwise_or():
    assert (Scalar(Dtype.INT32, 12) | 3).value == 15
    assert (3 | Scalar(Dtype.INT32, 12)).value == 15


def test_bitwise_xor():
    assert (Scalar(Dtype.INT32, 12) ^ 10).value == 6
    assert (10 ^ Scalar(Dtype.INT32, 12)).value == 6


def test_lshift():
    assert (Scalar(Dtype.INT32, 5) << 2).value == 20
    assert (5 << Scalar(Dtype.INT32, 2)).value == 20


def test_rshift():
    assert (Scalar(Dtype.INT32, 20) >> 2).value == 5
    assert (20 >> Scalar(Dtype.INT32, 2)).value == 5


def test_invert():
    assert (~Scalar(Dtype.INT32, 5)).value == -6


def test_bitwise_on_float_raises():
    with pytest.raises(TypeError):
        Scalar(Dtype.FLOAT64, 1.5) & 1


# ---------------------------------------------------------------------------
# Unary arithmetic
# ---------------------------------------------------------------------------

def test_neg():
    r = -Scalar(Dtype.INT32, 5)
    assert isinstance(r, Scalar)
    assert r.value == -5


def test_pos():
    assert (+Scalar(Dtype.INT32, 5)).value == 5


def test_abs():
    r = abs(Scalar(Dtype.INT32, -5))
    assert isinstance(r, Scalar)
    assert r.value == 5


def test_abs_float():
    assert abs(Scalar(Dtype.FLOAT64, -2.5)).value == 2.5


# ---------------------------------------------------------------------------
# In-place arithmetic
# ---------------------------------------------------------------------------

def test_iadd_returns_self():
    s = Scalar(Dtype.INT32, 5)
    r = s.__iadd__(3)
    assert r is s
    assert s.value == 8


def test_iadd_python_syntax():
    s = Scalar(Dtype.INT32, 5)
    s += 3
    assert s.value == 8


def test_iadd_promotes_dtype():
    s = Scalar(Dtype.INT32, 5)
    s += 0.5
    assert s.dtype is Dtype.FLOAT64
    assert s.value == 5.5


def test_isub():
    s = Scalar(Dtype.INT32, 5)
    s -= 2
    assert s.value == 3


def test_imul():
    s = Scalar(Dtype.INT32, 5)
    s *= 3
    assert s.value == 15


def test_itruediv():
    s = Scalar(Dtype.INT32, 5)
    s /= 2
    assert s.dtype is Dtype.FLOAT64
    assert s.value == 2.5


def test_ifloordiv():
    s = Scalar(Dtype.INT32, 5)
    s //= 2
    assert s.value == 2


def test_imod():
    s = Scalar(Dtype.INT32, 5)
    s %= 2
    assert s.value == 1


def test_ipow():
    s = Scalar(Dtype.INT32, 2)
    s **= 10
    assert s.value == 1024


def test_ipow_negative_exponent_promotes():
    s = Scalar(Dtype.INT32, 2)
    s **= -1
    assert s.dtype is Dtype.FLOAT64
    assert s.value == 0.5


def test_iand():
    s = Scalar(Dtype.INT32, 12)
    s &= 10
    assert s.value == 8


def test_ior():
    s = Scalar(Dtype.INT32, 12)
    s |= 3
    assert s.value == 15


def test_ixor():
    s = Scalar(Dtype.INT32, 12)
    s ^= 10
    assert s.value == 6


def test_ilshift():
    s = Scalar(Dtype.INT32, 5)
    s <<= 2
    assert s.value == 20


def test_irshift():
    s = Scalar(Dtype.INT32, 20)
    s >>= 2
    assert s.value == 5


def test_inplace_with_scalar_operand():
    s = Scalar(Dtype.INT32, 5)
    s += Scalar(Dtype.INT32, 3)
    assert s.value == 8


# ---------------------------------------------------------------------------
# In-place arithmetic — external (non-owning) backing
# ---------------------------------------------------------------------------

def test_external_inplace_same_dtype_writes_through():
    # In-place ops on an externally-backed scalar must write through to the
    # external RawBuffer, preserving the zero-copy aliasing.
    buf = RawBuffer(4, growing=False)
    s = Scalar(Dtype.INT32, buffer=buf)
    s += 3
    assert s._buffer is buf
    assert s.value == 3
    assert bytes(buf) == b'\x03\x00\x00\x00'


def test_external_inplace_all_ops_write_through():
    buf = RawBuffer(4, growing=False)
    s = Scalar(Dtype.INT32, buffer=buf)
    s += 10
    s -= 3
    s *= 2
    s //= 3
    s %= 3
    s |= 4
    s &= 3
    s ^= 2
    s <<= 2
    s >>= 1
    assert s._buffer is buf
    assert s.value == 6
    assert bytes(buf) == b'\x06\x00\x00\x00'


def test_external_inplace_promotion_raises():
    # A dtype-promoting in-place op cannot fit in the external buffer; it must
    # raise rather than silently swap in managed storage (which would break
    # the aliasing with the C/C++ side).
    buf = RawBuffer(4, growing=False)
    s = Scalar(Dtype.INT32, buffer=buf)
    with pytest.raises(BufferError, match='external'):
        s += 0.5
    # The scalar still aliases the external buffer, unchanged.
    assert s._buffer is buf
    assert s.dtype is Dtype.INT32
    assert s.value == 0


def test_external_inplace_promotion_raises_for_division():
    buf = RawBuffer(4, growing=False)
    s = Scalar(Dtype.INT32, buffer=buf)
    with pytest.raises(BufferError, match='external'):
        s /= 2  # int / int promotes to float64
    assert s._buffer is buf


def test_managed_inplace_promotion_still_works():
    # Managed (owning) scalars may still promote dtype in place.
    s = Scalar(Dtype.INT32, 5)
    s += 0.5
    assert s.dtype is Dtype.FLOAT64
    assert s.value == 5.5


# ---------------------------------------------------------------------------
# Arithmetic error cases
# ---------------------------------------------------------------------------

def test_add_unsupported_type_raises():
    with pytest.raises(TypeError):
        Scalar(Dtype.INT32, 5) + 'x'


def test_add_unsupported_type_reverse_raises():
    with pytest.raises(TypeError):
        'x' + Scalar(Dtype.INT32, 5)
