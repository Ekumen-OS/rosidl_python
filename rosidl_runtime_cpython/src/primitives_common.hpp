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

/// Shared infrastructure for the native typed wrappers: kind system,
/// promotion tables, coercion helpers, and std::visit-based op application.

#ifndef ROSIDL_RUNTIME_CPYTHON__SRC__PRIMITIVES_COMMON_HPP_
#define ROSIDL_RUNTIME_CPYTHON__SRC__PRIMITIVES_COMMON_HPP_

#include <pybind11/buffer_info.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "rosidl_runtime_cpp/experimental/scalar.hpp"
#include "rosidl_runtime_cpp/experimental/string.hpp"

namespace py = pybind11;

using rosidl_runtime_cpp::Scalar;

namespace rosidl_runtime_cpython
{

// ============================================================================
// Kind system
// ============================================================================

enum class Kind : uint8_t
{
  Bool,
  UInt8,
  UInt16,
  UInt32,
  UInt64,
  Int8,
  Int16,
  Int32,
  Int64,
  Float32,
  Float64,
  LongDouble,
  Count,
};

// Binary/unary operation codes.
enum class Op : uint8_t
{
  Add,
  Sub,
  Mul,
  TrueDiv,
  FloorDiv,
  Mod,
  Pow,
  And,
  Or,
  Xor,
  LShift,
  RShift,
  Neg,
  Pos,
  Invert,
};

template<Kind K>
struct KindType;

template<> struct KindType<Kind::Bool> { using type = bool; };
template<> struct KindType<Kind::UInt8> { using type = uint8_t; };
template<> struct KindType<Kind::UInt16> { using type = uint16_t; };
template<> struct KindType<Kind::UInt32> { using type = uint32_t; };
template<> struct KindType<Kind::UInt64> { using type = uint64_t; };
template<> struct KindType<Kind::Int8> { using type = int8_t; };
template<> struct KindType<Kind::Int16> { using type = int16_t; };
template<> struct KindType<Kind::Int32> { using type = int32_t; };
template<> struct KindType<Kind::Int64> { using type = int64_t; };
template<> struct KindType<Kind::Float32> { using type = float; };
template<> struct KindType<Kind::Float64> { using type = double; };
template<> struct KindType<Kind::LongDouble> { using type = long double; };

template<typename T>
struct TypeKind;

template<> struct TypeKind<bool> { static constexpr Kind value = Kind::Bool; };
template<> struct TypeKind<uint8_t> { static constexpr Kind value = Kind::UInt8; };
template<> struct TypeKind<uint16_t> { static constexpr Kind value = Kind::UInt16; };
template<> struct TypeKind<uint32_t> { static constexpr Kind value = Kind::UInt32; };
template<> struct TypeKind<uint64_t> { static constexpr Kind value = Kind::UInt64; };
template<> struct TypeKind<int8_t> { static constexpr Kind value = Kind::Int8; };
template<> struct TypeKind<int16_t> { static constexpr Kind value = Kind::Int16; };
template<> struct TypeKind<int32_t> { static constexpr Kind value = Kind::Int32; };
template<> struct TypeKind<int64_t> { static constexpr Kind value = Kind::Int64; };
template<> struct TypeKind<float> { static constexpr Kind value = Kind::Float32; };
template<> struct TypeKind<double> { static constexpr Kind value = Kind::Float64; };
template<> struct TypeKind<long double> { static constexpr Kind value = Kind::LongDouble; };

using ValueVariant = std::variant<
  bool, uint8_t, uint16_t, uint32_t, uint64_t,
  int8_t, int16_t, int32_t, int64_t, float, double, long double>;

// numpy result_type promotion table (computed from numpy 1.26; indices are
// the Kind enum values). kPromotion[i][j] = promoted kind of types i and j.
static constexpr uint8_t kPromotion[12][12] = {
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
  {1, 1, 2, 3, 4, 6, 6, 7, 8, 9, 10, 11},
  {2, 2, 2, 3, 4, 7, 7, 7, 8, 9, 10, 11},
  {3, 3, 3, 3, 4, 8, 8, 8, 8, 10, 10, 11},
  {4, 4, 4, 4, 4, 10, 10, 10, 10, 10, 10, 11},
  {5, 6, 7, 8, 10, 5, 6, 7, 8, 9, 10, 11},
  {6, 6, 7, 8, 10, 6, 6, 7, 8, 9, 10, 11},
  {7, 7, 7, 8, 10, 7, 7, 7, 8, 10, 10, 11},
  {8, 8, 8, 8, 10, 8, 8, 8, 8, 10, 10, 11},
  {9, 9, 9, 10, 10, 9, 9, 10, 10, 9, 10, 11},
  {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11},
  {11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11},
};

inline bool is_integer_kind(Kind k)
{
  return k >= Kind::UInt8 && k <= Kind::Int64;
}

inline bool is_float_kind(Kind k)
{
  return k >= Kind::Float32 && k <= Kind::LongDouble;
}

// ============================================================================
// Promotion (free functions, co-located with the promotion tables)
// ============================================================================

inline bool fits_in_kind(Kind k, long long v)
{
  switch (k) {
    case Kind::UInt8: return v >= 0 && v <= std::numeric_limits<uint8_t>::max();
    case Kind::UInt16: return v >= 0 && v <= std::numeric_limits<uint16_t>::max();
    case Kind::UInt32: return v >= 0 && v <= std::numeric_limits<uint32_t>::max();
    case Kind::UInt64: return v >= 0;
    case Kind::Int8: return v >= std::numeric_limits<int8_t>::min() && v <= std::numeric_limits<int8_t>::max();
    case Kind::Int16: return v >= std::numeric_limits<int16_t>::min() && v <= std::numeric_limits<int16_t>::max();
    case Kind::Int32: return v >= std::numeric_limits<int32_t>::min() && v <= std::numeric_limits<int32_t>::max();
    case Kind::Int64: return true;
    default: return false;
  }
}

inline long double max_float(Kind k)
{
  switch (k) {
    case Kind::Float32: return std::numeric_limits<float>::max();
    case Kind::Float64: return std::numeric_limits<double>::max();
    case Kind::LongDouble: return std::numeric_limits<long double>::max();
    default: return 0.0L;
  }
}

// numpy weak-scalar rule for a Python int operand.
inline Kind weak_int_promotion(Kind k, long long v)
{
  if (k == Kind::Bool) {
    return Kind::Int64;  // bool + int -> int64 (numpy)
  }
  if (is_integer_kind(k)) {
    if (fits_in_kind(k, v)) {
      return k;
    }
    return Kind::Int64;  // value does not fit: promote to int64
  }
  if (is_float_kind(k)) {
    if (static_cast<long double>(v) <= static_cast<long double>(max_float(k))) {
      return k;
    }
    return Kind::Float64;
  }
  return k;
}

// numpy weak-scalar rule for a Python float operand.
inline Kind weak_float_promotion(Kind k, double v)
{
  if (k == Kind::Bool || is_integer_kind(k)) {
    return Kind::Float64;
  }
  if (is_float_kind(k)) {
    if (std::isnan(v) || std::isinf(v) || std::fabs(v) <= static_cast<double>(max_float(k))) {
      return k;
    }
    return Kind::Float64;
  }
  return k;
}

// Promoted kind for a binary op between self_kind and an operand.
// - Wrapper operand (is_weak == false): static numpy promotion table.
// - Python scalar operand (is_weak == true): numpy weak-scalar rule.
// TrueDiv of two integer kinds promotes to float64 (numpy).
inline Kind promoted_kind(
  Kind self_kind, bool is_weak, Kind other_kind, const ValueVariant & other_value, Op op)
{
  Kind p;
  if (!is_weak) {
    p = static_cast<Kind>(kPromotion[static_cast<int>(self_kind)][static_cast<int>(other_kind)]);
  } else if (other_kind == Kind::Bool) {
    p = self_kind;  // Python bool is weak: fits every numeric kind
  } else if (other_kind == Kind::Int64) {
    p = weak_int_promotion(self_kind, std::get<int64_t>(other_value));
  } else if (other_kind == Kind::Float64) {
    p = weak_float_promotion(self_kind, std::get<double>(other_value));
  } else {
    p = self_kind;
  }
  if (op == Op::TrueDiv && is_integer_kind(self_kind) && is_integer_kind(other_kind)) {
    p = Kind::Float64;  // integer kinds never include LongDouble
  }
  return p;
}

// ============================================================================
// Value conversion helpers
// ============================================================================

template<Kind Target>
typename KindType<Target>::type convert_value(const ValueVariant & v)
{
  return std::visit([](auto x) -> typename KindType<Target>::type {
    return static_cast<typename KindType<Target>::type>(x);
  }, v);
}

template<Kind K>
ValueVariant make_variant(typename KindType<K>::type v)
{
  return ValueVariant(v);
}

// Runtime-dispatched: convert a ValueVariant to the value type of kind k
// (result holds the converted value as that kind's variant alternative).
inline ValueVariant convert_to_kind(Kind k, const ValueVariant & v)
{
  switch (k) {
    case Kind::Bool: return ValueVariant(convert_value<Kind::Bool>(v));
    case Kind::UInt8: return ValueVariant(convert_value<Kind::UInt8>(v));
    case Kind::UInt16: return ValueVariant(convert_value<Kind::UInt16>(v));
    case Kind::UInt32: return ValueVariant(convert_value<Kind::UInt32>(v));
    case Kind::UInt64: return ValueVariant(convert_value<Kind::UInt64>(v));
    case Kind::Int8: return ValueVariant(convert_value<Kind::Int8>(v));
    case Kind::Int16: return ValueVariant(convert_value<Kind::Int16>(v));
    case Kind::Int32: return ValueVariant(convert_value<Kind::Int32>(v));
    case Kind::Int64: return ValueVariant(convert_value<Kind::Int64>(v));
    case Kind::Float32: return ValueVariant(convert_value<Kind::Float32>(v));
    case Kind::Float64: return ValueVariant(convert_value<Kind::Float64>(v));
    case Kind::LongDouble: return ValueVariant(convert_value<Kind::LongDouble>(v));
    default: throw std::runtime_error("unknown kind");
  }
}

inline py::object not_implemented()
{
  return py::reinterpret_borrow<py::object>(Py_NotImplemented);
}

// Raise OverflowError (pybind11 has no py::overflow_error).
inline void raise_overflow(const char * msg)
{
  PyErr_SetString(PyExc_OverflowError, msg);
  throw py::error_already_set();
}

// Raise ZeroDivisionError (pybind11 has no py::zero_division_error).
inline void raise_zero_division(const char * msg)
{
  PyErr_SetString(PyExc_ZeroDivisionError, msg);
  throw py::error_already_set();
}

// ============================================================================
// Compile-time switch (dispatch a runtime index to a compile-time constant)
// ============================================================================

// Invoke `func` with std::integral_constant<size_t, Is> for the Is matching
// `index`; returns std::nullopt when no Is matches. All branches must return
// the same type. The fold over `||` short-circuits after the first match.
template<typename Func, std::size_t... Is>
auto compile_time_switch_impl(
  std::size_t index, Func && func, std::index_sequence<Is...>)
{
  using Result = std::invoke_result_t<Func, std::integral_constant<std::size_t, 0>>;
  std::optional<Result> result;
  (... || (index == Is
    ? (result.emplace(func(std::integral_constant<std::size_t, Is>{})), true)
    : false));
  return result;
}

// Dispatch `index` in [0, N) to a compile-time constant; nullopt otherwise.
template<std::size_t N, typename Func>
auto compile_time_switch(std::size_t index, Func && func)
{
  return compile_time_switch_impl(index, std::move(func), std::make_index_sequence<N>{});
}

// ============================================================================
// Element-type traits
// ============================================================================

// True for element types that map directly to numpy dtypes (the primitive
// scalar types). Message element types (Phase 1B) are NOT numpy-compatible;
// their arrays/sequences use wrapper-based element access instead of numpy
// views / builtin conversion.
template<typename T>
struct is_numpy_compatible : std::bool_constant<std::is_arithmetic_v<T>>
{
};

// ============================================================================
// Scalar coercion (ADR-003)
// ============================================================================

// Range-checked conversion of a Python int to an integer target.
template<typename T>
T coerce_int_to_integer(py::handle h)
{
  static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
  if constexpr (std::is_signed_v<T>) {
    long long v = PyLong_AsLongLong(h.ptr());
    if (v == -1 && PyErr_Occurred()) {
      PyErr_Clear();
      raise_overflow("value out of range for target type");
    }
    constexpr long long lo = static_cast<long long>(std::numeric_limits<T>::min());
    constexpr long long hi = static_cast<long long>(std::numeric_limits<T>::max());
    if (v < lo || v > hi) {
      raise_overflow("value out of range for target type");
    }
    return static_cast<T>(v);
  } else {
    // Unsigned target: detect negative values explicitly (py::cast to
    // unsigned long long would otherwise fail with a generic cast error).
    long long sv = PyLong_AsLongLong(h.ptr());
    if (sv == -1 && PyErr_Occurred()) {
      PyErr_Clear();
      // Too large for long long: use the unsigned path.
      unsigned long long uv = PyLong_AsUnsignedLongLong(h.ptr());
      if (uv == static_cast<unsigned long long>(-1) && PyErr_Occurred()) {
        throw py::error_already_set();
      }
      constexpr unsigned long long hi = static_cast<unsigned long long>(std::numeric_limits<T>::max());
      if (uv > hi) {
        raise_overflow("value out of range for target type");
      }
      return static_cast<T>(uv);
    }
    if (sv < 0) {
      raise_overflow("negative value for unsigned target");
    }
    constexpr unsigned long long hi = static_cast<unsigned long long>(std::numeric_limits<T>::max());
    if (static_cast<unsigned long long>(sv) > hi) {
      raise_overflow("value out of range for target type");
    }
    return static_cast<T>(sv);
  }
}

// Range-checked conversion of a Python int/float to a float target.
template<typename T>
T coerce_number_to_float(py::handle h)
{
  static_assert(std::is_floating_point_v<T>);
  double v;
  if (py::isinstance<py::int_>(h)) {
    v = PyLong_AsDouble(h.ptr());
    if (v == -1.0 && PyErr_Occurred()) {
      PyErr_Clear();
      raise_overflow("value out of range for target type");
    }
  } else {
    v = PyFloat_AsDouble(h.ptr());
    if (v == -1.0 && PyErr_Occurred()) {
      throw py::error_already_set();
    }
  }
  if (std::isnan(static_cast<double>(v)) || std::isinf(static_cast<double>(v))) {
    return static_cast<T>(v);  // NaN/Inf accepted for float targets
  }
  if (v > static_cast<long double>(std::numeric_limits<T>::max()) ||
    v < -static_cast<long double>(std::numeric_limits<T>::max()))
  {
    raise_overflow("value out of range for target type");
  }
  return static_cast<T>(v);
}

// Coerce a Python object to T per ADR-003. `is_aliased_uint8` enables the
// 1-char ASCII str / 1-byte bytes forms for the shared UInt8 class.
template<typename T>
T coerce_scalar(py::handle h, bool is_aliased_uint8)
{
  if (py::isinstance<py::bool_>(h)) {
    if constexpr (std::is_same_v<T, bool>) {
      return py::cast<bool>(h);
    }
    throw py::type_error("bool is only accepted for bool targets");
  }
  if (py::isinstance<py::int_>(h)) {
    if constexpr (std::is_same_v<T, bool>) {
      throw py::type_error("int is not accepted for bool targets");
    } else if constexpr (std::is_integral_v<T>) {
      return coerce_int_to_integer<T>(h);
    } else {
      return coerce_number_to_float<T>(h);
    }
  }
  if (py::isinstance<py::float_>(h)) {
    if constexpr (std::is_same_v<T, bool>) {
      throw py::type_error("float is not accepted for bool targets");
    } else if constexpr (std::is_integral_v<T>) {
      throw py::type_error("lossy float to integer conversion is rejected");
    } else {
      return coerce_number_to_float<T>(h);
    }
  }
  if (py::isinstance<py::str>(h)) {
    if constexpr (std::is_same_v<T, uint8_t>) {
      if (is_aliased_uint8) {
        std::string s = py::cast<std::string>(h);
        if (s.size() == 1 && static_cast<unsigned char>(s[0]) < 128) {
          return static_cast<T>(static_cast<unsigned char>(s[0]));
        }
        throw py::value_error("char/uint8 accepts a 1-character ASCII string");
      }
    }
    throw py::type_error("str is not accepted for this target type");
  }
  if (py::isinstance<py::bytes>(h) || py::isinstance<py::bytearray>(h)) {
    if constexpr (std::is_same_v<T, uint8_t>) {
      if (is_aliased_uint8) {
        std::string b = py::cast<std::string>(h);
        if (b.size() == 1) {
          return static_cast<T>(static_cast<unsigned char>(b[0]));
        }
        throw py::value_error("char/uint8 accepts a 1-byte buffer");
      }
    }
    throw py::type_error("bytes is not accepted for this target type");
  }
  throw py::type_error("unsupported value type for scalar target");
}

// ============================================================================
// Element conversion customization point (ADR-011)
// ============================================================================

// Forward declaration (defined below in the builtin-conversion section).
template<typename T>
py::object element_to_py(T v);

// Convert an element to a Python object and back. The primary template
// handles numpy-compatible (primitive) element types: to_py yields a builtin,
// from_py coerces a builtin. Generated message types specialize this to
// produce/consume message wrappers: to_py returns a parent-anchored message
// view, to_builtin returns a dict, from_py copies a message out of a message
// wrapper (or dict). Element types must provide operator== (used by
// contains/remove); the generator emits it for message types.
template<typename T, typename Enable = void>
struct ElementTraits
{
  // Convert an element to a Python object. `parent` is the container's
  // Python object (ignored for primitives; anchors message views).
  static py::object to_py(T & v, py::object /*parent*/)
  {
    return element_to_py(v);
  }

