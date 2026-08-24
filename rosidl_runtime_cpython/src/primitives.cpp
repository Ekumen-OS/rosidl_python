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

/// Native statically typed scalar wrappers over rosidl_runtime_cpp::Scalar<T>.
///
/// Phase 1 of the zero-copy CPython overhaul (ADR-001/002/003/004):
/// - One pybind11 class per distinct C++ instantiation (Bool, UInt8, ...).
/// - Aliased IDL types (uint8/octet/byte/char) share the UInt8 class; the
///   wrapper accepts range-checked int, 1-char ASCII str, and 1-byte bytes.
/// - Checked coercion per ADR-003 (reject overflow, lossy float->int,
///   negative->unsigned, bool-as-int, multi-char str) before mutation.
/// - as_builtin()/from_builtin() escape hatch.
/// - Arithmetic with numpy-style promotion (table computed from numpy).
/// - Buffer protocol + zero-copy NumPy views (base keeps the wrapper alive).

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
#include <string>
#include <type_traits>
#include <variant>

#include "rosidl_runtime_cpp/experimental/scalar.hpp"

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
// Coercion (ADR-003)
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
// ScalarWrapper<T>
// ============================================================================

// Forward declarations (defined after the class).
template<typename T>
class ScalarWrapper;
py::object make_wrapper(Kind k, const ValueVariant & v);

template<typename T>
class ScalarWrapper
{
public:
  static constexpr Kind kind = TypeKind<T>::value;

  explicit ScalarWrapper(T v = T{})
  : value_(v)
  {
  }

  T get() const
  {
    return value_.get();
  }

  void set(T v)
  {
    value_ = v;
  }

  Scalar<T> & scalar()
  {
    return value_;
  }

  const Scalar<T> & scalar() const
  {
    return value_;
  }

  // ---- as_builtin / from_builtin ----------------------------------------

  py::object as_builtin() const
  {
    if constexpr (std::is_same_v<T, bool>) {
      return py::bool_(get());
    } else if constexpr (std::is_integral_v<T>) {
      if constexpr (std::is_same_v<T, uint64_t>) {
        return py::int_(static_cast<unsigned long long>(get()));
      } else {
        return py::int_(static_cast<long long>(get()));
      }
    } else {
      return py::float_(static_cast<double>(get()));
    }
  }

  void from_builtin(py::handle h)
  {
    set(coerce_scalar<T>(h, /* is_aliased_uint8 */ std::is_same_v<T, uint8_t>));
  }

  // ---- value property ----------------------------------------------------

  py::object get_value() const
  {
    return as_builtin();
  }

  void set_value(py::handle h)
  {
    from_builtin(h);
  }

  // ---- numeric protocols -------------------------------------------------

  py::object int_() const
  {
    if constexpr (std::is_same_v<T, bool>) {
      return py::int_(get() ? 1 : 0);
    } else if constexpr (std::is_integral_v<T>) {
      if constexpr (std::is_same_v<T, uint64_t>) {
        return py::int_(static_cast<unsigned long long>(get()));
      } else {
        return py::int_(static_cast<long long>(get()));
      }
    } else {
      return py::int_(static_cast<long long>(get()));
    }
  }

  py::object float_() const
  {
    return py::float_(static_cast<double>(get()));
  }

