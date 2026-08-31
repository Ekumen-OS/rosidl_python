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

"""Tests for the native String/WString wrappers (Phase 1).

Ported from test_string.py to the new native API (ADR-002/004).
"""

import numpy as np
import pytest

from rosidl_runtime_cpython._primitives import String, UInt8, WString


# ===========================================================================
# String
# ===========================================================================

class TestString:

    def test_empty(self):
        s = String()
        assert len(s) == 0
        assert str(s) == ''

    def test_construct_from_str(self):
        s = String('hello')
        assert str(s) == 'hello'

    def test_construct_from_bytes(self):
        s = String(b'world')
        assert str(s) == 'world'

    def test_assign_str(self):
        s = String()
        s.assign('hello')
        assert str(s) == 'hello'
        assert len(s) == 5

    def test_assign_bytes(self):
        s = String()
        s.assign(b'world')
        assert str(s) == 'world'

    def test_assign_replaces_content(self):
        s = String()
        s.assign('first')
        s.assign('second')
        assert str(s) == 'second'

    def test_assign_empty_string(self):
        s = String()
        s.assign('hello')
        s.assign('')
        assert len(s) == 0
        assert str(s) == ''

    def test_assign_utf8_multibyte(self):
        s = String()
        s.assign('café')
        assert len(s) == 5
        assert str(s) == 'café'

    def test_append_str(self):
        s = String()
        s.assign('hello')
        s.append_str(' world')
        assert str(s) == 'hello world'
        assert len(s) == 11

    def test_append_str_to_empty(self):
        s = String()
        s.append_str('hi')
        assert str(s) == 'hi'

    def test_assign_bad_type_raises(self):
        with pytest.raises(TypeError):
            String().assign(42)

    def test_getitem_byte(self):
        s = String()
        s.assign('ABC')
        assert s[0] == 'A'
        assert s[1] == 'B'
        assert s[2] == 'C'

    def test_getitem_negative(self):
        s = String()
        s.assign('ABC')
        assert s[-1] == 'C'

    def test_getitem_slice(self):
        s = String()
        s.assign('ABCDE')
        assert str(s[1:3]) == 'BC'

    def test_getitem_out_of_range_raises(self):
        s = String()
        s.assign('AB')
        with pytest.raises(IndexError):
            _ = s[5]

    def test_setitem(self):
        s = String()
        s.assign('ABC')
        s[1] = 'X'
        assert str(s) == 'AXC'

    def test_setitem_slice(self):
        s = String()
        s.assign('ABCDE')
        s[1:4] = ['X', 'Y', 'Z']
        assert str(s) == 'AXYZE'

    def test_setitem_slice_string_value(self):
        s = String()
        s.assign('ABCDE')
        s[1:4] = String('XYZ')
        assert str(s) == 'AXYZE'

    def test_setitem_slice_bytes_value(self):
        s = String()
        s.assign('ABCDE')
        s[1:4] = b'XYZ'
        assert str(s) == 'AXYZE'

    def test_setitem_slice_str_value(self):
        s = String()
        s.assign('ABCDE')
        s[1:4] = 'XYZ'
        assert str(s) == 'AXYZE'

    def test_setitem_slice_step_string_value(self):
        s = String()
        s.assign('ABCDE')
        s[::2] = String('123')
        assert str(s) == '1B2D3'

    def test_setitem_slice_aliasing_self(self):
        s = String()
        s.assign('ABCDE')
        s[1:4] = s[0:3]  # str RHS (slices return str)
        assert str(s) == 'AABCE'
        s.assign('ABCDE')
        s[0:5] = s  # StringWrapper RHS aliasing self, matching length
        assert str(s) == 'ABCDE'

    def test_setitem_slice_length_mismatch_raises(self):
        s = String()
        s.assign('ABCDE')
        with pytest.raises(ValueError):
            s[1:4] = ['X', 'Y']
        with pytest.raises(ValueError):
            s[1:4] = String('XY')

    def test_delitem_int(self):
        s = String()
        s.assign('ABC')
        del s[1]
        assert str(s) == 'AC'

    def test_delitem_slice_step(self):
        s = String()
        s.assign('ZAXEW')
        del s[::2]
        assert str(s) == 'AE'

    def test_delitem_slice_negative_step(self):
        s = String()
        s.assign('ABCDE')
        del s[4:0:-2]
        assert str(s) == 'ABD'

    def test_delitem_slice_full_range(self):
        s = String()
        s.assign('ABC')
        del s[:]
        assert str(s) == ''

    def test_delitem_slice_prefix(self):
        s = String()
        s.assign('ABCDE')
        del s[:2]
        assert str(s) == 'CDE'
        del s[:-1]  # removes [0, len-1), leaving only the last char
        assert str(s) == 'E'

    def test_delitem_slice_empty(self):
        s = String()
        s.assign('ABC')
        del s[3:3]
        assert str(s) == 'ABC'

    def test_delitem_first(self):
        s = String()
        s.assign('ABC')
        del s[0]
        assert str(s) == 'BC'

    def test_delitem_negative(self):
        s = String()
        s.assign('ABC')
        del s[-1]
        assert str(s) == 'AB'

    def test_insert_boundaries(self):
        s = String()
        s.assign('BC')
        s.insert(0, 'A')
        assert str(s) == 'ABC'
        s.insert(99, 'D')
        assert str(s) == 'ABCD'
        s.insert(-1, 'X')
        assert str(s) == 'ABCXD'

    def test_insert_into_empty(self):
        s = String()
        s.insert(0, 'A')
        assert str(s) == 'A'

    def test_code_unit_range_checked(self):
        s = String()
        with pytest.raises(OverflowError):
            s.append(256)
        with pytest.raises(OverflowError):
            s.append(-1)
        # Bytes 128-255 are valid (char is unsigned in the API).
        s.append(200)
        assert s[-1] == '\u00c8'  # chr(200)
        s[0] = '\xff'
        assert s[0] == '\xff'
        assert '\xff' in s
        w = WString()
        with pytest.raises(OverflowError):
            w.append(65536)

    def test_append_byte(self):
        s = String()
        s.assign('AB')
        s.append('C')
        assert str(s) == 'ABC'

    def test_insert_byte(self):
        s = String()
        s.assign('AC')
        s.insert(1, 'B')
        assert str(s) == 'ABC'

    def test_clear(self):
        s = String()
        s.assign('hello')
        s.clear()
        assert len(s) == 0

    def test_iter(self):
        s = String()
        s.assign('AB')
        assert list(s) == ['A', 'B']

    def test_iter_keeps_wrapper_alive(self):
        import gc
        s = String('ABC')
        it = iter(s)
        del s
        gc.collect()
        assert list(it) == ['A', 'B', 'C']

    def test_contains(self):
        s = String()
        s.assign('ABC')
        assert 'B' in s
        assert 'Z' not in s
        assert 66 in s  # int code-unit value
        assert UInt8(65) in s  # Char scalar

    def test_reversed(self):
        s = String()
        s.assign('ABC')
        assert list(reversed(s)) == ['C', 'B', 'A']

    def test_eq_string(self):
        assert String('abc') == String('abc')
        assert String('abc') != String('abd')

    def test_eq_str(self):
        assert String('abc') == 'abc'
        assert String('abc') != 'abd'

    def test_numpy(self):
        s = String()
        s.assign('ABC')
        arr = s.numpy()
        assert isinstance(arr, np.ndarray)
        assert arr.dtype == np.uint8
        assert list(arr) == [ord('A'), ord('B'), ord('C')]

    def test_buffer_protocol(self):
        s = String()
        s.assign('ABC')
        mv = memoryview(s)
        assert len(mv) == 3
        assert bytes(mv) == b'ABC'

    def test_repr(self):
        assert repr(String('abc')) == "String('abc')"

    def test_add_string(self):
        r = String('ab') + String('cd')
        assert isinstance(r, str)
        assert r == 'abcd'

    def test_add_str(self):
        r = String('ab') + 'cd'
        assert isinstance(r, str)
        assert r == 'abcd'

    def test_radd_str(self):
        r = 'ab' + String('cd')
        assert isinstance(r, str)
        assert r == 'abcd'

    def test_add_returns_new_string(self):
        a = String('ab')
        b = a + 'cd'
        assert str(a) == 'ab'
        assert b == 'abcd'

    def test_mul(self):
        assert (String('ab') * 3) == 'ababab'

    def test_rmul(self):
        assert (3 * String('ab')) == 'ababab'

    def test_mul_zero(self):
        assert (String('ab') * 0) == ''

    def test_iadd_str(self):
        s = String('ab')
        s += 'cd'
        assert str(s) == 'abcd'

    def test_iadd_string(self):
        s = String('ab')
        s += String('cd')
        assert str(s) == 'abcd'

    def test_iadd_self_long(self):
        # Self-append beyond the inline capacity must not dangle the source.
        s = String('a' * 100)
        s += s
        assert str(s) == 'a' * 200

    def test_imul(self):
        s = String('ab')
        s *= 3
        assert str(s) == 'ababab'

    def test_imul_zero(self):
        s = String('ab')
        s *= 0
        assert str(s) == ''

    def test_as_builtin(self):
        s = String('hello')
        assert s.as_builtin() == 'hello'
        assert isinstance(s.as_builtin(), str)

    def test_from_builtin_roundtrip(self):
        s = String()
        s.from_builtin('hello')
        assert s.as_builtin() == 'hello'

    def test_value_property(self):
        s = String()
        s.value = 'hello'
        assert s.value == 'hello'

    def test_numpy_view_keeps_wrapper_alive(self):
        import gc
        s = String('abc')
        arr = s.numpy()
        del s
        gc.collect()
        assert list(arr) == [ord('a'), ord('b'), ord('c')]

    def test_memoryview_keeps_wrapper_alive(self):
        import gc
        s = String('abc')
        mv = memoryview(s)
        del s
        gc.collect()
        assert bytes(mv) == b'abc'


