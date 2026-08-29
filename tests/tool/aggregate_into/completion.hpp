#pragma once

#include <omnirefl/functional.hpp>
#include <omnirefl/reflection.hpp>

#include <type_traits>
#include <utility>

// std::optional is available starting with C++17.
#if 201703L <= OMNI_CPLUSPLUS
#  include <optional>

namespace ts { // test support

template <typename T>
using optional = std::optional<T>;
using std::nullopt;

} // namespace ts
#else
#  include <tl/optional.hpp>

namespace ts { // test support

template <typename T>
using optional = tl::optional<T>;
using tl::nullopt;

} // namespace ts
#endif

namespace aggregate_test {

template <typename>
struct is_optional: std::false_type {};

template <typename T>
struct is_optional<ts::optional<T>>: std::true_type {};

struct optional_field {
  template <typename Field>
  constexpr bool operator()() const {
    return is_optional<
      typename omni::compat::remove_cvref_t<Field>::type>::value;
  }
};

template <typename Field>
struct supplied_field {
  using field = omni::compat::remove_cvref_t<Field>;
  using type = typename field::type;

  type stored;

  static constexpr auto name() noexcept -> decltype(field::name()) {
    return field::name();
  }

  type &&value() && noexcept {
    return std::move(stored);
  }
};

template <typename>
struct missing_value;

template <>
struct missing_value<int> {
  static int make() {
    return 108;
  }
};

template <typename T>
struct missing_value<ts::optional<T>> {
  static ts::optional<T> make() {
    return ts::nullopt;
  }
};

struct supply_missing {
  template <typename Field>
  supplied_field<Field> operator()(Field) const {
    return {missing_value<typename supplied_field<Field>::type>::make()};
  }
};

} // namespace aggregate_test
