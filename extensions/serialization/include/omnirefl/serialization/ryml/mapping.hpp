#pragma once

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/detail.hpp>
#include <omnirefl/serialization/ryml/mapping/enum.hpp>
#include <omnirefl/serialization/ryml/mapping/record.hpp>
#include <omnirefl/serialization/ryml/mapping/sequence.hpp>

#include <ryml.hpp>

#include <type_traits>

namespace omni {
namespace ryml {

/** Write a char-based string as a quoted scalar. */
struct from_string_like {
  template <typename T>
  static constexpr bool supports() { return traits::is_string<T>::value; }

  template <typename T, typename Config>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    mapping_state<Config>) const
    -> compat::enable_if_t<supports<T>()>;
};

/** Read scalar text into a string-like type; views borrow tree storage. */
struct to_string_like {
  template <typename T>
  static constexpr bool supports() { return traits::is_string<T>::value; }

  template <typename T, typename Config>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    mapping_state<Config> context) const
    -> compat::enable_if_t<supports<T>(),
      with_diagnostics<compat::monostate, diagnostics>>;

  // FIXME: [high] Define encoding conversion for non-char strings through
  // the shared operation specialization points, including standard strings.
  // Classify them as strings; do not fall through to container mapping.
  // Keep unsupported-type diagnostics focused on the conversion requirement.
  // See the disabled wide-string case in tests/string_mapping.cpp.
};

/** Write booleans, integers, and floating-point values as plain scalars. */
struct from_arithmetic {
  template <typename T>
  static constexpr bool supports() { return std::is_arithmetic<T>::value; }

  template <typename T, typename Config>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    mapping_state<Config>) const
    -> compat::enable_if_t<std::is_arithmetic<T>::value>;
};

/** Validate scalar format and range before assigning an arithmetic value. */
struct to_arithmetic {
  template <typename T>
  static constexpr bool supports() { return std::is_arithmetic<T>::value; }

  template <typename T, typename Config>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    mapping_state<Config> context) const
    -> compat::enable_if_t<supports<T>(),
      with_diagnostics<compat::monostate, diagnostics>>;
};

/** Write a present optional value or a null scalar. */
struct from_optional_like {
  template <typename T>
  static constexpr bool supports() { return traits::is_optional<T>::value; }

  template <typename T, typename Config>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    mapping_state<Config> context) const
    -> compat::enable_if_t<traits::is_optional<T>::value>;
};

/** Reset on null; otherwise convert and emplace the contained value. */
struct to_optional_like {
  template <typename T>
  static constexpr bool supports() { return traits::is_optional<T>::value; }

  template <typename T, typename Config>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    mapping_state<Config> context) const
    -> compat::enable_if_t<traits::is_optional<T>::value,
      with_diagnostics<compat::monostate, diagnostics>>;
};

} // namespace ryml
} // namespace omni
