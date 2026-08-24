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

"""Tests for the native statically typed scalar wrappers (Phase 1).

Ported from test_scalar.py to the new typed-wrapper API (ADR-002/003/004).
"""

import numpy as np
import pytest

from rosidl_runtime_cpython.dtype import Dtype
from rosidl_runtime_cpython.scalar import (
    Bool,
    Char,
    Float32,
    Float64,
    Int8,
    Int16,
    Int32,
    Int64,
    LongDouble,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
)


# ---------------------------------------------------------------------------
# Construction
# ---------------------------------------------------------------------------

def test_default_value_is_zero():
    assert Int32().value == 0


@pytest.mark.parametrize('cls, value', [
    (Int32, 42),
    (Float64, 3.14),
    (Bool, True),
    (UInt8, 255),
    (Int64, -(2**62)),
])
def test_initial_value(cls, value):
    s = cls(value)
    if cls in (Float32, Float64, LongDouble):
        assert abs(s.value - value) < 1e-5
    else:
        assert s.value == value


def test_dtype_property():
    assert UInt16(7).dtype is Dtype.UINT16
    assert Int32(7).dtype is Dtype.INT32
    assert Bool(True).dtype is Dtype.BOOL
    assert Float64(1.0).dtype is Dtype.FLOAT64


# ---------------------------------------------------------------------------
# Aliased UInt8 (char/octet/byte) — ADR-004
# ---------------------------------------------------------------------------

def test_aliases_are_same_class():
    assert Char is UInt8


def test_uint8_accepts_ascii_char():
    assert UInt8('c').value == 99


def test_uint8_accepts_single_byte():
    assert UInt8(b'\xFE').value == 254
    assert UInt8(bytearray([7])).value == 7


def test_uint8_rejects_multi_char_str():
    with pytest.raises(ValueError):
        UInt8('ab')


def test_uint8_rejects_non_ascii_char():
    with pytest.raises(ValueError):
        UInt8('\u00FE')  # 254, but not ASCII


def test_uint8_rejects_multi_byte():
    with pytest.raises(ValueError):
        UInt8(b'\xFE\xFF')


# ---------------------------------------------------------------------------
# Value access + coercion (ADR-003)
# ---------------------------------------------------------------------------

def test_value_setter():
    s = Int32(0)
    s.value = 123
    assert s.value == 123


def test_overflow_rejected():
    with pytest.raises(OverflowError):
        UInt8(256)
    with pytest.raises(OverflowError):
        Int8(128)
    with pytest.raises(OverflowError):
        UInt8(-1)


def test_negative_to_unsigned_rejected():
    with pytest.raises(OverflowError):
        UInt8(-1)


def test_float_to_int_rejected():
    with pytest.raises(TypeError):
        Int32(3.5)


def test_bool_as_int_rejected():
    with pytest.raises(TypeError):
        Int32(True)


def test_int_as_bool_rejected():
    with pytest.raises(TypeError):
        Bool(1)


def test_float32_range():
    assert Float32(3.5).value == pytest.approx(3.5)
    with pytest.raises(OverflowError):
        Float32(1e39)  # > FLT_MAX


def test_float_accepts_nan_inf():
    assert np.isnan(Float64(float('nan')).value)
    assert np.isinf(Float64(float('inf')).value)


def test_unsupported_type_rejected():
    with pytest.raises(TypeError):
        Int32('x')


# ---------------------------------------------------------------------------
# as_builtin / from_builtin
# ---------------------------------------------------------------------------

def test_as_builtin_scalars():
    assert Int32(7).as_builtin() == 7
    assert isinstance(Int32(7).as_builtin(), int)
    assert Float64(2.5).as_builtin() == 2.5
    assert Bool(True).as_builtin() is True
    assert UInt8(200).as_builtin() == 200  # canonical int, not str