  py::object index_() const
  {
    if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
      if constexpr (std::is_same_v<T, uint64_t>) {
        return py::int_(static_cast<unsigned long long>(get()));
      } else {
        return py::int_(static_cast<long long>(get()));
      }
    }
    throw py::type_error("__index__ is only valid for integer dtypes");
  }

  py::object bool_() const
  {
    return py::bool_(static_cast<bool>(get()));
  }

  // ---- comparison --------------------------------------------------------

  py::object eq(py::handle other) const
  {
    auto other_val = classify_operand(other);
    if (!other_val) {
      return py::bool_(false);
    }
    auto [_, other_kind, other_value] = *other_val;
    // Value-based comparison whenever a promotion rule exists (user decision):
    // Int8(1) == UInt8(1) is True; Int32(5) == 5 is True.
    // Known limitation (matches numpy): cross-type equality that promotes to
    // float64 can compare unequal large integers as equal (precision loss),
    // e.g. UInt64(2**63+1) == Int64(2**63). Their hashes then differ, which
    // violates the dict/set contract for that edge; numpy has the same
    // behavior for scalar promotion.
    Kind p = static_cast<Kind>(kPromotion[static_cast<int>(kind)][static_cast<int>(other_kind)]);
    auto a = convert_to_kind(p, make_variant<kind>(get()));
    auto b = convert_to_kind(p, other_value);
    return py::bool_(std::visit([](auto x, auto y) { return x == y; }, a, b));
  }

  py::object compare(py::handle other, const char * op) const
  {
    auto other_val = classify_operand(other);
    if (!other_val) {
      return py::bool_(false);
    }
    auto [_, other_kind, other_value] = *other_val;
    Kind p = static_cast<Kind>(kPromotion[static_cast<int>(kind)][static_cast<int>(other_kind)]);
    auto a = convert_to_kind(p, make_variant<kind>(get()));
    auto b = convert_to_kind(p, other_value);
    bool r = false;
    #define CMP(OP) \
      std::visit([&](auto x, auto y) { r = x OP y; }, a, b)
    if (std::strcmp(op, "<") == 0) { CMP(<); }
    else if (std::strcmp(op, "<=") == 0) { CMP(<=); }
    else if (std::strcmp(op, ">") == 0) { CMP(>); }
    else if (std::strcmp(op, ">=") == 0) { CMP(>=); }
    #undef CMP
    return py::bool_(r);
  }

  py::object hash() const
  {
    // Hash the VALUE only (not (kind, value)): Int32(1) == 1 must imply
    // hash(Int32(1)) == hash(1) for the dict/set contract.
    return py::int_(py::hash(as_builtin()));
  }

  // ---- representation ----------------------------------------------------

  std::string repr() const
  {
    return class_name() + "(" + py::cast<std::string>(py::str(as_builtin())) + ")";
  }

  std::string str() const
  {
    return py::cast<std::string>(py::str(as_builtin()));
  }

  // ---- buffer / numpy ----------------------------------------------------

  py::buffer_info buffer() const
  {
    // Expose the raw bytes (itemsize elements of uint8), matching the legacy
    // Scalar's byte-level buffer semantics: len(memoryview(s)) == itemsize.
    return py::buffer_info(
      const_cast<T *>(&value_.get()),
      sizeof(uint8_t),
      py::format_descriptor<uint8_t>::format(),
      1,
      {sizeof(T)},
      {1});
  }

  py::array_t<T> numpy()
  {
    py::object self_obj = py::reinterpret_borrow<py::object>(
      py::cast(this, py::return_value_policy::reference));
    py::array_t<T> arr({1}, &value_.get(), self_obj);
    return arr;
  }

  // ---- arithmetic --------------------------------------------------------

  py::object binary(py::handle other, Op op) const
  {
    auto n = normalize_operands(other, op, /* exponent_is_self */ false);
    if (!n) {
      return not_implemented();
    }
    ValueVariant result = apply_binary(n->self, n->other, op);
    return make_wrapper(n->promoted, result);
  }

  py::object rbinary(py::handle other, Op op) const
  {
    auto n = normalize_operands(other, op, /* exponent_is_self */ true);
    if (!n) {
      return not_implemented();
    }
    ValueVariant result = apply_binary(n->other, n->self, op);
    return make_wrapper(n->promoted, result);
  }

  py::object inplace(py::handle other, Op op)
  {
    auto n = normalize_operands(other, op, /* exponent_is_self */ false);
    if (!n) {
      return not_implemented();
    }
    if (n->promoted == kind) {
      ValueVariant result = apply_binary(n->self, n->other, op);
      set(std::get<T>(result));
      return py::cast(this, py::return_value_policy::reference);
    }
    ValueVariant result = apply_binary(n->self, n->other, op);
    return make_wrapper(n->promoted, result);
  }

  py::object unary(Op op) const
  {
    ValueVariant result = apply_unary(make_variant<kind>(get()), op);
    return make_wrapper(kind, result);
  }

  py::object abs_() const
  {
    if constexpr (std::is_same_v<T, bool>) {
      return py::cast(ScalarWrapper<T>(get()));
    } else if constexpr (std::is_integral_v<T>) {
      using UPT = std::make_unsigned_t<T>;
      T v = get();
      T r = v < 0 ? static_cast<T>(static_cast<UPT>(0) - static_cast<UPT>(v)) : v;
      return py::cast(ScalarWrapper<T>(r));
    } else {
      return py::cast(ScalarWrapper<T>(static_cast<T>(std::fabs(get()))));
    }
  }