  // Convert an element to a Python builtin (as_builtin escape hatch).
  static py::object to_builtin(const T & v)
  {
    return element_to_py(v);
  }

  // Extract an element from a Python object (builtin for primitives).
  static T from_py(py::handle h)
  {
    return coerce_scalar<T>(h, std::is_same_v<T, uint8_t>);
  }
};
// ============================================================================
// Arithmetic helpers (Python/numpy semantics)
// ============================================================================

// floordiv with Python semantics (floor toward -inf for integers).
template<typename PT>
PT floor_div(PT a, PT b)
{
  if constexpr (std::is_same_v<PT, bool>) {
    return static_cast<PT>(a / b);  // degenerate; bool floordiv is not meaningful
  } else if constexpr (std::is_integral_v<PT>) {
    PT q = a / b;
    PT r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
      --q;
    }
    return q;
  } else {
    return static_cast<PT>(std::floor(a / b));
  }
}

// mod with Python semantics (result has the sign of the divisor).
template<typename PT>
PT py_mod(PT a, PT b)
{
  if constexpr (std::is_same_v<PT, bool>) {
    return static_cast<PT>(a % b);  // degenerate; bool mod is not meaningful
  } else if constexpr (std::is_integral_v<PT>) {
    PT r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
      r += b;
    }
    return r;
  } else {
    PT r = std::fmod(a, b);
    if (r != 0 && ((r < 0) != (b < 0))) {
      r += b;
    }
    return r;
  }
}