def test_from_builtin_roundtrip():
    for cls, v in [(Int32, 7), (Float64, 2.5), (Bool, True), (UInt8, 200)]:
        s = cls()
        s.from_builtin(v)
        assert s.as_builtin() == v


def test_from_builtin_accepts_aliased_forms():
    s = UInt8()
    s.from_builtin('c')
    assert s.value == 99
    s.from_builtin(b'\xFE')
    assert s.value == 254


# ---------------------------------------------------------------------------
# Numeric protocol
# ---------------------------------------------------------------------------

def test_int_conversion():
    assert int(Int32(7)) == 7
    assert isinstance(int(Int32(7)), int)


def test_float_conversion():
    assert float(Float64(2.5)) == 2.5


def test_bool_conversion():
    assert not bool(Bool(False))
    assert bool(Int32(1))


def test_index_for_integer_dtype():
    lst = [0, 1, 2, 3, 4]
    assert lst[Int32(3)] == 3


def test_index_raises_for_float_dtype():
    with pytest.raises(TypeError):
        _ = [0][Float64(1.0)]


# ---------------------------------------------------------------------------
# Equality and hashing
# ---------------------------------------------------------------------------

def test_eq_same_type_same_value():
    assert Int32(5) == Int32(5)


def test_eq_same_type_different_value():
    assert Int32(5) != Int32(6)


def test_eq_different_type():
    # Value-based comparison whenever a promotion rule exists (user decision):
    # UInt8(1) == Int8(1) is True (values promote and compare equal).
    assert UInt8(1) == Int8(1)
    assert UInt8(1) != Int8(2)


def test_eq_with_python_scalar():
    assert Int32(42) == 42
    assert 42 == Int32(42)


def test_hash_equal_scalars():
    assert hash(Int32(10)) == hash(Int32(10))


def test_hash_in_set():
    s = {Int32(1), Int32(1), Int32(2)}
    assert len(s) == 2


# ---------------------------------------------------------------------------
# numpy / buffer protocols
# ---------------------------------------------------------------------------

def test_numpy_returns_length_1_array():
    arr = Int32(7).numpy()
    assert isinstance(arr, np.ndarray)
    assert arr.shape == (1,)
    assert arr[0] == 7


def test_numpy_is_live_view():
    s = Int32(0)
    arr = s.numpy()
    s.value = 99
    assert arr[0] == 99


def test_numpy_mutation_reflected_in_scalar():
    s = Int32(0)
    s.numpy()[0] = 55
    assert s.value == 55


def test_buffer_protocol():
    mv = memoryview(Int32(0))
    assert len(mv) == 4


# ---------------------------------------------------------------------------
# Representation
# ---------------------------------------------------------------------------

def test_repr():
    assert repr(Int32(7)) == 'Int32(7)'


def test_str():
    assert str(Float64(1.5)) == '1.5'


# ---------------------------------------------------------------------------
# Binary arithmetic
# ---------------------------------------------------------------------------

def test_add_scalar():
    r = Int32(5) + Int32(3)
    assert isinstance(r, Int32)
    assert r.value == 8


def test_add_python_int():
    r = Int32(5) + 3
    assert isinstance(r, Int32)
    assert r.value == 8


def test_radd_python_int():
    r = 3 + Int32(5)
    assert isinstance(r, Int32)
    assert r.value == 8


def test_sub():
    assert (Int32(5) - 3).value == 2
    assert (3 - Int32(5)).value == -2


def test_mul():
    assert (Int32(5) * 3).value == 15
    assert (3 * Int32(5)).value == 15


def test_truediv_promotes_to_float():
    r = Int32(5) / 2
    assert isinstance(r, Float64)
    assert r.value == 2.5


def test_rtruediv():
    r = 6 / Int32(2)
    assert isinstance(r, Float64)
    assert r.value == 3.0


