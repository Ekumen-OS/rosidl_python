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

"""Deep in-place copy semantics for experimental messages and containers.

Used by the generated message ``__setattr__``: when a field of a message
backed by external storage is assigned a container or message value, the
source data must be copied *into* the receiver's existing containers (their
identity is fixed — they are views into a loaned buffer).  Managed receivers
instead rebind the field to the source (shallow reference assignment), so no
copy machinery is needed there.

All experimental messages share the same shape (field attributes plus
``_external_storage``), so a single generic implementation serves every
generated message type.
"""

from __future__ import annotations

from typing import Any

from rosidl_runtime_cpython.array import Array
from rosidl_runtime_cpython.dtype import Dtype
from rosidl_runtime_cpython.message_initialization import MessageInitialization
from rosidl_runtime_cpython.scalar import Scalar
from rosidl_runtime_cpython.sequence import Sequence
from rosidl_runtime_cpython.string import String, WString


def _string_copy(src: String | WString) -> String | WString:
    """Create a fresh managed string container holding a copy of *src*."""
    # Use the __buffer__ protocol to read the source bytes; assign() copies
    # directly from the memoryview without materialising a temporary bytes.
    if hasattr(src, 'max_size'):
        dst = type(src)(src.max_size)
    else:
        dst = type(src)()
    dst.assign(memoryview(src))
    return dst


def _element_copy(src: Any, dtype: Dtype | type[Any]) -> Any:
    """Deep-copy a single object-mode sequence element."""
    if isinstance(dtype, Dtype):
        return src  # primitive elements are copied by value by the caller
    if isinstance(src, (String, WString)):
        return _string_copy(src)
    # Nested message element.
    dst = type(src)(_init=MessageInitialization.SKIP)
    deepcopy_into(dst, src)
    return dst


def deepcopy_into(dst: Any, src: Any) -> None:
    """
    Copy *src*'s data into *dst*'s existing containers, in place and deep.

    *dst* may be an experimental message (its ``__slots__`` fields are walked,
    recursing into nested messages) or a single container.  Container
    identities in *dst* are preserved — no field is rebound — so the copy is
    valid when *dst* is backed by external storage.

    Type compatibility is the caller's responsibility (the generated
    ``__setattr__`` enforces it): *src* must be the same message type or the
    same container type as *dst*.
    """
    if isinstance(dst, Scalar):
        dst.value = src.value
    elif isinstance(dst, (String, WString)):
        dst.assign(memoryview(src))
    elif isinstance(dst, Array):
        if isinstance(dst.dtype, Dtype):
            # Fixed-length primitive array: slice-assign in place.  Lengths
            # must match (numpy raises ValueError otherwise).
            dst[:] = src[:]
        else:
            for i in range(len(dst)):
                deepcopy_into(dst[i], src[i])
    elif isinstance(dst, Sequence):
        if isinstance(dst.dtype, Dtype):
            # Primitive sequence: resize the backing buffer and copy values
            # in place.  Raises BufferError if the buffer is external and
            # *src* exceeds its capacity.
            dst.assign(src)
        elif getattr(dst, '_element_pool', None) is not None:
            # Pool-backed object sequence: assign() copies each source
            # element *into* the fixed pool elements (deep in-place, identity
            # preserved) and raises BufferError if *src* exceeds the pool.
            dst.assign(src)
        else:
            # Managed object sequence: rebuild with deep element copies so no
            # element is shared with *src*.
            dst.assign([_element_copy(e, dst.dtype) for e in src])
    else:
        # Experimental message: walk fields, skip internal storage descriptor.
        for name in dst.__slots__:
            if name == '_external_storage':
                continue
            deepcopy_into(getattr(dst, name), getattr(src, name))
