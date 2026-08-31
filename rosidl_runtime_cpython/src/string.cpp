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

/// String/WString wrapper registration (string.hpp). The final module
/// (primitives.cpp) calls register_strings(); template instantiation is
/// implicit here.

#include "string.hpp"

namespace rosidl_runtime_cpython
{

void register_strings(py::module_ & m)
{
  // Unbounded + bounded String / WString (ADR-004 amendment).
  register_string<char>(m, "String");
  register_string<char16_t>(m, "WString");
}

}  // namespace rosidl_runtime_cpython