private:
  // Classify an operand: (is_weak, kind, value). Python scalars are weak
  // (numpy weak-scalar rule); wrappers use the static promotion table.
  static std::optional<std::tuple<bool, Kind, ValueVariant>> classify_operand(py::handle h)
  {
    if (py::isinstance<py::bool_>(h)) {
      return std::make_tuple(true, Kind::Bool, ValueVariant(py::cast<bool>(h)));
    }
    if (py::isinstance<py::int_>(h)) {
      return std::make_tuple(true, Kind::Int64, ValueVariant(py::cast<int64_t>(h)));
    }
    if (py::isinstance<py::float_>(h)) {
      return std::make_tuple(true, Kind::Float64, ValueVariant(py::cast<double>(h)));
    }
    #define TRY_WRAPPER(CPPTYPE) \
      if (py::isinstance<ScalarWrapper<CPPTYPE>>(h)) { \
        auto & w = py::cast<ScalarWrapper<CPPTYPE> &>(h); \
        return std::make_tuple(false, TypeKind<CPPTYPE>::value, ValueVariant(w.get())); \
      }
    TRY_WRAPPER(bool)
    TRY_WRAPPER(uint8_t)
    TRY_WRAPPER(uint16_t)
    TRY_WRAPPER(uint32_t)
    TRY_WRAPPER(uint64_t)
    TRY_WRAPPER(int8_t)
    TRY_WRAPPER(int16_t)
    TRY_WRAPPER(int32_t)
    TRY_WRAPPER(int64_t)
    TRY_WRAPPER(float)
    TRY_WRAPPER(double)
    TRY_WRAPPER(long double)
    #undef TRY_WRAPPER
    return std::nullopt;
  }

  // Classify + normalize both operands to the promoted kind. Returns nullopt
  // for unsupported operands. `exponent_is_self` selects which operand is the
  // exponent for Pow's negative-exponent promotion (binary/inplace: other;
  // rbinary: self).
  struct NormalizedOperands
  {
    Kind promoted;
    ValueVariant self;
    ValueVariant other;
  };

  std::optional<NormalizedOperands> normalize_operands(py::handle other, Op op,
    bool exponent_is_self) const
  {
    auto other_val = classify_operand(other);
    if (!other_val) {
      return std::nullopt;
    }
    auto [is_weak, other_kind, other_value] = *other_val;
    Kind p = promoted_kind(kind, is_weak, other_kind, other_value, op);
    if (op == Op::Pow && is_integer_kind(kind) && is_integer_kind(other_kind)) {
      const ValueVariant & exp = exponent_is_self ? make_variant<kind>(get()) : other_value;
      if (convert_value<Kind::Float64>(exp) < 0.0) {
        p = Kind::Float64;
      }
    }
    return NormalizedOperands{
      p,
      convert_to_kind(p, make_variant<kind>(get())),
      convert_to_kind(p, other_value)};
  }

  static std::string class_name();

  Scalar<T> value_;
};

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
inline py::object make_wrapper(Kind k, const ValueVariant & v)
{
  switch (k) {
    case Kind::Bool: return py::cast(ScalarWrapper<bool>(std::get<bool>(v)));
    case Kind::UInt8: return py::cast(ScalarWrapper<uint8_t>(std::get<uint8_t>(v)));
    case Kind::UInt16: return py::cast(ScalarWrapper<uint16_t>(std::get<uint16_t>(v)));
    case Kind::UInt32: return py::cast(ScalarWrapper<uint32_t>(std::get<uint32_t>(v)));
    case Kind::UInt64: return py::cast(ScalarWrapper<uint64_t>(std::get<uint64_t>(v)));
    case Kind::Int8: return py::cast(ScalarWrapper<int8_t>(std::get<int8_t>(v)));
    case Kind::Int16: return py::cast(ScalarWrapper<int16_t>(std::get<int16_t>(v)));
    case Kind::Int32: return py::cast(ScalarWrapper<int32_t>(std::get<int32_t>(v)));
    case Kind::Int64: return py::cast(ScalarWrapper<int64_t>(std::get<int64_t>(v)));
    case Kind::Float32: return py::cast(ScalarWrapper<float>(std::get<float>(v)));
    case Kind::Float64: return py::cast(ScalarWrapper<double>(std::get<double>(v)));
    case Kind::LongDouble: return py::cast(ScalarWrapper<long double>(std::get<long double>(v)));
    default: throw std::runtime_error("unknown kind");
  }
}