// ============================================================================
// ============================================================================
// Op application via std::visit (the ValueVariant already encodes the type)
// ============================================================================

// Integer pow with wrapping (numpy C semantics), avoiding std::pow precision
// loss. Negative exponents are promoted to float64 before reaching here.
template<typename PT>
PT int_pow(PT base, PT exp)
{
  if constexpr (std::is_same_v<PT, bool>) {
    return base && exp;  // degenerate; bool pow is not meaningful
  } else {
    using UPT = std::make_unsigned_t<PT>;
    UPT result = 1;
    UPT b = static_cast<UPT>(base);
    UPT e = static_cast<UPT>(exp);
    while (e > 0) {
      if (e & 1) {
        result *= b;
      }
      b *= b;
      e >>= 1;
    }
    return static_cast<PT>(result);
  }
}

// Apply a binary op to two values of the same promoted type.
inline ValueVariant apply_binary(const ValueVariant & a, const ValueVariant & b, Op op)
{
  return std::visit([&](auto x, auto y) -> ValueVariant {
    using PT = std::common_type_t<decltype(x), decltype(y)>;
    // std::visit instantiates every (alternative, alternative) pair; convert
    // both operands to the common type so mixed pairs compile (at runtime the
    // operands are always the same promoted type).
    PT xp = static_cast<PT>(x);
    PT yp = static_cast<PT>(y);
    if constexpr (std::is_floating_point_v<PT>) {
      switch (op) {
        case Op::Add: return ValueVariant(static_cast<PT>(xp + yp));
        case Op::Sub: return ValueVariant(static_cast<PT>(xp - yp));
        case Op::Mul: return ValueVariant(static_cast<PT>(xp * yp));
        case Op::TrueDiv: return ValueVariant(static_cast<PT>(xp / yp));
        case Op::FloorDiv:
          if (yp == 0) { raise_zero_division("integer division or modulo by zero"); }
          return ValueVariant(floor_div(xp, yp));
        case Op::Mod:
          if (yp == 0) { raise_zero_division("integer division or modulo by zero"); }
          return ValueVariant(py_mod(xp, yp));
        case Op::Pow: return ValueVariant(static_cast<PT>(std::pow(xp, yp)));
        default: throw py::type_error("bitwise operations are not supported on float types");
      }
    } else if constexpr (std::is_same_v<PT, bool>) {
      // bool: plain C++ semantics (int promotion + cast back); numpy treats
      // bool + bool as logical OR, which the cast reproduces.
      switch (op) {
        case Op::Add: return ValueVariant(static_cast<PT>(xp + yp));
        case Op::Sub: return ValueVariant(static_cast<PT>(xp - yp));
        case Op::Mul: return ValueVariant(static_cast<PT>(xp * yp));
        case Op::TrueDiv:
          if (yp == 0) { raise_zero_division("integer division or modulo by zero"); }
          return ValueVariant(static_cast<PT>(xp / yp));
        case Op::FloorDiv:
          if (yp == 0) { raise_zero_division("integer division or modulo by zero"); }
          return ValueVariant(floor_div(xp, yp));
        case Op::Mod:
          if (yp == 0) { raise_zero_division("integer division or modulo by zero"); }
          return ValueVariant(py_mod(xp, yp));
        case Op::Pow: return ValueVariant(int_pow(xp, yp));
        case Op::And: return ValueVariant(static_cast<PT>(xp & yp));
        case Op::Or: return ValueVariant(static_cast<PT>(xp | yp));
        case Op::Xor: return ValueVariant(static_cast<PT>(xp ^ yp));
        case Op::LShift:
          if (yp < 0) { throw py::value_error("negative shift count"); }
          if (yp >= static_cast<PT>(sizeof(PT) * 8)) { return ValueVariant(static_cast<PT>(0)); }
          return ValueVariant(static_cast<PT>(xp << yp));
        case Op::RShift:
          if (yp < 0) { throw py::value_error("negative shift count"); }
          if (yp >= static_cast<PT>(sizeof(PT) * 8)) {
            return ValueVariant(static_cast<PT>(xp < 0 ? static_cast<PT>(-1) : static_cast<PT>(0)));
          }
          return ValueVariant(static_cast<PT>(xp >> yp));
        default: throw std::runtime_error("unknown binary op");
      }
    } else {
      // Integer arithmetic: compute in the unsigned counterpart so signed
      // overflow wraps (numpy C semantics) instead of being UB.
      using UPT = std::make_unsigned_t<PT>;
      switch (op) {
        case Op::Add: return ValueVariant(static_cast<PT>(static_cast<UPT>(xp) + static_cast<UPT>(yp)));
        case Op::Sub: return ValueVariant(static_cast<PT>(static_cast<UPT>(xp) - static_cast<UPT>(yp)));
        case Op::Mul: return ValueVariant(static_cast<PT>(static_cast<UPT>(xp) * static_cast<UPT>(yp)));
        case Op::TrueDiv:
          if (yp == 0) { raise_zero_division("integer division or modulo by zero"); }
          return ValueVariant(static_cast<PT>(xp / yp));
        case Op::FloorDiv:
          if (yp == 0) { raise_zero_division("integer division or modulo by zero"); }
          return ValueVariant(floor_div(xp, yp));
        case Op::Mod:
          if (yp == 0) { raise_zero_division("integer division or modulo by zero"); }
          return ValueVariant(py_mod(xp, yp));
        case Op::Pow: return ValueVariant(int_pow(xp, yp));
        case Op::And: return ValueVariant(static_cast<PT>(xp & yp));
        case Op::Or: return ValueVariant(static_cast<PT>(xp | yp));
        case Op::Xor: return ValueVariant(static_cast<PT>(xp ^ yp));
        case Op::LShift:
          if (yp < 0) { throw py::value_error("negative shift count"); }
          if (yp >= static_cast<PT>(sizeof(PT) * 8)) { return ValueVariant(static_cast<PT>(0)); }
          return ValueVariant(static_cast<PT>(static_cast<UPT>(xp) << yp));
        case Op::RShift:
          if (yp < 0) { throw py::value_error("negative shift count"); }
          if (yp >= static_cast<PT>(sizeof(PT) * 8)) {
            return ValueVariant(static_cast<PT>(xp < 0 ? static_cast<PT>(-1) : static_cast<PT>(0)));
          }
          // Arithmetic shift on the signed value (implementation-defined but
          // arithmetic on gcc/clang; matches numpy sign extension).
          return ValueVariant(static_cast<PT>(xp >> yp));
        default: throw std::runtime_error("unknown binary op");
      }
    }
  }, a, b);
}