def test_floordiv():
    assert (Int32(5) // 2).value == 2
    assert (5 // Int32(2)).value == 2


def test_floordiv_negative():
    assert (Int32(-5) // 2).value == -3  # Python floor semantics


def test_mod():
    assert (Int32(5) % 2).value == 1
    assert (5 % Int32(2)).value == 1


def test_mod_negative():
    assert (Int32(-5) % 2).value == 1  # Python sign-of-divisor semantics


def test_pow():
    assert (Int32(2) ** 10).value == 1024
    assert (2 ** Int32(3)).value == 8


def test_pow_negative_exponent_promotes_to_float():
    r = Int32(2) ** -1
    assert isinstance(r, Float64)
    assert r.value == 0.5


def test_rpow_negative_exponent_promotes_to_float():
    r = 2 ** Int32(-1)
    assert isinstance(r, Float64)
    assert r.value == 0.5


def test_float_arithmetic():
    r = Float64(2.5) + 1.5
    assert isinstance(r, Float64)
    assert r.value == 4.0


def test_bool_arithmetic_numpy_semantics():
    r = Bool(True) + Bool(True)
    assert isinstance(r, Bool)
    assert r.value is True


def test_bool_plus_int_promotes():
    r = Bool(True) + 1
    assert isinstance(r, Int64)
    assert r.value == 2


def test_result_is_new_scalar():
    a = Int32(5)
    b = a + 3
    assert a.value == 5
    assert b.value == 8


# ---------------------------------------------------------------------------
# Type promotion
# ---------------------------------------------------------------------------

def test_promotion_uint8_int32():
    r = UInt8(5) + Int32(3)
    assert isinstance(r, Int32)
    assert r.value == 8


def test_promotion_int32_float64():
    r = Int32(5) + 3.0
    assert isinstance(r, Float64)
    assert r.value == 8.0


def test_promotion_uint8_overflow_wraps():
    r = UInt8(200) + UInt8(100)
    assert isinstance(r, UInt8)
    assert r.value == 44


# ---------------------------------------------------------------------------
# Bitwise arithmetic
# ---------------------------------------------------------------------------

def test_bitwise_and():
    assert (Int32(12) & 10).value == 8
    assert (10 & Int32(12)).value == 8


def test_bitwise_or():
    assert (Int32(12) | 3).value == 15


def test_bitwise_xor():
    assert (Int32(12) ^ 10).value == 6


def test_lshift():
    assert (Int32(5) << 2).value == 20


def test_rshift():
    assert (Int32(20) >> 2).value == 5


def test_invert():
    assert (~Int32(5)).value == -6


def test_bitwise_on_float_raises():
    with pytest.raises(TypeError):
        Float64(1.5) & 1


# ---------------------------------------------------------------------------
# Unary arithmetic
# ---------------------------------------------------------------------------

def test_neg():
    r = -Int32(5)
    assert isinstance(r, Int32)
    assert r.value == -5


def test_pos():
    assert (+Int32(5)).value == 5


def test_abs():
    r = abs(Int32(-5))
    assert isinstance(r, Int32)
    assert r.value == 5


def test_abs_float():
    assert abs(Float64(-2.5)).value == 2.5


# ---------------------------------------------------------------------------
# In-place arithmetic
# ---------------------------------------------------------------------------

def test_iadd_returns_self():
    s = Int32(5)
    r = s.__iadd__(3)
    assert r is s
    assert s.value == 8


def test_iadd_python_syntax():
    s = Int32(5)
    s += 3
    assert s.value == 8


def test_iadd_promotes_dtype():
    s = Int32(5)
    s += 0.5
    assert isinstance(s, Float64)
    assert s.value == 5.5


def test_isub():
    s = Int32(5)
    s -= 2
    assert s.value == 3


def test_imul():
    s = Int32(5)
    s *= 3
    assert s.value == 15


def test_itruediv():
    s = Int32(5)
    s /= 2
    assert isinstance(s, Float64)
    assert s.value == 2.5


def test_ifloordiv():
    s = Int32(5)
    s //= 2
    assert s.value == 2


def test_imod():
    s = Int32(5)
    s %= 2
    assert s.value == 1


def test_ipow():
    s = Int32(2)
    s **= 10
    assert s.value == 1024


def test_ipow_negative_exponent_promotes():
    s = Int32(2)
    s **= -1
    assert isinstance(s, Float64)
    assert s.value == 0.5


def test_iand():
    s = Int32(12)
    s &= 10
    assert s.value == 8


def test_ior():
    s = Int32(12)
    s |= 3
    assert s.value == 15


def test_ixor():
    s = Int32(12)
    s ^= 10
    assert s.value == 6


def test_ilshift():
    s = Int32(5)
    s <<= 2
    assert s.value == 20


def test_irshift():
    s = Int32(20)
    s >>= 2
    assert s.value == 5


def test_inplace_with_scalar_operand():
    s = Int32(5)
    s += Int32(3)
    assert s.value == 8


# ---------------------------------------------------------------------------
# Arithmetic error cases
# ---------------------------------------------------------------------------

def test_add_unsupported_type_raises():
    with pytest.raises(TypeError):
        Int32(5) + 'x'


def test_add_unsupported_type_reverse_raises():
    with pytest.raises(TypeError):
        'x' + Int32(5)


# ---------------------------------------------------------------------------
# Regression tests (code review findings)
# ---------------------------------------------------------------------------

def test_uint64_exact_value():
    # Values > 2^63-1 must not be corrupted (ADR-002 exact-value guarantee).
    u = UInt64(2**63)
    assert u.as_builtin() == 2**63
    assert int(u) == 2**63
    assert UInt64(2**64 - 1).as_builtin() == 2**64 - 1
    # __index__ must also preserve the exact value.
    import operator
    assert operator.index(UInt64(2**63)) == 2**63
    assert operator.index(UInt64(2**64 - 1)) == 2**64 - 1


def test_hash_consistent_with_eq():
    # Int32(1) == 1 must imply hash(Int32(1)) == hash(1) (dict/set contract).
    assert hash(Int32(1)) == hash(1)
    assert hash(UInt64(2**63)) == hash(2**63)


def test_mod_by_zero_raises():
    with pytest.raises(ZeroDivisionError):
        Int32(5) % 0
    with pytest.raises(ZeroDivisionError):
        Int32(5) // 0
    with pytest.raises(ZeroDivisionError):
        Float64(5.0) % 0.0


def test_shift_guards():
    # Shift counts >= width are UB in C++; must be guarded deterministically.
    assert (Int32(1) << 32).value == 0
    assert (Int32(-8) >> 32).value == -1
    with pytest.raises(ValueError):
        Int32(1) << -1


def test_right_shift_arithmetic():
    # Signed right shift must sign-extend (numpy semantics).
    assert (Int32(-8) >> 1).value == -4
    assert (Int8(-8) >> 1).value == -4


def test_signed_overflow_wraps():
    # numpy C semantics: signed overflow wraps, not UB.
    assert (Int8(100) + Int8(100)).value == -56
    assert (-Int64(-(2**63))).value == -(2**63)
    assert abs(Int64(-(2**63))).value == -(2**63)


def test_int_pow_wraps_like_legacy_array():
    # Legacy: np.array([2], int32) ** 31 wraps to int32.
    assert (Int32(2) ** 31).value == -2147483648


def test_huge_int_coercion_raises_overflow():
    with pytest.raises(OverflowError):
        Int8(2**70)
    with pytest.raises(OverflowError):
        UInt8(2**70)
    with pytest.raises(OverflowError):
        Float64(10**400)


def test_floordiv_stays_integer():
    r = Int32(5) // 2
    assert isinstance(r, Int32)
    assert r.value == 2


def test_numpy_view_keeps_wrapper_alive():
    import gc
    s = Int32(7)
    arr = s.numpy()
    del s
    gc.collect()
    assert arr[0] == 7  # base holds the wrapper alive