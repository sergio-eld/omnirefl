#pragma once

#include <omnirefl/compat.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace omni {
namespace traits {

/**
 * A predicate and operation in a finite, ordered type-dispatch table.
 */
template <bool Condition, typename Operation>
struct case_ {};

/**
 * Select the first matching case's operation; the last type is the fallback.
 * Only the selected operation is used, so other operation bodies need not be
 * valid for the input. This works in C++11 without if constexpr.
 *
 * Use this for a finite set of operations that callers cannot extend.
 * Overlapping predicates would make enable_if overloads ambiguous; ordering
 * the cases makes precedence explicit and keeps every choice in one place.
 * Readers can see the whole operation set without searching for scattered
 * overload declarations.
 * An extensible customization point should instead allow user overloads.
 *
 * C++20 example inside a function template with input type T:
 *
```cpp
// bool is also integral: the boolean operation must take precedence.
const auto write = typename omni::traits::select<
  omni::traits::case_<std::is_same_v<T, bool>, write_boolean>,
  omni::traits::case_<std::is_integral_v<T>, write_integer>,
  unsupported>::type{};

write(value); // For T=bool, calls write_boolean with no competing overload.
```
 *
 * In C++11, use std::is_same<T, bool>::value and std::is_integral<T>::value
 * for the predicates; the selection and invocation are unchanged.
 */
template <typename... Case>
struct select;

template <typename Fallback>
struct select<Fallback> {
  using type = Fallback;
};

template <bool Condition, typename Operation, typename... Case>
struct select<case_<Condition, Operation>, Case...> {
  using type = omni::compat::conditional_t<Condition,
    Operation,
    typename select<Case...>::type>;
};

namespace detail {

// Function templates cannot be partially specialized, so the public `is`
// overloads delegate template-shape matching to these class templates.
template <template <typename...> class, typename>
struct is_type_template_specialization: std::false_type {};

template <template <typename...> class Template, typename... Argument>
struct is_type_template_specialization<Template, Template<Argument...>>:
    std::true_type {};

template <template <typename T, T> class, typename>
struct is_type_value_template_specialization: std::false_type {};

#if defined(__cpp_nontype_template_parameter_auto)
// auto also matches independent value-parameter types, e.g. array<int, size_t>.
template <template <typename T, T> class Template, typename Type, auto Value>
struct is_type_value_template_specialization<Template, Template<Type, Value>>:
    std::true_type {};
#else
// C++11 can match a constant whose type is the preceding template parameter.
template <template <typename T, T> class Template, typename Type, Type Value>
struct is_type_value_template_specialization<Template, Template<Type, Value>>:
    std::true_type {};
#endif

template <template <typename...> class Left,
  template <typename...> class Right>
struct is_same_template: std::false_type {};

template <template <typename...> class Template>
struct is_same_template<Template, Template>: std::true_type {};

// Detects the result selected by the type-parameter-only `fn::ctad` overload.
// Valid specializations expose the result as `type`.
template <template <typename...> class Template,
  typename Enable,
  typename... Values>
struct type_template_construct_result_impl: std::false_type {};

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
template <template <typename...> class Template, typename... Values>
struct type_template_construct_result_impl<Template,
  compat::void_t<decltype(Template{std::declval<Values>()...})>,
  Values...>: std::true_type {
  using type = decltype(Template{std::declval<Values>()...});
};
#else
template <template <typename...> class Template, typename... Values>
struct type_template_construct_result_impl<Template,
  compat::void_t<decltype(Template<compat::decay_t<Values>...>{
    std::declval<Values>()...})>,
  Values...>: std::true_type {
  using type = Template<compat::decay_t<Values>...>;
};
#endif

template <template <typename...> class Template, typename... Values>
struct type_template_construct_result:
    type_template_construct_result_impl<Template, void, Values...> {};

// Detects the CTAD result selected by the type-and-size `fn::ctad` overload.
// It remains invalid when deduction guides are unavailable.
template <template <typename, std::size_t> class Template,
  typename Value,
  typename = void>
struct type_size_template_construct_result: std::false_type {};

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
template <template <typename, std::size_t> class Template, typename Value>
struct type_size_template_construct_result<Template,
  Value,
  compat::void_t<decltype(Template{std::declval<Value>()})>>: std::true_type {
  using type = decltype(Template{std::declval<Value>()});
};
#endif

} // namespace detail

/**
 * Whether `Type` is a specialization of a template whose parameters are
 * types.
 */
template <template <typename...> class Template, typename Type>
constexpr bool is() noexcept {
  return detail::is_type_template_specialization<Template,
    compat::remove_cvref_t<Type>>::value;
}

/**
 * Whether `Type` specializes a template taking a type and a constant.
 * Before C++17, the constant must have the preceding parameter's type.
 */
template <template <typename T, T> class Template, typename Type>
constexpr bool is() noexcept {
  return detail::is_type_value_template_specialization<Template,
    compat::remove_cvref_t<Type>>::value;
}

#if defined(__cpp_nontype_template_parameter_auto)
/** Return the constant held by a one-value class template. */
template <template <auto> class Template, auto Value>
constexpr decltype(Value) template_value(Template<Value>) noexcept {
  return Value;
}
#else
/** Return the constant held by a type-and-value class template. */
template <template <typename T, T> class Template, typename T, T Value>
constexpr T template_value(Template<T, Value>) noexcept {
  return Value;
}
#endif

/** Report whether `To` can be brace-constructed from `Value`. */
template <typename To, typename Value, typename = void>
struct is_brace_constructible: std::false_type {};

template <typename To, typename Value>
struct is_brace_constructible<To,
  Value,
  compat::void_t<decltype(To{std::declval<Value>()})>>: std::true_type {};

/** Decompose a function signature from a function or pointer type. */
template <typename Function>
struct function_signature;

template <typename Return, typename... Argument>
struct function_signature<Return(Argument...)> {
  using return_type = Return;
  using argument_types = std::tuple<Argument...>;
};

template <typename Return, typename... Argument>
struct function_signature<Return (*)(Argument...)>:
    function_signature<Return(Argument...)> {};

template <typename Return, typename Class, typename... Argument>
struct function_signature<Return (Class::*)(Argument...)>:
    function_signature<Return(Argument...)> {};

#define OMNI_FUNCTION_SIGNATURE_MEMBER(Qualifiers)                        \
  template <typename Return, typename Class, typename... Argument>         \
  struct function_signature<Return (Class::*)(Argument...) Qualifiers>:    \
      function_signature<Return(Argument...)> {}

OMNI_FUNCTION_SIGNATURE_MEMBER(const);
OMNI_FUNCTION_SIGNATURE_MEMBER(volatile);
OMNI_FUNCTION_SIGNATURE_MEMBER(const volatile);
OMNI_FUNCTION_SIGNATURE_MEMBER(&);
OMNI_FUNCTION_SIGNATURE_MEMBER(const &);
OMNI_FUNCTION_SIGNATURE_MEMBER(volatile &);
OMNI_FUNCTION_SIGNATURE_MEMBER(const volatile &);
OMNI_FUNCTION_SIGNATURE_MEMBER(&&);
OMNI_FUNCTION_SIGNATURE_MEMBER(const &&);
OMNI_FUNCTION_SIGNATURE_MEMBER(volatile &&);
OMNI_FUNCTION_SIGNATURE_MEMBER(const volatile &&);

#if defined(__cpp_noexcept_function_type)
template <typename Return, typename... Argument>
struct function_signature<Return(Argument...) noexcept>:
    function_signature<Return(Argument...)> {};

template <typename Return, typename... Argument>
struct function_signature<Return (*)(Argument...) noexcept>:
    function_signature<Return(Argument...)> {};

OMNI_FUNCTION_SIGNATURE_MEMBER(noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(const noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(volatile noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(const volatile noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(& noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(const & noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(volatile & noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(const volatile & noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(&& noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(const && noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(volatile && noexcept);
OMNI_FUNCTION_SIGNATURE_MEMBER(const volatile && noexcept);
#endif

#undef OMNI_FUNCTION_SIGNATURE_MEMBER

/**
 * Report whether a type-parameter-only class template can construct from
 * `Values` using CTAD or the pre-C++17 decayed-argument fallback.
 */
template <template <typename...> class Template, typename... Values>
struct is_type_template_constructible_from:
    detail::type_template_construct_result<Template, Values...> {};

/** The result selected by `is_type_template_constructible_from`. */
template <template <typename...> class Template, typename... Values>
using type_template_construct_result_t =
  typename detail::type_template_construct_result<Template, Values...>::type;

/** Report whether a type-and-size class template can use CTAD with `Value`. */
template <template <typename, std::size_t> class Template, typename Value>
struct is_type_size_template_constructible_from:
    detail::type_size_template_construct_result<Template, Value> {};

/** The result selected by `is_type_size_template_constructible_from`. */
template <template <typename, std::size_t> class Template, typename Value>
using type_size_template_construct_result_t =
  typename detail::type_size_template_construct_result<Template, Value>::type;

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
/** Require brace construction of `To` from `Value`. */
template <typename To, typename Value>
concept brace_constructible_from = is_brace_constructible<To, Value>::value;

/** Require a valid type-parameter-only template construction from `Values`. */
template <template <typename...> class Template, typename... Values>
concept type_template_constructible_from =
  is_type_template_constructible_from<Template, Values...>::value;

/** Require valid CTAD for a type-and-size template from `Value`. */
template <template <typename, std::size_t> class Template, typename Value>
concept type_size_template_constructible_from =
  is_type_size_template_constructible_from<Template, Value>::value;
#endif

/** Compare two types. */
template <typename Left, typename Right>
constexpr bool is() {
  return std::is_same<Left, Right>::value;
}

/** Compare two templates containing only type template parameters. */
template <template <typename...> class Left,
  template <typename...> class Right>
constexpr bool is() {
  return detail::is_same_template<Left, Right>::value;
}

} // namespace traits
} // namespace omni