// Apply a unary op to a value.
inline ValueVariant apply_unary(const ValueVariant & v, Op op)
{
  return std::visit([&](auto x) -> ValueVariant {
    using PT = decltype(x);
    if constexpr (std::is_floating_point_v<PT>) {
      switch (op) {
        case Op::Neg: return ValueVariant(static_cast<PT>(-x));
        case Op::Pos: return ValueVariant(static_cast<PT>(+x));
        default: throw py::type_error("bitwise operations are not supported on float types");
      }
    } else if constexpr (std::is_same_v<PT, bool>) {
      switch (op) {
        case Op::Neg: return ValueVariant(static_cast<PT>(!x));
        case Op::Pos: return ValueVariant(static_cast<PT>(x));
        case Op::Invert: return ValueVariant(static_cast<PT>(!x));
        default: throw std::runtime_error("unknown unary op");
      }
    } else {
      using UPT = std::make_unsigned_t<PT>;
      switch (op) {
        case Op::Neg: return ValueVariant(static_cast<PT>(static_cast<UPT>(0) - static_cast<UPT>(x)));
        case Op::Pos: return ValueVariant(static_cast<PT>(+x));
        case Op::Invert: return ValueVariant(static_cast<PT>(~x));
        default: throw std::runtime_error("unknown unary op");
      }
    }
  }, v);
}


