#pragma once

#include <omnirefl/extensions/compat.hpp>

namespace omni {
namespace traits {
namespace detail {

// Detection stays in the defaults so user partial specializations replace the
// public primary templates rather than competing with built-in specializations.
template <typename String, typename = void>
struct default_as_string {};

template <typename String>
struct default_as_string<String,
  compat::void_t<decltype(compat::basic_string_view<typename String::value_type,
    typename String::traits_type>{std::declval<const String &>().data(),
    std::declval<const String &>().size()})>> {
  constexpr auto operator()(const String &value) const
    -> compat::basic_string_view<typename String::value_type,
      typename String::traits_type> {
    return {value.data(), value.size()};
  }
};

template <typename String, typename = void>
struct default_assign_string {};

template <typename String>
struct default_assign_string<String,
  compat::void_t<typename String::traits_type,
    decltype(String{}),
    decltype(std::declval<String &>() = String{
      std::declval<const typename String::value_type *>(),
      std::declval<std::size_t>()})>> {
  void operator()(String *into,
    compat::basic_string_view<typename String::value_type,
      typename String::traits_type> text) const {
    // Owning constructors need not accept an empty view's null data pointer.
    *into = text.empty() ? String{} : String{text.data(), text.size()};
  }
};

template <typename Optional, typename = void>
struct default_has_value {};

template <typename Optional>
struct default_has_value<Optional,
  compat::void_t<decltype(std::declval<const Optional &>().has_value())>> {
  constexpr auto operator()(const Optional &value) const
    -> decltype(value.has_value()) { return value.has_value(); }
};

template <typename Optional, typename = void>
struct default_get_value {};

template <typename Optional>
struct default_get_value<Optional,
  compat::void_t<decltype(*std::declval<const Optional &>())>> {
  constexpr auto operator()(const Optional &value) const
    -> decltype(*value) { return *value; }
};

template <typename Optional, typename = void>
struct default_reset {};

template <typename Optional>
struct default_reset<Optional,
  compat::void_t<decltype(std::declval<Optional &>().reset())>> {
  auto operator()(Optional *into) const -> decltype(into->reset()) {
    return into->reset();
  }
};

template <typename Optional, typename = void>
struct default_emplace {};

template <typename Optional>
struct default_emplace<Optional,
  compat::void_t<typename Optional::value_type,
    decltype(std::declval<Optional &>().emplace(
      std::declval<typename Optional::value_type &&>()))>> {
  auto operator()(Optional *into, typename Optional::value_type &&value) const
    -> decltype(into->emplace(std::move(value))) {
    return into->emplace(std::move(value));
  }
};

} // namespace detail

namespace cp {

/** Adapt text to std::basic_string or compat::basic_string_view.
 * A returned string owns converted characters; a returned view borrows from
 * the source and must remain valid while the source is alive and unchanged.
 * Specialize for string APIs requiring conversion or different accessors.
 */
template <typename String, typename = void>
struct as_string: detail::default_as_string<String> {};

/** Assign normalized text. Owning strings copy; views borrow its storage. */
template <typename String, typename = void>
struct assign_string: detail::default_assign_string<String> {};

/** Check engagement; defaults to the type's has_value() member. */
template <typename Optional, typename = void>
struct has_value: detail::default_has_value<Optional> {};

/** Borrow a present value. The caller must first check has_value(). */
template <typename Optional, typename = void>
struct get_value: detail::default_get_value<Optional> {};

/** Disengage a value; defaults to reset(). */
template <typename Optional, typename = void>
struct reset: detail::default_reset<Optional> {};

/** Replace the contained value; defaults to emplace(value). */
template <typename Optional, typename = void>
struct emplace: detail::default_emplace<Optional> {};

} // namespace cp
} // namespace traits

namespace adapt {

/** Deduce the source type and invoke its string adapter. */
template <typename String>
constexpr auto as_string(const String &value)
  -> decltype(traits::cp::as_string<String>{}(value)) {
  return traits::cp::as_string<String>{}(value);
}

/** Deduce the destination type and invoke its text-assignment adapter. */
template <typename String, typename Char, typename Traits>
auto assign_string(String *into, compat::basic_string_view<Char, Traits> text)
  -> decltype(traits::cp::assign_string<String>{}(into, text)) {
  return traits::cp::assign_string<String>{}(into, text);
}

/** Deduce the optional type and invoke its engagement check. */
template <typename Optional>
constexpr auto has_value(const Optional &value)
  -> decltype(traits::cp::has_value<Optional>{}(value)) {
  return traits::cp::has_value<Optional>{}(value);
}

/** Deduce the optional type and borrow its present value. */
template <typename Optional>
constexpr auto get_value(const Optional &value)
  -> decltype(traits::cp::get_value<Optional>{}(value)) {
  return traits::cp::get_value<Optional>{}(value);
}

/** Deduce the optional type and invoke its reset adapter. */
template <typename Optional>
auto reset(Optional *into) -> decltype(traits::cp::reset<Optional>{}(into)) {
  return traits::cp::reset<Optional>{}(into);
}

/** Deduce the optional type and invoke its emplacement adapter. */
template <typename Optional>
auto emplace(Optional *into, typename Optional::value_type &&value)
  -> decltype(traits::cp::emplace<Optional>{}(into, std::move(value))) {
  return traits::cp::emplace<Optional>{}(into, std::move(value));
}

} // namespace adapt

namespace traits {
namespace detail {

// String types supported as results of as_string(), independent of ownership.
template <typename T>
struct is_core_compatible_string: std::false_type {};

template <typename Char, typename Traits, typename Allocator>
struct is_core_compatible_string<std::basic_string<Char, Traits, Allocator>>:
    std::true_type {};

template <typename Char, typename Traits>
struct is_core_compatible_string<compat::basic_string_view<Char, Traits>>:
    std::true_type {};

template <typename T>
using string_storage_t = compat::remove_cvref_t<decltype(
  adapt::as_string(std::declval<const T &>()))>;

template <typename T>
using string_view_t = compat::basic_string_view<
  typename string_storage_t<T>::value_type,
  typename string_storage_t<T>::traits_type>;

} // namespace detail

/** Whether both adapted string operations accept normalized text storage.
 * Customize the operations rather than specializing this detection trait.
 */
template <typename T, typename = void>
struct is_string: std::false_type {};

template <typename T>
struct is_string<T,
  compat::void_t<detail::string_storage_t<T>,
    decltype(adapt::assign_string(std::declval<T *>(),
      std::declval<detail::string_view_t<T>>()))>>:
    detail::is_core_compatible_string<detail::string_storage_t<T>> {};

/** Whether the adapted optional operations are available, with value_type
 * describing the contained type. get_value() borrows it by const reference.
 * No inheritance or explicit registration is required. Customize operations
 * rather than specializing this detection trait.
 */
template <typename T, typename = void>
struct is_optional: std::false_type {};

template <typename T>
struct is_optional<T,
  compat::void_t<typename T::value_type,
    decltype(adapt::has_value(std::declval<const T &>())),
    decltype(adapt::get_value(std::declval<const T &>())),
    decltype(adapt::reset(std::declval<T *>())),
    decltype(adapt::emplace(std::declval<T *>(),
      std::declval<typename T::value_type &&>()))>>:
    compat::conjunction<
      std::is_convertible<decltype(adapt::has_value(
        std::declval<const T &>())), bool>,
      std::is_same<decltype(adapt::get_value(
        std::declval<const T &>())), const typename T::value_type &>> {};

// TODO: [high] Document the generalized operation-adapter pattern:
// traits::cp specializations override primary member-based defaults; dispatchers
// deduce the adapted type and invoke the selected struct without ADL. Traits
// check the complete dispatched protocol and its normalized result types.
// Include template-family specializations and C++11 examples; adapted types
// need neither inheritance nor explicit registration.

// FIXME: [high] [external] Remove tl_optional-emplace.patch after upstream
// fixes emplace() calling value() under -fno-exceptions; fetched copies
// are patched even when constructing the contained value cannot throw.

} // namespace traits
} // namespace omni
