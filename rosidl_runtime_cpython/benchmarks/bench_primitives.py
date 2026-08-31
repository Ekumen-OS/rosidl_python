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

"""Phase 1A microbenchmarks (informational; ADR-010 methodology).

Measures the per-operation overhead of the non-owning reference-view layer:
virtual dispatch through the type-erased interfaces, wrapper attribute
access, sequence indexing, and scalar arithmetic. Run with:

    python3 benchmarks/bench_primitives.py

Results are informational — the Phase 1A gate is that virtual dispatch stays
in the low-nanosecond range (spike 4 measured ~1.9 ns/op).
"""

import statistics
import time

from rosidl_runtime_cpython._primitives import (
    Int32,
    Int32Sequence,
    UInt8Array,
    bench_virtual_dispatch,
)


def bench(fn, iterations, repeat=5):
    """Return median ns/op across `repeat` runs of `iterations` ops."""
    samples = []
    for _ in range(repeat):
        start = time.perf_counter_ns()
        fn(iterations)
        elapsed = time.perf_counter_ns() - start
        samples.append(elapsed / iterations)
    return statistics.median(samples)


def main():
    iterations = 2_000_000

    # 1. Virtual dispatch through the type-erased interface (C++ side).
    us = bench_virtual_dispatch(iterations)
    print(f'virtual-dispatch get() x{iterations}: '
          f'{us:.1f} us total, {us * 1e3 / iterations:.3f} ns/op')

    # 2. Wrapper attribute access: s.value get/set.
    s = Int32(7)

    def attr_get(n):
        acc = 0
        for _ in range(n):
            acc += s.value
        return acc

    def attr_set(n):
        for _ in range(n):
            s.value = 7
        return None

    ns = bench(attr_get, iterations)
    print(f'attribute get (s.value) x{iterations}: {ns:.2f} ns/op')
    ns = bench(attr_set, iterations)
    print(f'attribute set (s.value = 7) x{iterations}: {ns:.2f} ns/op')

    # 3. Sequence indexing: seq[i] (checked at() through the interface).
    seq = Int32Sequence([1, 2, 3, 4, 5])

    def seq_get(n):
        acc = 0
        for i in range(n):
            acc += seq[i % 5]
        return acc

    ns = bench(seq_get, iterations)
    print(f'sequence getitem x{iterations}: {ns:.2f} ns/op')

    # 4. Scalar arithmetic (builtin-return policy).
    a = Int32(5)

    def arith(n):
        acc = 0
        for _ in range(n):
            acc += a + 3
        return acc

    ns = bench(arith, iterations)
    print(f'scalar add (wrapper + int -> int) x{iterations}: {ns:.2f} ns/op')

    # 5. Array element access (checked at() through the interface).
    arr = UInt8Array.Make(4, [1, 2, 3, 4])

    def arr_get(n):
        acc = 0
        for i in range(n):
            acc += arr[i % 4]
        return acc

    ns = bench(arr_get, iterations)
    print(f'array getitem x{iterations}: {ns:.2f} ns/op')

    def arr_set(n):
        for i in range(n):
            arr[i % 4] = 7
        return None

    ns = bench(arr_set, iterations)
    print(f'array setitem x{iterations}: {ns:.2f} ns/op')


if __name__ == '__main__':
    main()