// ============================================================================
// Value -> Python builtin conversion (builtin-return policy, ADR-002
// amendment): value-producing operations return Python builtins, not wrappers.
// ============================================================================

// Convert a C++ value to a Python builtin (int/float/bool).
template<typename T>
py::object element_to_py(T v)
{
  if constexpr (std::is_same_v<T, bool>) {
    return py::bool_(v);
  } else if constexpr (std::is_integral_v<T>) {
    if constexpr (std::is_same_v<T, uint64_t>) {
      return py::int_(static_cast<unsigned long long>(v));
    } else {
      return py::int_(static_cast<long long>(v));
    }
  } else {
    return py::float_(static_cast<double>(v));
  }
}

// Convert a ValueVariant (of the given kind) to a Python builtin.
inline py::object value_to_py(Kind k, const ValueVariant & v)
{
  switch (k) {
    case Kind::Bool: return py::bool_(std::get<bool>(v));
    case Kind::UInt8: return py::int_(std::get<uint8_t>(v));
    case Kind::UInt16: return py::int_(std::get<uint16_t>(v));
    case Kind::UInt32: return py::int_(std::get<uint32_t>(v));
    case Kind::UInt64: return py::int_(static_cast<unsigned long long>(std::get<uint64_t>(v)));
    case Kind::Int8: return py::int_(std::get<int8_t>(v));
    case Kind::Int16: return py::int_(std::get<int16_t>(v));
    case Kind::Int32: return py::int_(std::get<int32_t>(v));
    case Kind::Int64: return py::int_(std::get<int64_t>(v));
    case Kind::Float32: return py::float_(std::get<float>(v));
    case Kind::Float64: return py::float_(std::get<double>(v));
    case Kind::LongDouble: return py::float_(static_cast<double>(std::get<long double>(v)));
    default: throw std::runtime_error("unknown kind");
  }
}

