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

/// @file raw_buffer_cpp.cpp
/// @brief pybind11 wrapper for the RawBuffer C API.
///
/// The XCDR CPython typesupport wraps external rosidl_memory_region_t
/// descriptors (loaned samples, wire buffers) as RawBuffer objects.  That
/// wrapping lives here — in the package that owns RawBuffer — so downstream
/// typesupports import this module rather than re-implementing the C API
/// import and wrap themselves.

#include <pybind11/pybind11.h>

#include <cstdint>

#include "rosidl_runtime_c/experimental/memory.h"
#include "rosidl_runtime_cpython/raw_buffer.h"

namespace py = pybind11;

namespace rosidl_runtime_cpython
{

/// Wrap the memory region at *address* (size *size* bytes) as a non-owning
/// RawBuffer.  The caller must keep the underlying memory alive for as long
/// as the returned object is used.
///
/// Raises on failure (RawBuffer C API unavailable, or the C API factory
/// failed).  The exception is left pending so callers can propagate it.
py::object raw_buffer_from_region(uintptr_t address, size_t size)
{
  if (RawBuffer_ImportCAPI() < 0) {
    throw py::error_already_set();
  }
  rosidl_memory_region_t region{{reinterpret_cast<void *>(address), 0}, size};
  PyObject * buf = RawBuffer_FromRegion(&region);
  if (buf == nullptr) {
    throw py::error_already_set();
  }
  // Steal the new reference into a py::object.
  return py::reinterpret_steal<py::object>(buf);
}

}  // namespace rosidl_runtime_cpython

PYBIND11_MODULE(_raw_buffer_cpp, m)
{
  m.doc() = "pybind11 helpers for the RawBuffer C API (external region wrapping).";
  m.def(
    "from_region", &rosidl_runtime_cpython::raw_buffer_from_region,
    py::arg("address"), py::arg("size"),
    "Wrap the memory region at *address* (size *size* bytes) as a non-owning "
    "RawBuffer.  Raises on failure.");
}
