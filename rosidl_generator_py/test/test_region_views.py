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

import pytest
from rosidl_generator_py.msg.views import Arrays, BuiltinTypeSequencesIdl, UnboundedSequences


def test_primitive_arrays_and_sequences():
    arrays = Arrays()
    arrays.bool_values = [True, False, True]
    arrays.char_values = [1, 2, 255]
    arrays.int64_values = [-1, 0, 2 ** 60]
    assert list(arrays.bool_values) == [True, False, True]
    assert list(arrays.char_values) == [1, 2, 255]
    assert list(arrays.int64_values) == [-1, 0, 2 ** 60]
    with pytest.raises(ValueError, match='same length'):
        arrays.bool_values = [True]

    sequences = UnboundedSequences()
    sequences.bool_values = [True, False]
    sequences.uint8_values = [0, 255]
    assert list(sequences.bool_values) == [True, False]
    assert list(sequences.uint8_values) == [0, 255]


def test_idl_char_sequence():
    message = BuiltinTypeSequencesIdl()
    message.char_sequence_unbounded = [65, 66]
    assert list(message.char_sequence_unbounded) == [65, 66]