// ============================================================================
// Shared numpy/buffer helpers (array and sequence wrappers)
// ============================================================================

// True if the buffer info describes a C-contiguous layout.
inline bool is_c_contiguous(const py::buffer_info & info)
{
  py::ssize_t expected = info.itemsize;
  for (py::ssize_t i = info.ndim - 1; i >= 0; --i) {
    if (info.strides[i] != expected) {
      return false;
    }
    expected *= info.shape[i];
  }
  return true;
}

// Extract (data, count) from a C-contiguous numpy array of dtype T or a
// C-contiguous buffer; nullopt otherwise. Primitive-only: message element
// types have no numpy/buffer representation.
template<typename T>
std::optional<std::pair<const T *, size_t>> typed_buffer_from_numpy_or_buffer(py::handle h)
{
  if constexpr (is_numpy_compatible<T>::value) {
    if (py::isinstance<py::array>(h)) {
      py::array arr = py::cast<py::array>(h);
      if (arr.dtype().is(py::dtype::of<T>())) {
        auto info = arr.request();
        if (is_c_contiguous(info)) {
          return std::make_pair(static_cast<const T *>(info.ptr), info.size);
        }
      }
    }
    if (py::isinstance<py::buffer>(h)) {
      py::buffer buf = py::cast<py::buffer>(h);
      auto info = buf.request();
      if (info.itemsize == sizeof(T) &&
        info.format == py::format_descriptor<T>::format() &&
        is_c_contiguous(info))
      {
        return std::make_pair(
          static_cast<const T *>(info.ptr),
          info.size * info.itemsize / sizeof(T));
      }
    }
  }
  return std::nullopt;
}

// A numpy array over a materialized vector. bool has no data(), so it is
// built element-wise; other types are zero-copy views over the vector.
template<typename T>
py::array_t<T> to_numpy(const std::vector<T> & vec)
{
  if constexpr (std::is_same_v<T, bool>) {
    py::array_t<T> arr({static_cast<py::ssize_t>(vec.size())});
    auto r = arr.template mutable_unchecked<1>();
    for (size_t i = 0; i < vec.size(); ++i) {
      r(static_cast<py::ssize_t>(i)) = vec[i];
    }
    return arr;
  } else {
    return py::array_t<T>(
      {static_cast<py::ssize_t>(vec.size())}, vec.data());
  }
}

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SRC__PRIMITIVES_COMMON_HPP_
