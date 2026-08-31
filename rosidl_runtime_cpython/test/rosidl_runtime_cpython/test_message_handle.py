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

"""Tests for the Phase 1B MessageHandleBase pattern (ADR-008/011).

Proves, with a simulated experimental message: top-level ownership via
unique_ptr with a custom deleter, nested message views (non-owning,
parent-anchored), field getters returning non-owning wrappers, write-through
to the C++ members, parent-anchor lifetime, no reference cycles, and
exact-once destruction.
"""

import gc
import weakref

import pytest

from rosidl_runtime_cpython._primitives import UInt8Array  # noqa: F401  (registers wrapper types)
from rosidl_runtime_cpython._simulated import SimHeader, SimMessage


# ---------------------------------------------------------------------------
# Construction / ownership
# ---------------------------------------------------------------------------

def test_construction_defaults():
    m = SimMessage()
    assert m.width.value == 0
    assert m.height.value == 0
    assert m.encoding.value == ''
    assert m.pixels.as_builtin() == []
    assert m.data.as_builtin() == [0, 0, 0, 0]


def test_top_level_owns_message():
    assert SimMessage.alive_count() == 0
    m = SimMessage()
    assert SimMessage.alive_count() == 1
    del m
    gc.collect()
    assert SimMessage.alive_count() == 0


def test_top_level_header_owns_message():
    # The standalone SimHeader() owning path (not just the nested view).
    h = SimHeader()
    h.stamp.value = 5
    assert h.stamp.value == 5
    del h
    gc.collect()


def test_nested_view_does_not_own():
    # A nested header view is non-owning: destroying it must not destroy the
    # message it views into.
    assert SimMessage.alive_count() == 0
    m = SimMessage()
    h = m.header
    assert SimMessage.alive_count() == 1
    del h
    gc.collect()
    assert SimMessage.alive_count() == 1  # message still alive
    del m
    gc.collect()
    assert SimMessage.alive_count() == 0


# ---------------------------------------------------------------------------
# Field write-through
# ---------------------------------------------------------------------------

def test_scalar_field_write_through():
    m = SimMessage()
    w = m.width
    w.value = 640
    assert m.width.value == 640
    assert m.height.value == 0  # other fields unaffected


def test_string_field_write_through():
    m = SimMessage()
    e = m.encoding
    e.value = 'rgb8'
    assert m.encoding.value == 'rgb8'


def test_sequence_field_write_through():
    m = SimMessage()
    p = m.pixels
    p.append(1)
    p.append(2)
    assert m.pixels.as_builtin() == [1, 2]


def test_array_field_write_through():
    m = SimMessage()
    d = m.data
    d[0] = 255
    assert m.data.as_builtin() == [255, 0, 0, 0]


# ---------------------------------------------------------------------------
# Attribute assignment (properties are assignable, matching C++ members)
# ---------------------------------------------------------------------------

def test_scalar_attribute_assignment():
    m = SimMessage()
    m.width = 640
    assert m.width.value == 640
    assert m.height.value == 0  # other fields unaffected


def test_string_attribute_assignment():
    m = SimMessage()
    m.encoding = 'rgb8'
    assert m.encoding.value == 'rgb8'


def test_sequence_attribute_assignment():
    m = SimMessage()
    m.pixels = [1, 2, 3]
    assert m.pixels.as_builtin() == [1, 2, 3]


def test_array_attribute_assignment():
    m = SimMessage()
    m.data = [9, 8, 7, 6]
    assert m.data.as_builtin() == [9, 8, 7, 6]
    with pytest.raises(ValueError):
        m.data = [1, 2]  # fixed size must match exactly


def test_nested_attribute_assignment():
    m = SimMessage()
    m.header = {'stamp': 5, 'frame_id': 'map', 'seq': 7}
    assert m.header.stamp.value == 5
    assert m.header.frame_id.value == 'map'
    assert m.header.seq.value == 7
    # Assign from another header view copies (no aliasing).
    src = SimHeader()
    src.stamp.value = 42
    m.header = src
    assert m.header.stamp.value == 42
    src.stamp.value = 99
    assert m.header.stamp.value == 42  # copy, not a view


def test_nested_scalar_attribute_assignment():
    m = SimMessage()
    h = m.header
    h.stamp = 12345
    h.frame_id = 'map'
    h.seq = 7
    assert m.header.stamp.value == 12345
    assert m.header.frame_id.value == 'map'
    assert m.header.seq.value == 7


# ---------------------------------------------------------------------------
# Nested message views
# ---------------------------------------------------------------------------

def test_nested_message_fields():
    m = SimMessage()
    h = m.header
    h.stamp.value = 12345
    h.frame_id.value = 'map'
    h.seq.value = 7
    # Write-through to the nested C++ member.
    assert m.header.stamp.value == 12345
    assert m.header.frame_id.value == 'map'
    assert m.header.seq.value == 7


