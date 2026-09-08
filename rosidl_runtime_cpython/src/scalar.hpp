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

/// Native statically typed scalar wrappers over rosidl_runtime_cpp::Scalar<T>
/// (ADR-001/002/003/004/011): non-owning reference views with checked
/// coercion, numpy promotion, buffer/NumPy views, as_builtin/from_builtin,
/// and test-only .Make() factories (aliasing shared_ptr fixtures).

#ifndef ROSIDL_RUNTIME_CPYTHON__SRC__SCALAR_HPP_
#define ROSIDL_RUNTIME_CPYTHON__SRC__SCALAR_HPP_

#include "primitives_common.hpp"

namespace rosidl_runtime_cpython
{

// Forward declaration (defined below).
template<typename T>
class ScalarWrapper;

// Fixture data owner for standalone scalars (test-only .Make() factories,
// ADR-011): the owner holds the Scalar<T>; the wrapper is a non-owning view.
template<typename T>
struct ScalarData
{
  Scalar<T> data;
  ScalarWrapper<T> wrapper;
  explicit ScalarData(T value)
  : data(value), wrapper(&data)
  {
  }
};

template<typename T>
class ScalarWrapper
{
public:
  static constexpr Kind kind = TypeKind<T>::value;

  // Non-owning: references existing Scalar<T> storage (ADR-011). `parent` is
  // the Python parent object (message handle) that keeps the storage alive;
  // empty for standalone fixtures.
  explicit ScalarWrapper(Scalar<T> * data, py::object parent = {})
  : data_(data), parent_(std::move(parent))
  {
  }

  // Test-only factory: creates a fixture owner and returns an aliasing
  // shared_ptr to the non-owning wrapper (ADR-011).
  static std::shared_ptr<ScalarWrapper<T>> make(T value)
  {
    auto owner = std::make_shared<ScalarData<T>>(value);
    return std::shared_ptr<ScalarWrapper<T>>(owner, &owner->wrapper);
  }

  T get() const
  {
    return data_->get();
  }

  void set(T v)
  {
    data_->get() = v;
  }

  Scalar<T> & scalar()
  {
    return *data_;
  }

  const Scalar<T> & scalar() const
  {
    return *data_;
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
    return py::bool_(std::visit([](auto x, auto y) {
      using Common = std::common_type_t<decltype(x), decltype(y)>;
      return static_cast<Common>(x) == static_cast<Common>(y);
    }, a, b));
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
      std::visit([&](auto x, auto y) { \
        using Common = std::common_type_t<decltype(x), decltype(y)>; \
        r = static_cast<Common>(x) OP static_cast<Common>(y); \
      }, a, b)
    if (std::strcmp(op, "<") == 0) { CMP(<); }
    else if (std::strcmp(op, "<=") == 0) { CMP(<=); }
    else if (std::strcmp(op, ">") == 0) { CMP(>); }
    else if (std::strcmp(op, ">=") == 0) { CMP(>=); }
    #undef CMP
    return py::bool_(r);
  }

  py::object ne(py::handle other) const
  {
    return py::bool_(!py::cast<bool>(eq(other)));
  }

  py::object divmod(py::handle other) const
  {
    py::object q = binary(other, Op::FloorDiv);
    py::object r = binary(other, Op::Mod);
    if (q.is_none() || r.is_none()) {
      return not_implemented();
    }
    py::tuple result(2);
    result[0] = q;
    result[1] = r;
    return result;
  }

  py::object rdivmod(py::handle other) const
  {
    py::object q = rbinary(other, Op::FloorDiv);
    py::object r = rbinary(other, Op::Mod);
    if (q.is_none() || r.is_none()) {
      return not_implemented();
    }
    py::tuple result(2);
    result[0] = q;
    result[1] = r;
    return result;
  }

  py::object round_(py::handle ndigits) const
  {
    if (ndigits.is_none()) {
      if constexpr (std::is_integral_v<T>) {
        return py::int_(get());
      } else {
        return py::float_(static_cast<double>(get())).attr("__round__")();
      }
    }
    if constexpr (std::is_integral_v<T>) {
      return py::int_(get()).attr("__round__")(ndigits);
    } else {
      return py::float_(static_cast<double>(get())).attr("__round__")(ndigits);
    }
  }

  py::object ceil() const
  {
    if constexpr (std::is_integral_v<T>) {
      return py::int_(get());
    } else {
      return py::float_(static_cast<double>(get())).attr("__ceil__")();
    }
  }

  py::object floor() const
  {
    if constexpr (std::is_integral_v<T>) {
      return py::int_(get());
    } else {
      return py::float_(static_cast<double>(get())).attr("__floor__")();
    }
  }

  py::object trunc() const
  {
    if constexpr (std::is_integral_v<T>) {
      return py::int_(get());
    } else {
      return py::float_(static_cast<double>(get())).attr("__trunc__")();
    }
  }

