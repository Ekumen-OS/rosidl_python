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


// make_wrapper: construct a py::object of the wrapper class for a kind.

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SRC__PRIMITIVES_COMMON_HPP_
