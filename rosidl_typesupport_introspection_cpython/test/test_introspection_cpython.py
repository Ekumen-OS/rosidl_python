# Copyright 2026 Ekumen Inc.
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

"""Tests for the rosidl_typesupport_introspection_cpython generator."""

import os
import subprocess
import sys


def test_generator_function_is_importable():
    """The generator entry point function must be importable and callable."""
    from rosidl_typesupport_introspection_cpython import generate_cpython_introspection

    assert callable(generate_cpython_introspection)


def test_generator_entry_point_requires_arguments_file():
    """--generator-arguments-file is required by the CLI."""
    from rosidl_typesupport_introspection_cpython import __file__ as _pkg_init

    pkg_root = os.path.dirname(os.path.dirname(_pkg_init))
    bin_script = os.path.join(pkg_root, 'bin', 'rosidl_typesupport_introspection_cpython')
    if not os.path.exists(bin_script):
        # Installed layout: script lives in lib/rosidl_typesupport_introspection_cpython
        bin_script = os.path.join(
            os.path.dirname(pkg_root), 'lib', 'rosidl_typesupport_introspection_cpython',
            'rosidl_typesupport_introspection_cpython')
    if not os.path.exists(bin_script):
        return  # not installed; nothing to check
    proc = subprocess.run(
        [sys.executable, bin_script], capture_output=True, text=True)
    assert proc.returncode != 0
    assert '--generator-arguments-file' in proc.stderr