def test_nested_view_anchored_to_parent():
    m = SimMessage()
    h = m.header
    s = h.stamp
    s.value = 99
    del m
    gc.collect()
    # The header view keeps the message alive; the stamp wrapper keeps the
    # header view alive.
    assert s.value == 99
    assert h.stamp.value == 99
    del h, s
    gc.collect()


# ---------------------------------------------------------------------------
# Parent-anchor lifetime (wrappers outlive the handle)
# ---------------------------------------------------------------------------

def test_wrappers_outlive_handle():
    m = SimMessage()
    w = m.width
    w.value = 640
    e = m.encoding
    e.value = 'rgb8'
    p = m.pixels
    p.append(9)
    d = m.data
    d[3] = 77
    del m
    gc.collect()
    assert w.value == 640
    assert e.value == 'rgb8'
    assert p.as_builtin() == [9]
    assert d.as_builtin() == [0, 0, 0, 77]
    del w, e, p, d
    gc.collect()


def test_no_reference_cycle():
    m = SimMessage()
    ref = weakref.ref(m)
    w = m.width
    h = m.header
    s = h.stamp
    del m
    gc.collect()
    assert ref() is not None  # alive while wrappers anchored
    del w, h, s
    gc.collect()
    assert ref() is None  # collected after the last wrapper dies (no cycle)


def test_nested_no_reference_cycle():
    m = SimMessage()
    h = m.header
    href = weakref.ref(h)
    s = h.stamp
    del m
    gc.collect()
    assert href() is not None  # header view alive while stamp wrapper anchored
    del s, h
    gc.collect()
    assert href() is None  # header view collected (no cycle)


# ---------------------------------------------------------------------------
# Exact-once destruction
# ---------------------------------------------------------------------------

def test_exact_once_destruction_with_wrappers():
    assert SimMessage.alive_count() == 0
    m = SimMessage()
    w = m.width
    del m
    gc.collect()
    assert SimMessage.alive_count() == 1  # wrapper keeps the message alive
    del w
    gc.collect()
    assert SimMessage.alive_count() == 0  # freed exactly once


def test_header_view_lifetime():
    # The nested header view keeps the message alive; SimHeader instances are
    # copied into message-element containers, so the alive counter is not
    # asserted here (SimMessage::alive_count covers exact-once destruction).
    m = SimMessage()
    h = m.header
    del m
    gc.collect()
    assert h.stamp.value == 0  # header view keeps the message alive
    del h
    gc.collect()


# ---------------------------------------------------------------------------
# Message-element sequences (Sequence<SimHeader>)
# ---------------------------------------------------------------------------

def test_message_sequence_getitem_returns_views():
    m = SimMessage()
    hs = m.headers
    assert len(hs) == 0
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    hs.append({'stamp': 2, 'frame_id': 'b', 'seq': 20})
    assert len(hs) == 2
    # getitem returns a parent-anchored SimHeader view.
    h0 = hs[0]
    assert h0.stamp.value == 1
    assert h0.frame_id.value == 'a'
    assert h0.seq.value == 10
    # Write-through to the C++ element.
    h0.stamp.value = 99
    assert hs[0].stamp.value == 99
    assert hs[1].stamp.value == 2  # other elements unaffected


def test_message_sequence_setitem_copies():
    m = SimMessage()
    hs = m.headers
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    # setitem from a dict copies into the slot.
    hs[0] = {'stamp': 5, 'frame_id': 'z', 'seq': 50}
    assert hs[0].stamp.value == 5
    assert hs[0].frame_id.value == 'z'
    # setitem from another SimHeader view copies (no aliasing).
    src = SimHeader()
    src.stamp.value = 7
    hs[0] = src
    assert hs[0].stamp.value == 7
    src.stamp.value = 8
    assert hs[0].stamp.value == 7  # copy, not a view


def test_message_sequence_as_builtin():
    m = SimMessage()
    hs = m.headers
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    hs.append({'stamp': 2, 'frame_id': 'b', 'seq': 20})
    assert hs.as_builtin() == [
        {'stamp': 1, 'frame_id': 'a', 'seq': 10},
        {'stamp': 2, 'frame_id': 'b', 'seq': 20},
    ]


def test_message_sequence_iteration_views():
    m = SimMessage()
    hs = m.headers
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    hs.append({'stamp': 2, 'frame_id': 'b', 'seq': 20})
    views = list(hs)
    assert [v.stamp.value for v in views] == [1, 2]
    assert [v.seq.value for v in reversed(hs)] == [20, 10]