  py::object format(py::handle spec) const
  {
    std::string s = py::cast<std::string>(spec);
    if constexpr (std::is_integral_v<T>) {
      return py::int_(get()).attr("__format__")(py::str(s));
    } else {
      return py::float_(static_cast<double>(get())).attr("__format__")(py::str(s));
    }
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
      const_cast<T *>(&data_->get()),
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
    py::array_t<T> arr({1}, &data_->get(), self_obj);
    return arr;
  }

  // ---- arithmetic (builtin-return policy, ADR-002 amendment) --------------

  py::object binary(py::handle other, Op op) const
  {
    auto n = normalize_operands(other, op, /* exponent_is_self */ false);
    if (!n) {
      return not_implemented();
    }
    ValueVariant result = apply_binary(n->self, n->other, op);
    return value_to_py(n->promoted, result);
  }

  py::object rbinary(py::handle other, Op op) const
  {
    auto n = normalize_operands(other, op, /* exponent_is_self */ true);
    if (!n) {
      return not_implemented();
    }
    ValueVariant result = apply_binary(n->other, n->self, op);
    return value_to_py(n->promoted, result);
  }

  py::object inplace(py::handle other, Op op)
  {
    auto n = normalize_operands(other, op, /* exponent_is_self */ false);
    if (!n) {
      return not_implemented();
    }
    ValueVariant result = apply_binary(n->self, n->other, op);
    if (n->promoted == kind) {
      // Result fits the wrapper's storage: mutate in place, return self.
      set(std::get<T>(result));
      return py::cast(this, py::return_value_policy::reference);
    }
    // Promoted kind differs: return the builtin result (Python rebinds the
    // name; the underlying storage is unchanged).
    return value_to_py(n->promoted, result);
  }

  py::object unary(Op op) const
  {
    ValueVariant result = apply_unary(make_variant<kind>(get()), op);
    return value_to_py(kind, result);
  }

  py::object abs_() const
  {
    if constexpr (std::is_same_v<T, bool>) {
      return py::bool_(get());
    } else if constexpr (std::is_integral_v<T>) {
      using UPT = std::make_unsigned_t<T>;
      T v = get();
      T r = v < 0 ? static_cast<T>(static_cast<UPT>(0) - static_cast<UPT>(v)) : v;
      return value_to_py(kind, make_variant<kind>(r));
    } else {
      return py::float_(static_cast<double>(std::fabs(get())));
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

  Scalar<T> * data_;
  py::object parent_;  // anchor (empty for standalone fixtures)
};

// ============================================================================
// Registration
// ============================================================================

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
  py::class_<ScalarWrapper<T>, std::shared_ptr<ScalarWrapper<T>>> cls(
    m, name, py::buffer_protocol());
  cls
    .def(py::init([](py::handle v) {
      if (v.is_none()) {
        return ScalarWrapper<T>::make(T{});
      }
      return ScalarWrapper<T>::make(coerce_scalar<T>(v, std::is_same_v<T, uint8_t>));
    }), py::arg("value") = py::none())
    .def_static("Make", [](py::handle v) {
      // Same coercion as the constructor: range-checked (OverflowError on
      // out-of-range), aliased uint8 accepts 1-char str / 1-byte bytes.
      return ScalarWrapper<T>::make(coerce_scalar<T>(v, std::is_same_v<T, uint8_t>));
    }, py::arg("value"))
    .def_property("value", &ScalarWrapper<T>::get_value, &ScalarWrapper<T>::set_value)
    .def("as_builtin", &ScalarWrapper<T>::as_builtin)
    .def("from_builtin", &ScalarWrapper<T>::from_builtin, py::arg("value"))
    .def("__int__", &ScalarWrapper<T>::int_)
    .def("__float__", &ScalarWrapper<T>::float_)
    .def("__index__", &ScalarWrapper<T>::index_)
    .def("__bool__", &ScalarWrapper<T>::bool_)
    .def("__eq__", &ScalarWrapper<T>::eq)
    .def("__ne__", &ScalarWrapper<T>::ne)
    .def("__divmod__", &ScalarWrapper<T>::divmod)
    .def("__rdivmod__", &ScalarWrapper<T>::rdivmod)
    .def("__round__", &ScalarWrapper<T>::round_, py::arg("ndigits") = py::none())
    .def("__ceil__", &ScalarWrapper<T>::ceil)
    .def("__floor__", &ScalarWrapper<T>::floor)
    .def("__trunc__", &ScalarWrapper<T>::trunc)
    .def("__format__", &ScalarWrapper<T>::format, py::arg("spec"))
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

// Registers every scalar wrapper class (defined in scalar.cpp).
void register_scalars(py::module_ & m);

}  // namespace rosidl_runtime_cpython

#endif  // ROSIDL_RUNTIME_CPYTHON__SRC__SCALAR_HPP_