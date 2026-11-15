#pragma once

// FIXME: [high] Define encoding conversion for non-char strings through
// the shared operation specialization points, including standard strings.
// Classify them as strings; do not fall through to container mapping.
// Keep unsupported-type diagnostics focused on the conversion requirement.
// See the disabled wide-string case in tests/string_mapping.cpp.

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/detail.hpp>
#include <omnirefl/serialization/ryml/mapping/enum.hpp>
#include <omnirefl/serialization/ryml/mapping/record.hpp>
#include <omnirefl/serialization/ryml/mapping/sequence.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>

namespace omni {
namespace ryml {

/** Write a char-based string as a quoted scalar. */
struct from_string_like {
  template <typename T>
  static constexpr bool supports() { return traits::is_string<T>::value; }

  template <typename T,
    bool BorrowStrings,
    compat::enable_if_t<traits::is_string<T>::value, int> = 0>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    write_state_t<BorrowStrings>) const
    -> void;
};

/** Read scalar text into a string-like type; views borrow tree storage. */
struct to_string_like {
  template <typename T>
  static constexpr bool supports() { return traits::is_string<T>::value; }

  template <typename T,
    typename Config,
    compat::enable_if_t<traits::is_string<T>::value, int> = 0>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    read_state_t<Config> context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

/** Write booleans, integers, and floating-point values as plain scalars. */
struct from_arithmetic {
  template <typename T>
  static constexpr bool supports() { return std::is_arithmetic<T>::value; }

  template <typename T,
    bool BorrowStrings,
    compat::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    write_state_t<BorrowStrings>) const
    -> void;
};

/** Validate scalar format and range before assigning an arithmetic value. */
struct to_arithmetic {
  template <typename T>
  static constexpr bool supports() { return std::is_arithmetic<T>::value; }

  template <typename T,
    typename Config,
    compat::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    read_state_t<Config> context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

/** Write a present optional value or a null scalar. */
struct from_optional_like {
  template <typename T>
  static constexpr bool supports() { return traits::is_optional<T>::value; }

  template <typename T,
    bool BorrowStrings,
    compat::enable_if_t<traits::is_optional<T>::value, int> = 0>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    write_state_t<BorrowStrings> context) const
    -> void;
};

/** Reset on null; otherwise convert and emplace the contained value. */
struct to_optional_like {
  template <typename T>
  static constexpr bool supports() { return traits::is_optional<T>::value; }

  template <typename T,
    typename Config,
    compat::enable_if_t<traits::is_optional<T>::value, int> = 0>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    read_state_t<Config> context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/mapping.hpp>
