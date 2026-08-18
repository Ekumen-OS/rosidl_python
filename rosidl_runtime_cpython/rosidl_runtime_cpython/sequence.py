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

"""Experimental Sequence and BoundedSequence types for rosidl_runtime_py."""

from __future__ import annotations

from collections.abc import Iterable, Iterator
from typing import Any

import numpy as np
import numpy.typing as npt

from rosidl_runtime_cpython._raw_buffer import RawBuffer
from rosidl_runtime_cpython.dtype import Dtype
from rosidl_runtime_cpython.string import String, WString


class Sequence:
    """
    A variable-length, homogeneous sequence of ROS interface values.

    Two storage modes are selected automatically based on *dtype*:

    **Primitive** (``dtype`` is a :class:`Dtype`)
        Elements are stored contiguously in a :class:`RawBuffer` with
        geometric growth.  Structural mutations may replace the internal
        buffer; prior numpy views remain valid (though stale) until GC.
        Zero-copy interoperability is available via :meth:`numpy` and
        ``__buffer__``.

    **Object** (``dtype`` is a Python type or ``None``)
        Elements are stored in a numpy object array (``dtype=object``).
        A capacity-sized backing array ``_store`` (analogous to the
        primitive ``_buffer``) is managed with geometric growth; ``_view``
        always holds the logical slice ``_store[:_size]``.  :meth:`numpy`
        returns the current logical view.

        When constructed with ``element_pool=`` the element containers are
        **fixed for the lifetime of the sequence** (they view external
        memory).  All mutations copy values *into* those pool elements rather
        than rebinding them, and the capacity equals the pool length —
        appending beyond it raises :exc:`BufferError`.  This mirrors the C++
        ``element_storage_pool_`` semantics.

    Construction
    ------------
    ``Sequence(dtype)``
        Empty sequence with managed storage.

    ``Sequence(dtype, buffer=buf)``
        Use *buf* as backing storage (primitive mode only).  Capacity is
        ``buf.capacity // dtype.itemsize``.  Mutations that exceed it raise
        :exc:`BufferError` for external (non-owning) buffers.

    ``Sequence(dtype, element_pool=pool)``
        Object mode only: use *pool* (a list of pre-constructed element
        containers, one per external buffer) as fixed backing.  Capacity is
        ``len(pool)``; the sequence starts empty and every mutation writes
        into the pool elements.  An empty pool falls back to managed
        (unbounded) storage.

    ``Sequence(..., initial_size=n)``
        Expose *n* elements already present in the external backing
        (zero-copy cast path), clamped to capacity / pool length.

    Notes
    -----
    The sequence does NOT silently promote from external to managed storage.
    Overflow on an external buffer raises :exc:`BufferError`.

    """

    __slots__ = (
        '_dtype', '_is_primitive', '_buffer', '_store', '_view', '_size',
        '_element_pool',
    )

    def __init__(
        self,
        dtype: Dtype | type[Any],
        *,
        data: Any = None,
        buffer: RawBuffer | None = None,
        element_pool: list[Any] | None = None,
        initial_size: int = 0,
    ) -> None:
        self._dtype = dtype
        self._is_primitive = isinstance(dtype, Dtype)
        self._size = 0

        if self._is_primitive:
            if buffer is not None:
                if not isinstance(buffer, RawBuffer):
                    raise TypeError(
                        f'buffer must be a RawBuffer, got {type(buffer).__name__}')
                self._buffer: RawBuffer | None = buffer
            else:
                self._buffer = RawBuffer()
            self._store: npt.NDArray[Any] | None = None
            # *initial_size* exposes elements already present in an external
            # buffer (zero-copy cast path), clamped to capacity.
            if initial_size:
                self._size = min(int(initial_size), self._capacity())
            self._view: npt.NDArray[Any] = self._make_view()
            self._element_pool = None
        else:
            if buffer is not None:
                raise TypeError(
                    'buffer= is only supported for Dtype (primitive) sequences')
            self._buffer = None
            if element_pool is not None:
                if not isinstance(element_pool, list):
                    raise TypeError(
                        f'element_pool must be a list, got '
                        f'{type(element_pool).__name__}')
                pool = list(element_pool)
                if not pool:
                    # An empty pool means "no fixed backing": fall back to
                    # managed (unbounded) storage, mirroring C++ where an
                    # empty element_storage_pool_ disables the pool.
                    self._element_pool = None
                    self._store = np.empty(0, dtype=object)
                else:
                    for elem in pool:
                        if not isinstance(elem, dtype):
                            raise TypeError(
                                f'element_pool entries must be instances of '
                                f'{dtype.__name__}, got {type(elem).__name__}')
                    self._element_pool = pool
                    # Capacity-sized numpy object array referencing the pool
                    # elements: stores PyObject* pointers.
                    self._store = np.empty(len(pool), dtype=object)
                    self._store[:] = pool
                    # *initial_size* exposes pool elements already populated
                    # by a zero-copy cast, clamped to the pool length.
                    if initial_size:
                        self._size = min(int(initial_size), len(pool))
            else:
                self._element_pool = None
                # Capacity-sized numpy object array: stores PyObject* pointers.
                self._store = np.empty(0, dtype=object)
            self._view = self._make_view()

        if data is not None:
            self.extend(data)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _make_view(self) -> npt.NDArray[Any]:
        if self._is_primitive:
            if self._size == 0:
                return np.empty(0, dtype=self._dtype.numpy_dtype)
            return np.frombuffer(
                self._buffer, dtype=self._dtype.numpy_dtype, count=self._size)
        return self._store[:self._size]  # view of the object store

    def _capacity(self) -> int:
        if self._is_primitive:
            return self._buffer.capacity // self._dtype.itemsize
        if self._element_pool is not None:
            return len(self._element_pool)
        return len(self._store)

    def _ensure_capacity(self, needed: int) -> None:
        """Grow backing storage to hold at least *needed* elements."""
        if needed <= self._capacity():
            return
        if self._is_primitive:
            if not self._buffer.growing:
                raise BufferError(
                    f'Sequence capacity {self._capacity()} exceeded for fixed '
                    f'external buffer (needed {needed})')
            new_cap = max(1, self._capacity())
            while new_cap < needed:
                new_cap *= 2
            self._buffer.reserve(new_cap * self._dtype.itemsize)
        elif self._element_pool is not None:
            raise BufferError(
                f'Sequence capacity {len(self._element_pool)} exceeded for '
                f'external element pool (needed {needed})')
        else:
            new_cap = max(4, len(self._store))
            while new_cap < needed:
                new_cap *= 2
            new_store: npt.NDArray[Any] = np.empty(new_cap, dtype=object)
            new_store[:self._size] = self._store[:self._size]
            self._store = new_store

    def _resize_buffer(self, nbytes: int) -> None:
        """
        Set the backing buffer's logical size.

        External (non-owning) buffers cannot be resized — their capacity is
        fixed and the sequence tracks its own logical size — so this is a
        no-op for them.  Callers must ensure *nbytes* <= capacity (via
        :meth:`_ensure_capacity`).
        """
        if self._buffer.growing:
            self._buffer.resize(nbytes)

    def _assign_element(self, element: Any, value: Any) -> None:
        """
        Copy *value* into a pool-backed *element*, preserving its identity.

        Pythonic scalar values (``str``/``bytes``/``memoryview``) are assigned
        into string-family elements.  Container/message values must match the
        element type exactly and are deep-copied in place.
        """
        if isinstance(value, (str, bytes, bytearray, memoryview)):
            if hasattr(element, 'assign'):
                element.assign(value)
                return
            raise TypeError(
                f'cannot assign {type(value).__name__} to '
                f'{type(element).__name__} element')
        if type(value) is not type(element):
            raise TypeError(
                f'cannot assign {type(value).__name__} to '
                f'{type(element).__name__} element')
        # Lazy import: copy.py imports this module at module scope.
        from rosidl_runtime_cpython.copy import deepcopy_into
        deepcopy_into(element, value)

    @staticmethod
    def _clear_element(element: Any) -> None:
        """Reset a pool element's logical content (identity preserved)."""
        clear = getattr(element, 'clear', None)
        if callable(clear):
            clear()

    def _wrap_element(self, value: Any) -> Any:
        """
        Return the stored form of a Pythonic *value* for a managed object sequence.

        The generated C++ size/writer walkers read string elements through
        ``numpy()``, so a sequence must never store a bare Python ``str``: the
        invariant is that every element is a String/WString container (matching
        scalar string fields, which convert via ``assign()``).  ``str``/``bytes``
        values are wrapped into a fresh unbounded container; bounded string
        dtypes cannot be constructed without their bound, so they raise a clear
        error instead of silently storing the raw value (which would fail later,
        deep inside serialization).
        """
        if isinstance(value, (str, bytes, bytearray, memoryview)):
            dtype = self._dtype
            if dtype is String:
                return String(value)
            if dtype is WString:
                return WString(value)
            if isinstance(dtype, type) and issubclass(dtype, (String, WString)):
                raise TypeError(
                    f'cannot assign {type(value).__name__} to a '
                    f'{dtype.__name__} sequence element: bounded string '
                    f'elements require an explicit bound, construct '
                    f'{dtype.__name__}(bound) yourself')
            # Unrelated dtype (e.g. the untyped `object` sequence): store by
            # reference, preserving the container's generic semantics.
            return value
        return value

    def _refresh_view(self) -> None:
        self._view = self._make_view()

    # ------------------------------------------------------------------
    # Properties
    # ------------------------------------------------------------------

    @property
    def dtype(self) -> Dtype | type[Any]:
        """Element type (a :class:`Dtype` or a Python type)."""
        return self._dtype

    @property
    def capacity(self) -> int:
        """int: element capacity without reallocation."""
        return self._capacity()

    # ------------------------------------------------------------------
    # MutableSequence protocol
    # ------------------------------------------------------------------

    def __len__(self) -> int:
        return self._size

    def __getitem__(self, index: int | slice) -> Any:
        if isinstance(index, int):
            if index < 0:
                index += self._size
            if not (0 <= index < self._size):
                raise IndexError('sequence index out of range')
            if self._is_primitive:
                return self._view[index].item()
            return self._store[index]
        if self._is_primitive:
            return list(self._view[index])
        return list(self._view[index])  # view[slice] → object ndarray → list

    def __setitem__(self, index: int | slice, value: Any) -> None:
        if isinstance(index, int):
            if index < 0:
                index += self._size
            if not (0 <= index < self._size):
                raise IndexError('sequence index out of range')
        if self._is_primitive:
            self._view[index] = value
        elif self._element_pool is not None:
            if isinstance(index, int):
                self._assign_element(self._store[index], value)
            else:
                indices = list(range(*index.indices(self._size)))
                values = list(value)
                if len(values) != len(indices):
                    raise ValueError(
                        f'cannot assign {len(values)} elements to a slice of '
                        f'{len(indices)} positions')
                for i, v in zip(indices, values):
                    self._assign_element(self._store[i], v)
        else:
            if isinstance(index, int):
                self._store[index] = self._wrap_element(value)
            else:
                self._view[index] = [self._wrap_element(v) for v in value]

    def __delitem__(self, index: int | slice) -> None:
        if isinstance(index, int):
            if index < 0:
                index += self._size
            if not (0 <= index < self._size):
                raise IndexError('sequence index out of range')
            if self._is_primitive:
                self._view[index:-1] = self._view[index + 1:]
                self._size -= 1
                self._resize_buffer(self._size * self._dtype.itemsize)
            elif self._element_pool is not None:
                # Shift element *content* left; the pool element identities
                # (external backing) are preserved.
                for j in range(index, self._size - 1):
                    self._assign_element(self._store[j], self._store[j + 1])
                self._clear_element(self._store[self._size - 1])
                self._size -= 1
            else:
                self._store[index:self._size - 1] = self._store[index + 1:self._size]
                self._store[self._size - 1] = None  # release the dangling ref
                self._size -= 1
            self._refresh_view()
        else:
            if self._is_primitive:
                for i in sorted(range(*index.indices(self._size)), reverse=True):
                    del self[i]
            elif self._element_pool is not None:
                keep = np.ones(self._size, dtype=bool)
                for i in range(*index.indices(self._size)):
                    keep[i] = False
                surviving = [i for i in range(self._size) if keep[i]]
                n = len(surviving)
                for j, src_idx in enumerate(surviving):
                    if j != src_idx:
                        self._assign_element(self._store[j], self._store[src_idx])
                for i in range(n, self._size):
                    self._clear_element(self._store[i])
                self._size = n
                self._refresh_view()
            else:
                keep = np.ones(self._size, dtype=bool)
                for i in range(*index.indices(self._size)):
                    keep[i] = False
                surviving = np.array(self._store[:self._size][keep], copy=True)
                n = len(surviving)
                self._store[:n] = surviving
                self._store[n:self._size] = None  # release dangling refs
                self._size = n
                self._refresh_view()

    def __iter__(self) -> Iterator[Any]:
        if self._is_primitive:
            for i in range(self._size):
                yield self._view[i].item()
        else:
            for i in range(self._size):
                yield self._store[i]

    def __contains__(self, value: object) -> bool:
        if self._is_primitive:
            return bool(np.any(self._view == value))
        # Object mode: use identity-then-equality for correct Python semantics.
        for i in range(self._size):
            elem = self._store[i]
            if elem is value or elem == value:
                return True
        return False

    def __reversed__(self) -> Iterator[Any]:
        if self._is_primitive:
            for i in range(self._size - 1, -1, -1):
                yield self._view[i].item()
        else:
            for i in range(self._size - 1, -1, -1):
                yield self._store[i]

    def __eq__(self, other: object) -> bool:
        if isinstance(other, Sequence):
            if self._dtype != other._dtype or self._size != other._size:
                return False
            return bool(np.array_equal(self._view, other._view))
        try:
            return bool(np.array_equal(self._view, other))
        except (TypeError, ValueError):
            return NotImplemented  # type: ignore[return-value]

    # ------------------------------------------------------------------
    # Bound hook (overridden by BoundedSequence)
    # ------------------------------------------------------------------

    def _check_bound(self, needed: int) -> None:
        pass

    # ------------------------------------------------------------------
    # Mutation
    # ------------------------------------------------------------------

    def append(self, value: Any) -> None:
        """Append a single element."""
        self._check_bound(self._size + 1)
        if self._is_primitive:
            self._ensure_capacity(self._size + 1)
            self._resize_buffer((self._size + 1) * self._dtype.itemsize)
            self._size += 1
            self._refresh_view()
            self._view[self._size - 1] = value
        else:
            self._ensure_capacity(self._size + 1)
            if self._element_pool is not None:
                self._assign_element(self._store[self._size], value)
            else:
                self._store[self._size] = self._wrap_element(value)
            self._size += 1
            self._refresh_view()

    def extend(self, values: Iterable[Any]) -> None:
        """
        Append all elements from *values*.

        If *values* contains an element that cannot be assigned into the
        backing (e.g. a type mismatch in pool mode), elements before the
        failure may already have been written (partial mutation).
        """
        items = list(values)
        self._check_bound(self._size + len(items))
        if self._is_primitive:
            self._ensure_capacity(self._size + len(items))
            new_size = self._size + len(items)
            self._resize_buffer(new_size * self._dtype.itemsize)
            old_size = self._size
            self._size = new_size
            self._refresh_view()
            for i, v in enumerate(items):
                self._view[old_size + i] = v
        else:
            self._ensure_capacity(self._size + len(items))
            if self._element_pool is not None:
                for i, v in enumerate(items):
                    self._assign_element(self._store[self._size + i], v)
            else:
                for i, v in enumerate(items):
                    self._store[self._size + i] = self._wrap_element(v)
            self._size += len(items)
            self._refresh_view()

    def insert(self, index: int, value: Any) -> None:
        """Insert *value* before *index*."""
        if index < 0:
            index = max(0, self._size + index)
        if index >= self._size:
            self.append(value)
            return
        self._check_bound(self._size + 1)
        if self._is_primitive:
            self._ensure_capacity(self._size + 1)
            self._resize_buffer((self._size + 1) * self._dtype.itemsize)
            self._size += 1
            self._refresh_view()
            self._view[index + 1:] = self._view[index:-1].copy()
            self._view[index] = value
        elif self._element_pool is not None:
            self._ensure_capacity(self._size + 1)
            # Shift element content right; pool identities are preserved.
            for j in range(self._size - 1, index - 1, -1):
                self._assign_element(self._store[j + 1], self._store[j])
            self._assign_element(self._store[index], value)
            self._size += 1
            self._refresh_view()
        else:
            self._ensure_capacity(self._size + 1)
            # Shift elements right from the end to avoid overwriting.
            self._store[index + 1:self._size + 1] = self._store[index:self._size]
            self._store[index] = self._wrap_element(value)
            self._size += 1
            self._refresh_view()

    def pop(self, index: int = -1) -> Any:
        """Remove and return element at *index*."""
        if self._size == 0:
            raise IndexError('pop from empty sequence')
        if index < 0:
            index += self._size
        if not (0 <= index < self._size):
            raise IndexError('pop index out of range')
        if self._element_pool is not None:
            # Deep-copy the value out *before* deleting: deletion shifts
            # content into the fixed pool element, so the returned object
            # must not alias it.
            # Lazy import: copy.py imports this module at module scope.
            from rosidl_runtime_cpython.copy import _element_copy
            value = _element_copy(self._store[index], self._dtype)
            del self[index]
            return value
        value = self._view[index].item() if self._is_primitive else self._store[index]
        del self[index]
        return value

    def remove(self, value: Any) -> None:
        """Remove the first occurrence of *value*."""
        if self._is_primitive:
            for i in range(self._size):
                if self._view[i].item() == value:
                    del self[i]
                    return
        else:
            for i in range(self._size):
                elem = self._store[i]
                if elem is value or elem == value:
                    del self[i]
                    return
        raise ValueError(f'{value!r} is not in sequence')

    def clear(self) -> None:
        """Remove all elements (capacity is preserved)."""
        if self._is_primitive:
            self._size = 0
            self._resize_buffer(0)
        elif self._element_pool is not None:
            for i in range(self._size):
                self._clear_element(self._store[i])
            self._size = 0
        else:
            self._store[:self._size] = None  # release object references
            self._size = 0
        self._refresh_view()

    def resize(self, new_size: int, fill: Any = 0) -> None:
        """
        Set the logical length to *new_size*.

        New elements are initialised to *fill*.  For pool-backed object
        sequences, growing with the default *fill* of 0 exposes the
        pre-constructed pool slots without modifying them.
        Raises :exc:`BufferError` if the backing storage is external and
        *new_size* exceeds its capacity.
        """
        self._check_bound(new_size)
        if self._is_primitive:
            self._ensure_capacity(new_size)
            old_size = self._size
            self._resize_buffer(new_size * self._dtype.itemsize)
            self._size = new_size
            self._refresh_view()
            if new_size > old_size:
                if fill != 0:
                    self._view[old_size:] = fill
                elif not self._buffer.growing:
                    # RawBuffer.resize() zero-fills newly exposed bytes for
                    # managed buffers; external (non-owning) buffers are not
                    # resized, so zero the new slots explicitly.
                    self._view[old_size:new_size] = 0
        elif self._element_pool is not None:
            self._ensure_capacity(new_size)
            old_size = self._size
            if new_size < old_size:
                for i in range(new_size, old_size):
                    self._clear_element(self._store[i])
            elif new_size > old_size and fill != 0:
                for i in range(old_size, new_size):
                    self._assign_element(self._store[i], fill)
            self._size = new_size
            self._refresh_view()
        else:
            self._ensure_capacity(new_size)
            if new_size > self._size:
                self._store[self._size:new_size] = fill
            elif new_size < self._size:
                self._store[new_size:self._size] = None  # release refs
            self._size = new_size
            self._refresh_view()

    def assign(self, values: Iterable[Any]) -> None:
        """
        Replace the contents with *values* (length may differ).

        Primitive sequences resize the backing buffer and copy the values in
        place.  Managed object sequences clear and store the given elements by
        reference (shallow semantics — no element copies are made).
        Pool-backed object sequences copy each value *into* the fixed pool
        elements (deep in-place, identity preserved).

        Raises :exc:`ValueError` if a bound is exceeded, or :exc:`BufferError`
        if the backing storage is external and *values* exceed its capacity.
        If *values* contains an element that cannot be assigned into the
        backing (e.g. a type mismatch in pool mode), elements before the
        failure may already have been written (partial mutation).
        """
        items = list(values)
        self._check_bound(len(items))
        if self._is_primitive:
            self._ensure_capacity(len(items))
            self._resize_buffer(len(items) * self._dtype.itemsize)
            self._size = len(items)
            self._refresh_view()
            if items:
                self._view[:] = items
        elif self._element_pool is not None:
            self._ensure_capacity(len(items))
            old_size = self._size
            for i, v in enumerate(items):
                self._assign_element(self._store[i], v)
            for i in range(len(items), old_size):
                self._clear_element(self._store[i])
            self._size = len(items)
            self._refresh_view()
        else:
            self._store[:self._size] = None  # release previous refs
            self._ensure_capacity(len(items))
            for i, v in enumerate(items):
                self._store[i] = self._wrap_element(v)
            self._size = len(items)
            self._refresh_view()

    # ------------------------------------------------------------------
    # Buffer / numpy protocols
    # ------------------------------------------------------------------

    def __buffer__(self, flags: int) -> memoryview:
        """
        PEP 688 buffer protocol (Python >= 3.12, primitive sequences only).

        Only the logical content is exposed (``_size * itemsize`` bytes),
        never the underlying capacity.
        """
        if not self._is_primitive:
            raise TypeError('__buffer__ is not supported for object-typed sequences')
        return memoryview(self._buffer)[:self._size * self._dtype.itemsize]

    def numpy(self) -> npt.NDArray[Any]:
        """
        Return a numpy array over the live elements.

        For **primitive** sequences the returned array is a snapshot of the
        current buffer; it becomes stale after any mutation that replaces the
        buffer.

        For **object** sequences the returned array is a view of the internal
        object store slice; it also becomes stale after structural mutations
        that grow the store.  Pool-backed object sequences return a
        **read-only** view: write-through would rebind store slots away from
        the fixed pool elements and break the external-backing invariant.
        """
        if self._element_pool is not None:
            view = self._view
            view.setflags(write=False)
            return view
        return self._view

    # ------------------------------------------------------------------
    # Representation
    # ------------------------------------------------------------------

    def __repr__(self) -> str:
        return f'{type(self).__name__}({self._dtype!r}, data={self._view.tolist()!r})'


