#!/usr/bin/env python3
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

"""Phase 3A copy-count benchmark (informational; ADR-010 methodology).

Counts per-element Python<->C++ conversions on the assignment/getter paths
via the thread-local counter in _primitives. The Phase 3A exit gate is that
no *unnecessary* copies remain: numpy/buffer sources must take the zero-copy
bulk path (0 conversions), while list-of-Python-scalar sources necessarily
materialize once (N conversions). Run with:

    python3 benchmarks/bench_copy_counts.py

Requires a built message package with a primitive sequence field (uses
sensor_msgs/Image.data by default; pass a different module path as argv[1]).
"""

import sys

import numpy as np

from rosidl_runtime_cpython._primitives import copy_count, reset_copy_count

N = 100_000


def report(label, expected, actual):
    status = 'OK' if actual == expected else 'FAIL'
    print(f'{status:4} {label}: {actual} conversions (expected {expected})')
    return status == 'OK'


def main():
    # Import the message package (sensor_msgs by default).
    pkg = sys.argv[1] if len(sys.argv) > 1 else 'sensor_msgs'
    msg_mod = __import__(f'{pkg}.msg.experimental', fromlist=['Image'])
    Image = msg_mod.Image

    ok = True

    # 1. numpy -> sequence slice assignment: zero-copy bulk path.
    img = Image()
    arr = np.arange(N, dtype=np.uint8)
    reset_copy_count()
    img.data = arr
    ok &= report('numpy -> field setter', 0, copy_count())

    # 2. list -> sequence slice assignment: one materialization (necessary).
    img = Image()
    lst = [i % 256 for i in range(N)]
    reset_copy_count()
    img.data = lst
    ok &= report('list  -> field setter', N, copy_count())

    # 3. numpy -> SequenceWrapper setitem (slice): zero-copy.
    from rosidl_runtime_cpython._primitives import UInt8Sequence
    seq = UInt8Sequence([0] * N)
    reset_copy_count()
    seq[:] = arr
    ok &= report('numpy -> seq[:] setitem', 0, copy_count())

    # 4. list -> SequenceWrapper setitem (slice): one materialization.
    seq = UInt8Sequence([0] * N)
    reset_copy_count()
    seq[:] = lst
    ok &= report('list  -> seq[:] setitem', N, copy_count())

    # 5. numpy <- field getitem (zero-copy view via .numpy()).
    img = Image()
    img.data = arr
    reset_copy_count()
    view = img.data.numpy()
    ok &= report('numpy <- field getitem (.numpy())', 0, copy_count())

    # 5b. slice getitem returns a list of builtins (builtin-return contract,
    # ADR-002 amendment): N conversions are required, not unnecessary.
    reset_copy_count()
    builtin = img.data[:]
    ok &= report('slice <- field getitem (list contract)', N, copy_count())

    # 6. as_builtin: N conversions (contract requires builtins).
    reset_copy_count()
    builtin = img.data.as_builtin()
    ok &= report('as_builtin', N, copy_count())

    # 7. scalar setitem: 1 conversion per element.
    seq = UInt8Sequence([0] * N)
    reset_copy_count()
    for i in range(N):
        seq[i] = 7
    ok &= report('scalar setitem xN', N, copy_count())

    print()
    print('PASS' if ok else 'FAIL')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()