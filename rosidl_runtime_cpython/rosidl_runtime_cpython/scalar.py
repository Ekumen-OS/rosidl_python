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

"""Experimental Scalar type for rosidl_runtime_py."""

from __future__ import annotations

import numbers
from typing import Any

import numpy as np
import numpy.typing as npt

from rosidl_runtime_cpython._raw_buffer import RawBuffer
from rosidl_runtime_cpython.dtype import Dtype, dtype_from_numpy


class Scalar:
    """
    A single ROS primitive value backed by a RawBuffer.

    The value is stored as a length-1 numpy array inside a RawBuffer, giving
    zero-copy interoperability with numpy.  Numeric protocol methods
    (__int__, __float__, __bool__, __index__) forward to the underlying value.

    Construction
    ------------
    Scalar(dtype, value=0)
        Allocate managed storage and initialise to *value*.

    Scalar(dtype, buffer=buf)
        Use an existing RawBuffer (managed or external) as backing storage.
        *buf* must have size >= dtype.itemsize.

    The *buffer* keyword argument is the sole mechanism for wrapping an
    external rosidl_memory_region_t — callers in C/C++ create the RawBuffer
    via RawBuffer_FromRegion() and pass it here.
    """

    __slots__ = ('_dtype', '_buffer', '_view')

    def __init__(
        self,
        dtype: Dtype,
        value: Any = 0,
        *,
        buffer: RawBuffer | None = None,
    ) -> None:
        """
        Initialise a Scalar.

        Parameters
        ----------
        dtype : Dtype
            The ROS primitive type of this scalar.
        value : Any, optional
            Initial value when *buffer* is None.  Ignored if *buffer* is given.
        buffer : RawBuffer, optional
            Existing RawBuffer to use as storage.  Must have size >=
            dtype.itemsize.  If None, a managed RawBuffer is allocated.
        """
        if not isinstance(dtype, Dtype):
            raise TypeError(f'dtype must be a Dtype, got {type(dtype).__name__}')
        self._dtype = dtype

        if buffer is not None:
            if not isinstance(buffer, RawBuffer):
                raise TypeError(
                    f'buffer must be a RawBuffer, got {type(buffer).__name__}')
            if buffer.size < dtype.itemsize:
                raise ValueError(
                    f'buffer size {buffer.size} is smaller than '
                    f'itemsize {dtype.itemsize} for {dtype!r}')
            self._buffer = buffer
        else:
            self._buffer = RawBuffer(dtype.itemsize)

        self._view: npt.NDArray[Any] = np.frombuffer(
            self._buffer, dtype=dtype.numpy_dtype)

        if buffer is None:
            self._view[0] = value

    # ------------------------------------------------------------------
    # Value access
    # ------------------------------------------------------------------

    @property
    def dtype(self) -> Dtype:
        """Dtype: the ROS primitive type of this scalar."""
        return self._dtype

    @property
    def value(self) -> Any:
        """Current scalar value as a Python scalar."""
        return self._view[0].item()

    @value.setter
    def value(self, v: Any) -> None:
        self._view[0] = np.array(v).astype(self._dtype.numpy_dtype)

    # ------------------------------------------------------------------
    # Numeric protocol
    # ------------------------------------------------------------------

    def __int__(self) -> int:
        return int(self._view[0])

    def __float__(self) -> float:
        return float(self._view[0])

    def __bool__(self) -> bool:
        return bool(self._view[0])

    def __index__(self) -> int:
        # Only valid for integer-typed scalars; numpy will raise for floats.
        if not np.issubdtype(self._dtype.numpy_dtype, np.integer):
            raise TypeError(f'__index__ is only valid for integer dtypes, got {self._dtype}')
        return self._view[0].__index__()

    # ------------------------------------------------------------------
    # Arithmetic helpers
    # ------------------------------------------------------------------

    def _coerce_operand(self, other: Any) -> Any:
        """
        Return the numpy operand for arithmetic, or ``NotImplemented``.

        Accepts :class:`Scalar` (its length-1 view is used, so numpy
        broadcasting applies) and any :class:`numbers.Number` (Python
        int/float/bool and numpy scalars).  Anything else is rejected so
        Python raises the usual ``TypeError`` for unsupported operands.
        """
        if isinstance(other, Scalar):
            return other._view
        if isinstance(other, numbers.Number):
            return other
        return NotImplemented

    def _wrap_result(self, result: npt.NDArray[Any]) -> Scalar:
        """Wrap a length-1 numpy result array into a new managed Scalar."""
        dtype = dtype_from_numpy(result.dtype)
        if dtype is None:
            raise TypeError(
                f'unsupported result dtype {result.dtype} for Scalar arithmetic')
        return Scalar(dtype, result[0])

    def _binary(self, other: Any, op: Any) -> Scalar | Any:
        """Compute ``op(self, other)`` returning a new Scalar."""
        operand = self._coerce_operand(other)
        if operand is NotImplemented:
            return NotImplemented
        return self._wrap_result(op(self._view, operand))

    def _rbinary(self, other: Any, op: Any) -> Scalar | Any:
        """Compute ``op(other, self)`` returning a new Scalar."""
        operand = self._coerce_operand(other)
        if operand is NotImplemented:
            return NotImplemented
        return self._wrap_result(op(operand, self._view))

    def _inplace(self, other: Any, op: Any) -> Scalar | Any:
        """
        Compute ``op(self, other)`` and store the result in *self*.

        The dtype is promoted when required (e.g. ``Scalar(INT32) += 0.5``
        becomes a FLOAT64 scalar); a promoted dtype replaces the backing
        buffer with fresh managed storage.
        """
        operand = self._coerce_operand(other)
        if operand is NotImplemented:
            return NotImplemented
        return self._store_result(op(self._view, operand))

    def _store_result(self, result: npt.NDArray[Any]) -> Scalar:
        """Store a length-1 numpy result array into *self*, promoting dtype."""
        dtype = dtype_from_numpy(result.dtype)
        if dtype is None:
            raise TypeError(
                f'unsupported result dtype {result.dtype} for Scalar arithmetic')
        if dtype != self._dtype:
            if not self._buffer.growing:
                # External (non-owning) storage cannot hold a wider dtype;
                # silently swapping in managed storage would break the
                # zero-copy aliasing with the C/C++ side.  Mirror the strict
                # external-capacity behaviour of Sequence/String.
                raise BufferError(
                    f'Scalar dtype promotion from {self._dtype} to {dtype} '
                    f'requires managed storage, but the backing buffer is '
                    f'external (non-owning)')
            self._dtype = dtype
            self._buffer = RawBuffer(dtype.itemsize)
            self._view = np.frombuffer(self._buffer, dtype=dtype.numpy_dtype)
        self._view[0] = result[0]
        return self

    @staticmethod
    def _exponent_is_negative(operand: Any) -> bool:
        """Return True if *operand* (a numpy value or Python number) is negative."""
        try:
            if isinstance(operand, np.ndarray):
                return bool(operand[0] < 0)
            return bool(operand < 0)
        except TypeError:
            # Complex operands are not orderable; the subsequent power
            # operation yields a complex result with no ROS dtype anyway.
            return False

    # ------------------------------------------------------------------
    # Binary arithmetic
    # ------------------------------------------------------------------

    def __add__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.add)

    def __radd__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.add)

    def __sub__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.subtract)

    def __rsub__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.subtract)

    def __mul__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.multiply)

    def __rmul__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.multiply)

    def __truediv__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.true_divide)

    def __rtruediv__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.true_divide)

    def __floordiv__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.floor_divide)

    def __rfloordiv__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.floor_divide)

    def __mod__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.mod)

    def __rmod__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.mod)

    def __pow__(self, other: Any) -> Scalar | Any:
        operand = self._coerce_operand(other)
        if operand is NotImplemented:
            return NotImplemented
        # Match Python: integer ** negative exponent yields a float result
        # (numpy raises "Integers to negative integer powers are not allowed").
        if (np.issubdtype(self._view.dtype, np.integer)
                and self._exponent_is_negative(operand)):
            return self._wrap_result(
                np.power(self._view.astype(np.float64), operand))
        return self._wrap_result(np.power(self._view, operand))

    def __rpow__(self, other: Any) -> Scalar | Any:
        operand = self._coerce_operand(other)
        if operand is NotImplemented:
            return NotImplemented
        if (np.issubdtype(self._view.dtype, np.integer)
                and self._view[0] < 0):
            return self._wrap_result(
                np.power(operand, self._view.astype(np.float64)))
        return self._wrap_result(np.power(operand, self._view))

    # ------------------------------------------------------------------
    # Bitwise arithmetic
    # ------------------------------------------------------------------

    def __and__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.bitwise_and)

    def __rand__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.bitwise_and)

    def __or__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.bitwise_or)

    def __ror__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.bitwise_or)

    def __xor__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.bitwise_xor)

    def __rxor__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.bitwise_xor)

    def __lshift__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.left_shift)

    def __rlshift__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.left_shift)

    def __rshift__(self, other: Any) -> Scalar | Any:
        return self._binary(other, np.right_shift)

    def __rrshift__(self, other: Any) -> Scalar | Any:
        return self._rbinary(other, np.right_shift)

    # ------------------------------------------------------------------
    # Unary arithmetic
    # ------------------------------------------------------------------

    def __neg__(self) -> Scalar:
        return self._wrap_result(np.negative(self._view))

    def __pos__(self) -> Scalar:
        return self._wrap_result(np.positive(self._view))

    def __abs__(self) -> Scalar:
        return self._wrap_result(np.abs(self._view))

    def __invert__(self) -> Scalar:
        return self._wrap_result(np.invert(self._view))

    # ------------------------------------------------------------------
    # In-place arithmetic
    # ------------------------------------------------------------------

    def __iadd__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.add)

    def __isub__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.subtract)

    def __imul__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.multiply)

    def __itruediv__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.true_divide)

    def __ifloordiv__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.floor_divide)

    def __imod__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.mod)

    def __ipow__(self, other: Any) -> Scalar | Any:
        operand = self._coerce_operand(other)
        if operand is NotImplemented:
            return NotImplemented
        if (np.issubdtype(self._view.dtype, np.integer)
                and self._exponent_is_negative(operand)):
            return self._store_result(
                np.power(self._view.astype(np.float64), operand))
        return self._store_result(np.power(self._view, operand))

    def __iand__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.bitwise_and)

    def __ior__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.bitwise_or)

    def __ixor__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.bitwise_xor)

    def __ilshift__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.left_shift)

    def __irshift__(self, other: Any) -> Scalar | Any:
        return self._inplace(other, np.right_shift)

    # ------------------------------------------------------------------
    # Comparison / hashing
    # ------------------------------------------------------------------

    def __eq__(self, other: object) -> bool:
        if isinstance(other, Scalar):
            return self._dtype == other._dtype and self.value == other.value
        return bool(self.value == other)

    def __hash__(self) -> int:
        return hash((self._dtype, self.value))

    # ------------------------------------------------------------------
    # Buffer / numpy protocols
    # ------------------------------------------------------------------

    def __buffer__(self, flags: int) -> memoryview:
        """PEP 688 buffer protocol (Python >= 3.12)."""
        return memoryview(self._buffer)

    def numpy(self) -> npt.NDArray[Any]:
        """
        Return a length-1 numpy array backed by this scalar's RawBuffer.

        The returned array is a *live view* for managed buffers; it becomes
        stale (but valid) if the backing buffer is replaced.
        """
        return self._view

    def __array__(
        self,
        dtype: npt.DTypeLike | None = None,
        copy: bool | None = None,
    ) -> npt.NDArray[Any]:
        """
        Numpy protocol: expose the value as a length-1 array of the right dtype.

        numpy 1.26 prefers the PEP 688 buffer protocol over ``__array__``, so
        on that version ``np.asarray(scalar)`` still sees the raw bytes; on
        numpy versions that honour ``__array__`` the scalar converts as a
        length-1 array of its actual dtype.  The returned view keeps the
        backing :class:`RawBuffer` alive (it is the view's base), so arrays
        created from it never dangle after an in-place dtype promotion.
        """
        arr = self._view
        if dtype is not None:
            arr = arr.astype(dtype)
        if copy:
            arr = arr.copy()
        return arr

    # ------------------------------------------------------------------
    # Representation
    # ------------------------------------------------------------------

    def __repr__(self) -> str:
        return f'Scalar({self._dtype!r}, {self.value!r})'

    def __str__(self) -> str:
        return str(self.value)
