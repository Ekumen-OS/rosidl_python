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

import gc

import _test_borrowed_view
import pytest


@pytest.mark.parametrize('return_explicitly', [True, False])
def test_retained_views_after_loan_return(return_explicitly):
    loan = _test_borrowed_view.Loan()
    message = loan.view()
    field = message.field
    field.value = 12
    assert message.field.value == 12

    if return_explicitly:
        loan.return_loan()
    else:
        del loan
        gc.collect()

    with pytest.raises(RuntimeError, match='no longer valid'):
        _ = message.field
    with pytest.raises(RuntimeError, match='no longer valid'):
        _ = field.value
    with pytest.raises(RuntimeError, match='no longer valid'):
        field.value = 1


def test_sequence_lifetimes_and_mutations():
    loan = _test_borrowed_view.Loan()
    message = loan.view()
    sequence = message.values()
    another = message.values()
    first = message.value_ref(0)
    second = message.value_ref(1)
    assert len(sequence) == 2
    assert sequence[-1] == 2
    with pytest.raises(IndexError):
        _ = sequence[2]
    with pytest.raises(TypeError, match='read-only'):
        message.values(read_only=True).append(3)
    assert first.value == 1
    another.append(3)
    assert list(sequence) == [1, 2, 3]
    with pytest.raises(RuntimeError, match='no longer valid'):
        _ = first.value
    with pytest.raises(RuntimeError, match='no longer valid'):
        _ = second.value
    fresh = message.value_ref(0)
    sequence[0] = 9
    with pytest.raises(RuntimeError, match='no longer valid'):
        _ = fresh.value
    assert sequence[0] == 9
    loan.return_loan()
    with pytest.raises(RuntimeError, match='no longer valid'):
        len(sequence)
    with pytest.raises(RuntimeError, match='no longer valid'):
        sequence.append(4)
