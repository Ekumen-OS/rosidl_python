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

"""CPython runtime support for ROS 2 experimental message types.

The runtime is provided by the compiled extensions in this package:

- ``_primitives``: the native typed wrappers (ScalarWrapper, StringWrapper,
  SequenceWrapper, ArrayWrapper) over the experimental C++ containers.
- ``_raw_buffer`` / ``_raw_buffer_cpp``: the RawBuffer C API bridge.
- ``_simulated``: test-only simulated experimental messages.

Generated message bindings (``rosidl_generator_py``) build on ``_primitives``
and the C++ headers installed under ``include/rosidl_runtime_cpython/``.
"""