# ===========================================================================
# WString
# ===========================================================================

class TestWString:

    def test_empty(self):
        s = WString()
        assert len(s) == 0
        assert str(s) == ''

    def test_construct_from_str(self):
        s = WString('héllo')
        assert str(s) == 'héllo'

    def test_assign_str(self):
        s = WString()
        s.assign('héllo')
        assert str(s) == 'héllo'

    def test_assign_multibyte(self):
        s = WString()
        s.assign('café')
        assert str(s) == 'café'

    def test_assign_replaces_content(self):
        s = WString()
        s.assign('first')
        s.assign('second')
        assert str(s) == 'second'

    def test_append_str(self):
        s = WString()
        s.assign('hello')
        s.append_str(' world')
        assert str(s) == 'hello world'

    def test_assign_odd_bytes_raises(self):
        with pytest.raises(ValueError):
            WString().assign(b'\x00')

    def test_getitem_unit(self):
        s = WString()
        s.assign('ABC')
        assert s[0] == 'A'
        assert s[1] == 'B'

    def test_getitem_slice(self):
        s = WString()
        s.assign('ABCDE')
        assert str(s[1:3]) == 'BC'

    def test_setitem(self):
        s = WString()
        s.assign('ABC')
        s[1] = 'X'
        assert str(s) == 'AXC'

    def test_setitem_slice(self):
        s = WString()
        s.assign('ABCDE')
        s[1:4] = ['X', 'Y', 'Z']
        assert str(s) == 'AXYZE'

    def test_setitem_slice_string_value(self):
        s = WString()
        s.assign('ABCDE')
        s[1:4] = WString('XYZ')
        assert str(s) == 'AXYZE'
        s[1:4] = 'XYZ'
        assert str(s) == 'AXYZE'

    def test_setitem_slice_bytes_value(self):
        s = WString()
        s.assign('ABCDE')
        s[1:4] = 'XYZ'.encode('utf-16-le')
        assert str(s) == 'AXYZE'

    def test_setitem_slice_step_string_value(self):
        s = WString()
        s.assign('ABCDE')
        s[::2] = WString('123')
        assert str(s) == '1B2D3'

    def test_setitem_slice_aliasing_self(self):
        s = WString()
        s.assign('ABCDE')
        s[0:5] = s  # StringWrapper aliasing self, matching length
        assert str(s) == 'ABCDE'

    def test_setitem_slice_length_mismatch_raises(self):
        s = WString()
        s.assign('ABCDE')
        with pytest.raises(ValueError):
            s[1:4] = WString('XY')
        with pytest.raises(ValueError):
            s[1:4] = 'XY'.encode('utf-16-le')

    def test_delitem(self):
        s = WString()
        s.assign('ABC')
        del s[1]
        assert str(s) == 'AC'

    def test_delitem_slice(self):
        s = WString()
        s.assign('ABCDE')
        del s[1:3]
        assert str(s) == 'ADE'

    def test_delitem_slice_prefix(self):
        s = WString()
        s.assign('ABCDE')
        del s[:2]
        assert str(s) == 'CDE'

    def test_append_unit(self):
        s = WString()
        s.assign('AB')
        s.append('C')
        assert str(s) == 'ABC'

    def test_insert_unit(self):
        s = WString()
        s.assign('AC')
        s.insert(1, 'B')
        assert str(s) == 'ABC'

    def test_clear(self):
        s = WString()
        s.assign('hello')
        s.clear()
        assert len(s) == 0

    def test_iter(self):
        s = WString()
        s.assign('AB')
        assert list(s) == ['A', 'B']

    def test_iter_keeps_wrapper_alive(self):
        import gc
        s = WString('AB')
        it = iter(s)
        del s
        gc.collect()
        assert list(it) == ['A', 'B']

    def test_contains(self):
        s = WString()
        s.assign('ABC')
        assert 'B' in s
        assert 'Z' not in s

    def test_eq_wstring(self):
        assert WString('abc') == WString('abc')
        assert WString('abc') != WString('abd')

    def test_eq_str(self):
        assert WString('abc') == 'abc'
        assert WString('abc') != 'abd'

    def test_numpy(self):
        s = WString()
        s.assign('ABC')
        arr = s.numpy()
        assert isinstance(arr, np.ndarray)
        assert arr.dtype == np.uint16
        assert list(arr) == [ord('A'), ord('B'), ord('C')]

    def test_buffer_protocol(self):
        s = WString()
        s.assign('ABC')
        mv = memoryview(s)
        assert len(mv) == 3  # 3 code units

    def test_repr(self):
        assert repr(WString('abc')) == "WString('abc')"

    def test_add_wstring(self):
        r = WString('ab') + WString('cd')
        assert isinstance(r, str)
        assert r == 'abcd'

    def test_add_str(self):
        r = WString('ab') + 'cd'
        assert isinstance(r, str)
        assert r == 'abcd'

    def test_radd_str(self):
        r = 'ab' + WString('cd')
        assert isinstance(r, str)
        assert r == 'abcd'

    def test_mul(self):
        assert (WString('ab') * 3) == 'ababab'

    def test_iadd_str(self):
        s = WString('ab')
        s += 'cd'
        assert str(s) == 'abcd'

    def test_imul(self):
        s = WString('ab')
        s *= 3
        assert str(s) == 'ababab'

    def test_as_builtin(self):
        s = WString('héllo')
        assert s.as_builtin() == 'héllo'

    def test_from_builtin_roundtrip(self):
        s = WString()
        s.from_builtin('héllo')
        assert s.as_builtin() == 'héllo'

    def test_value_property(self):
        s = WString()
        s.value = 'héllo'
        assert s.value == 'héllo'

# ---------------------------------------------------------------------------
# Phase 3A: str-semantics methods
# ---------------------------------------------------------------------------

def test_string_ne_ordering_hash_mod_format():
    s = String.Make('abc')
    assert s != String.Make('abd')
    assert s < String.Make('abd')
    assert String.Make('abd') > s
    assert s <= String.Make('abc')
    assert hash(s) == hash('abc')
    assert (String.Make('x%sx') % 'abc') == 'xabcx'
    assert format(s, '>5') == '  abc'
