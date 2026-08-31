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

/// Sequence wrapper registration (sequence.hpp). The final module
/// (primitives.cpp) calls register_sequences(); template instantiation is
/// implicit here.

#include "sequence.hpp"

namespace rosidl_runtime_cpython
{

void register_sequences(py::module_ & m)
{
  // Unbounded + bounded primitive sequences (ADR-004 amendment): one class
  // per element type.
  register_sequence<bool>(m, "BoolSequence");
  register_sequence<uint8_t>(m, "UInt8Sequence");
  register_sequence<uint16_t>(m, "UInt16Sequence");
  register_sequence<uint32_t>(m, "UInt32Sequence");
  register_sequence<uint64_t>(m, "UInt64Sequence");
  register_sequence<int8_t>(m, "Int8Sequence");
  register_sequence<int16_t>(m, "Int16Sequence");
  register_sequence<int32_t>(m, "Int32Sequence");
  register_sequence<int64_t>(m, "Int64Sequence");
  register_sequence<float>(m, "Float32Sequence");
  register_sequence<double>(m, "Float64Sequence");
  register_sequence<long double>(m, "LongDoubleSequence");

  // Aliases for the shared SequenceU8 class (ADR-004 aliasing acceptance).
  m.attr("CharSequence") = m.attr("UInt8Sequence");
  m.attr("ByteSequence") = m.attr("UInt8Sequence");
  m.attr("OctetSequence") = m.attr("UInt8Sequence");
}

}  // namespace rosidl_runtime_cpython