// ============================================================================
// Registration
// ============================================================================

inline py::object dtype_for_kind(Kind k)
{
  static py::object dtype_enum = py::module_::import("rosidl_runtime_cpython.dtype").attr("Dtype");
  switch (k) {
    case Kind::Bool: return dtype_enum.attr("BOOL");
    case Kind::UInt8: return dtype_enum.attr("UINT8");
    case Kind::UInt16: return dtype_enum.attr("UINT16");
    case Kind::UInt32: return dtype_enum.attr("UINT32");
    case Kind::UInt64: return dtype_enum.attr("UINT64");
    case Kind::Int8: return dtype_enum.attr("INT8");
    case Kind::Int16: return dtype_enum.attr("INT16");
    case Kind::Int32: return dtype_enum.attr("INT32");
    case Kind::Int64: return dtype_enum.attr("INT64");
    case Kind::Float32: return dtype_enum.attr("FLOAT32");
    case Kind::Float64: return dtype_enum.attr("FLOAT64");
    case Kind::LongDouble: return dtype_enum.attr("LONG_DOUBLE");
    default: throw std::runtime_error("unknown kind");
  }
}

template<typename T>
std::string ScalarWrapper<T>::class_name()
{
  switch (kind) {
    case Kind::Bool: return "Bool";
    case Kind::UInt8: return "UInt8";
    case Kind::UInt16: return "UInt16";
    case Kind::UInt32: return "UInt32";
    case Kind::UInt64: return "UInt64";
    case Kind::Int8: return "Int8";
    case Kind::Int16: return "Int16";
    case Kind::Int32: return "Int32";
    case Kind::Int64: return "Int64";
    case Kind::Float32: return "Float32";
    case Kind::Float64: return "Float64";
    case Kind::LongDouble: return "LongDouble";
    default: return "Scalar";
  }
}

