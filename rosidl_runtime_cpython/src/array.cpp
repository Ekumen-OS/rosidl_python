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

/// Array wrapper registration (array.hpp). The final module (primitives.cpp)
/// calls register_arrays(); template instantiation is implicit here.

#include "array.hpp"

namespace rosidl_runtime_cpython
{

void register_arrays(py::module_ & m)
{
  // Fixed arrays (ADR-004 amendment): one class per element type.
  register_array<bool>(m, "BoolArray");
  register_array<uint8_t>(m, "UInt8Array");
  register_array<uint16_t>(m, "UInt16Array");
  register_array<uint32_t>(m, "UInt32Array");
  register_array<uint64_t>(m, "UInt64Array");
  register_array<int8_t>(m, "Int8Array");
  register_array<int16_t>(m, "Int16Array");
  register_array<int32_t>(m, "Int32Array");
  register_array<int64_t>(m, "Int64Array");
  register_array<float>(m, "Float32Array");
  register_array<double>(m, "Float64Array");
  register_array<long double>(m, "LongDoubleArray");

  // Aliases for the shared UInt8Array class (ADR-004 aliasing acceptance).
  m.attr("CharArray") = m.attr("UInt8Array");
  m.attr("ByteArray") = m.attr("UInt8Array");
  m.attr("OctetArray") = m.attr("UInt8Array");
}

}  // namespace rosidl_runtime_cpython