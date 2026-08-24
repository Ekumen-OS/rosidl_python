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

/// Native statically typed scalar and string wrappers (Phase 1).
/// Wrapper implementations live in scalar_wrapper.hpp / string_wrapper.hpp;
/// this TU only registers them.

#include "scalar_wrapper.hpp"
#include "string_wrapper.hpp"

PYBIND11_MODULE(_primitives, m)
{
  using namespace rosidl_runtime_cpython;
  m.doc() = "Native statically typed scalar wrappers (Phase 1)";

  register_scalar<bool>(m, "Bool");
  register_scalar<uint8_t>(m, "UInt8");
  register_scalar<uint16_t>(m, "UInt16");
  register_scalar<uint32_t>(m, "UInt32");
  register_scalar<uint64_t>(m, "UInt64");
  register_scalar<int8_t>(m, "Int8");
  register_scalar<int16_t>(m, "Int16");
  register_scalar<int32_t>(m, "Int32");
  register_scalar<int64_t>(m, "Int64");
  register_scalar<float>(m, "Float32");
  register_scalar<double>(m, "Float64");
  register_scalar<long double>(m, "LongDouble");

  // Aliases for the shared UInt8 class (ADR-004 aliasing acceptance).
  m.attr("Char") = m.attr("UInt8");
  m.attr("Byte") = m.attr("UInt8");
  m.attr("Octet") = m.attr("UInt8");

  // Unbounded String / WString (ADR-004).
  register_string<char>(m, "String");
  register_string<char16_t>(m, "WString");
}