template<typename T>
void register_scalar(py::module_ & m, const char * name)
{
  py::class_<ScalarWrapper<T>> cls(m, name, py::buffer_protocol());
  cls
    .def(py::init([](py::handle v) {
      if (v.is_none()) {
        return new ScalarWrapper<T>(T{});
      }
      return new ScalarWrapper<T>(coerce_scalar<T>(v, std::is_same_v<T, uint8_t>));
    }), py::arg("value") = py::none())
    .def_property("value", &ScalarWrapper<T>::get_value, &ScalarWrapper<T>::set_value)
    .def_property_readonly("dtype", [](const ScalarWrapper<T> &) {
      return dtype_for_kind(ScalarWrapper<T>::kind);
    })
    .def("as_builtin", &ScalarWrapper<T>::as_builtin)
    .def("from_builtin", &ScalarWrapper<T>::from_builtin, py::arg("value"))
    .def("__int__", &ScalarWrapper<T>::int_)
    .def("__float__", &ScalarWrapper<T>::float_)
    .def("__index__", &ScalarWrapper<T>::index_)
    .def("__bool__", &ScalarWrapper<T>::bool_)
    .def("__eq__", &ScalarWrapper<T>::eq)
    .def("__lt__", [](const ScalarWrapper<T> & s, py::handle o) { return s.compare(o, "<"); })
    .def("__le__", [](const ScalarWrapper<T> & s, py::handle o) { return s.compare(o, "<="); })
    .def("__gt__", [](const ScalarWrapper<T> & s, py::handle o) { return s.compare(o, ">"); })
    .def("__ge__", [](const ScalarWrapper<T> & s, py::handle o) { return s.compare(o, ">="); })
    .def("__hash__", &ScalarWrapper<T>::hash)
    .def("__repr__", &ScalarWrapper<T>::repr)
    .def("__str__", &ScalarWrapper<T>::str)
    .def("numpy", &ScalarWrapper<T>::numpy)
    .def_buffer(&ScalarWrapper<T>::buffer)
    // Binary arithmetic
    .def("__add__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::Add); })
    .def("__radd__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::Add); })
    .def("__sub__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::Sub); })
    .def("__rsub__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::Sub); })
    .def("__mul__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::Mul); })
    .def("__rmul__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::Mul); })
    .def("__truediv__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::TrueDiv); })
    .def("__rtruediv__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::TrueDiv); })
    .def("__floordiv__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::FloorDiv); })
    .def("__rfloordiv__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::FloorDiv); })
    .def("__mod__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::Mod); })
    .def("__rmod__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::Mod); })
    .def("__pow__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::Pow); })
    .def("__rpow__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::Pow); })
    .def("__and__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::And); })
    .def("__rand__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::And); })
    .def("__or__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::Or); })
    .def("__ror__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::Or); })
    .def("__xor__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::Xor); })
    .def("__rxor__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::Xor); })
    .def("__lshift__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::LShift); })
    .def("__rlshift__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::LShift); })
    .def("__rshift__", [](const ScalarWrapper<T> & s, py::handle o) { return s.binary(o, Op::RShift); })
    .def("__rrshift__", [](const ScalarWrapper<T> & s, py::handle o) { return s.rbinary(o, Op::RShift); })
    // Unary arithmetic
    .def("__neg__", [](const ScalarWrapper<T> & s) { return s.unary(Op::Neg); })
    .def("__pos__", [](const ScalarWrapper<T> & s) { return s.unary(Op::Pos); })
    .def("__invert__", [](const ScalarWrapper<T> & s) { return s.unary(Op::Invert); })
    .def("__abs__", &ScalarWrapper<T>::abs_)
    // In-place arithmetic
    .def("__iadd__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::Add); })
    .def("__isub__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::Sub); })
    .def("__imul__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::Mul); })
    .def("__itruediv__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::TrueDiv); })
    .def("__ifloordiv__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::FloorDiv); })
    .def("__imod__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::Mod); })
    .def("__ipow__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::Pow); })
    .def("__iand__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::And); })
    .def("__ior__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::Or); })
    .def("__ixor__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::Xor); })
    .def("__ilshift__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::LShift); })
    .def("__irshift__", [](ScalarWrapper<T> & s, py::handle o) { return s.inplace(o, Op::RShift); });
}

}  // namespace rosidl_runtime_cpython

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
}