class BoundedSequence(Sequence):
    """
    A variable-length sequence with an upper bound on its element count.

    All mutations that would exceed *upper_bound* raise :exc:`ValueError`
    before any storage is modified.

    Construction
    ------------
    ``BoundedSequence(dtype, upper_bound)``
        Empty sequence with managed storage, bounded to *upper_bound* elements.

    ``BoundedSequence(dtype, upper_bound, buffer=buf)``
        Use *buf* as backing storage.

    ``BoundedSequence(dtype, upper_bound, element_pool=pool)``
        Object mode only: use *pool* (a list of pre-constructed element
        containers) as fixed backing.  The effective element limit is
        ``min(upper_bound, len(pool))`` — the bound is checked first
        (:exc:`ValueError`), the pool capacity second (:exc:`BufferError`).

    ``Sequence(..., initial_size=n)``
        Expose *n* elements already present in the external backing
        (zero-copy cast path), clamped to capacity / pool length.
    """

    __slots__ = ('_upper_bound',)

    def __init__(
        self,
        dtype: Dtype | type[Any],
        upper_bound: int,
        *,
        data: Any = None,
        buffer: RawBuffer | None = None,
        element_pool: list[Any] | None = None,
        initial_size: int = 0,
    ) -> None:
        if not isinstance(upper_bound, int) or upper_bound <= 0:
            raise ValueError(
                f'upper_bound must be a positive integer, got {upper_bound!r}')
        self._upper_bound = upper_bound
        super().__init__(
            dtype, data=data, buffer=buffer, element_pool=element_pool,
            initial_size=initial_size)

    @property
    def max_size(self) -> int:
        """int: maximum number of elements this sequence can hold."""
        return self._upper_bound

    def _check_bound(self, needed: int) -> None:
        if needed > self._upper_bound:
            raise ValueError(
                f'BoundedSequence upper bound {self._upper_bound} '
                f'exceeded (needed {needed})')

    def __repr__(self) -> str:
        return (
            f'BoundedSequence({self._dtype!r}, {self._upper_bound}, '
            f'data={self._view.tolist()!r})'
        )
