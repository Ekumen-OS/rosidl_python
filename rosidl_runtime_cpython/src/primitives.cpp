// Copyright 2026 Ekumen, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// Native statically typed scalar, string, sequence, and array wrappers
/// (Phase 1A: non-owning reference views, ADR-011). Each family lives in its
/// own .hpp/.cpp pair (scalar, string, sequence, array); this TU is the final
/// module: it registers every family and exposes the microbenchmarks.

#include <chrono>

#include "array.hpp"
#include "message_handle.hpp"
#include "scalar.hpp"
#include "sequence.hpp"
#include "string.hpp"

namespace rosidl_runtime_cpython
{

// Microbenchmark (informational; Phase 1A gate): virtual dispatch through the
// type-erased reference interfaces. Measures the per-op overhead of the
// non-owning view layer (ArrayInterface::at) in microseconds for
// `iterations` calls.
double bench_virtual_dispatch(size_t iterations)
{
  ArrayData<uint8_t, 4> ad;
  auto start = std::chrono::steady_clock::now();
  volatile uint8_t sink = 0;
  for (size_t i = 0; i < iterations; ++i) {
    sink += ad.wrapper.get(0);
  }
  auto end = std::chrono::steady_clock::now();
  (void)sink;
  return std::chrono::duration<double, std::micro>(end - start).count();
}

}  // namespace rosidl_runtime_cpython

PYBIND11_MODULE(_primitives, m)
{
  using namespace rosidl_runtime_cpython;
  m.doc() = "Native statically typed non-owning wrappers (Phase 1A)";

  m.def("bench_virtual_dispatch", &bench_virtual_dispatch, py::arg("iterations"));
  m.def("copy_count", []() { return element_copies(); });
  m.def("reset_copy_count", []() { element_copies() = 0; });

  register_scalars(m);
  register_strings(m);
  register_sequences(m);
  register_arrays(m);
  register_constraints(m);

  // Non-template message-handle base: generated message handles derive from
  // it, enabling rclpy and the typesupport adapters to unwrap/wrap
  // type-erased.  The shared_ptr holder matches the generated handles' holder
  // type.
  py::class_<MessageHandleInterface, std::shared_ptr<MessageHandleInterface>>(
    m, "MessageHandleInterface");
}