def test_message_sequence_contains_and_remove():
    m = SimMessage()
    hs = m.headers
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    hs.append({'stamp': 2, 'frame_id': 'b', 'seq': 20})
    assert {'stamp': 1, 'frame_id': 'a', 'seq': 10} in hs
    assert {'stamp': 9, 'frame_id': 'x', 'seq': 90} not in hs
    hs.remove({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    assert len(hs) == 1
    assert hs[0].seq.value == 20


def test_message_sequence_view_lifetime():
    m = SimMessage()
    hs = m.headers
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    h0 = hs[0]
    del m
    gc.collect()
    # The element view keeps the message (and the sequence) alive.
    assert h0.stamp.value == 1
    del hs, h0
    gc.collect()


def test_message_sequence_pop_returns_detached_copy():
    # pop must return the popped element's data, not a live view that the
    # shift overwrites (message elements return a detached dict).
    m = SimMessage()
    hs = m.headers
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    hs.append({'stamp': 2, 'frame_id': 'b', 'seq': 20})
    popped = hs.pop(0)
    assert popped == {'stamp': 1, 'frame_id': 'a', 'seq': 10}
    assert len(hs) == 1
    assert hs[0].seq.value == 20  # remaining element intact


def test_message_sequence_extend_self():
    # s.extend(s) must not dangle on reallocation (message elements).
    m = SimMessage()
    hs = m.headers
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    hs.append({'stamp': 2, 'frame_id': 'b', 'seq': 20})
    hs.extend(hs)
    assert len(hs) == 4
    assert hs.as_builtin() == [
        {'stamp': 1, 'frame_id': 'a', 'seq': 10},
        {'stamp': 2, 'frame_id': 'b', 'seq': 20},
        {'stamp': 1, 'frame_id': 'a', 'seq': 10},
        {'stamp': 2, 'frame_id': 'b', 'seq': 20},
    ]


def test_message_sequence_extend_self_slice():
    # s.extend(s[0:2]) — the slice holds views into the sequence's own
    # storage; must be materialized before push_back reallocates.
    m = SimMessage()
    hs = m.headers
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    hs.append({'stamp': 2, 'frame_id': 'b', 'seq': 20})
    hs.extend(hs[0:2])
    assert len(hs) == 4
    assert hs[2].seq.value == 10
    assert hs[3].seq.value == 20


def test_message_sequence_assign_self():
    # s.assign(s) keeps contents (message elements).
    m = SimMessage()
    hs = m.headers
    hs.append({'stamp': 1, 'frame_id': 'a', 'seq': 10})
    hs.append({'stamp': 2, 'frame_id': 'b', 'seq': 20})
    hs.assign(hs)
    assert len(hs) == 2
    assert hs.as_builtin() == [
        {'stamp': 1, 'frame_id': 'a', 'seq': 10},
        {'stamp': 2, 'frame_id': 'b', 'seq': 20},
    ]


# ---------------------------------------------------------------------------
# Message-element arrays (Array<SimHeader, 2>)
# ---------------------------------------------------------------------------

def test_message_array_getitem_views():
    m = SimMessage()
    pair = m.pair
    assert len(pair) == 2
    pair[0] = {'stamp': 1, 'frame_id': 'a', 'seq': 10}
    pair[1] = {'stamp': 2, 'frame_id': 'b', 'seq': 20}
    h0 = pair[0]
    assert h0.stamp.value == 1
    h0.stamp.value = 99
    assert pair[0].stamp.value == 99
    assert pair[1].stamp.value == 2


def test_message_array_as_builtin():
    m = SimMessage()
    pair = m.pair
    pair[0] = {'stamp': 1, 'frame_id': 'a', 'seq': 10}
    pair[1] = {'stamp': 2, 'frame_id': 'b', 'seq': 20}
    assert pair.as_builtin() == [
        {'stamp': 1, 'frame_id': 'a', 'seq': 10},
        {'stamp': 2, 'frame_id': 'b', 'seq': 20},
    ]


def test_message_array_iteration_and_slice():
    m = SimMessage()
    pair = m.pair
    pair[0] = {'stamp': 1, 'frame_id': 'a', 'seq': 10}
    pair[1] = {'stamp': 2, 'frame_id': 'b', 'seq': 20}
    assert [v.seq.value for v in pair] == [10, 20]
    sl = pair[0:1]
    assert len(sl) == 1
    assert sl[0].stamp.value == 1


def test_message_array_contains():
    m = SimMessage()
    pair = m.pair
    pair[0] = {'stamp': 1, 'frame_id': 'a', 'seq': 10}
    assert {'stamp': 1, 'frame_id': 'a', 'seq': 10} in pair
    assert {'stamp': 9, 'frame_id': 'x', 'seq': 90} not in pair