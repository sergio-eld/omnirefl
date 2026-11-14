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

// Prefer sequence insertion, then the generic emplace member. Priority tags
// keep these defaults unambiguous when a type supplies several operations.
template <int Priority>
struct insertion_priority: insertion_priority<Priority - 1> {};

template <>
struct insertion_priority<0> {};

template <typename T, typename... Args>
auto emplace_member(T *into, insertion_priority<2>, Args &&...args)
  -> decltype(into->emplace_back(std::forward<Args>(args)...)) {
  return into->emplace_back(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
auto emplace_member(T *into, insertion_priority<1>, Args &&...args)
  -> decltype(into->push_back(std::forward<Args>(args)...)) {
  return into->push_back(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
auto emplace_member(T *into, insertion_priority<0>, Args &&...args)
  -> decltype(into->emplace(std::forward<Args>(args)...)) {
  return into->emplace(std::forward<Args>(args)...);
}

// Detection keeps member aliases optional in the customizable primary.
template <typename Range, typename = void>
struct default_key_value {};

template <typename Range>
struct default_key_value<Range,
  compat::void_t<typename Range::key_type, typename Range::mapped_type>> {
  using key_type = typename Range::key_type;
  using mapped_type = typename Range::mapped_type;
};

template <typename Range, typename = void>
struct default_has_unique_key: std::false_type {};

template <typename Range>
struct default_has_unique_key<Range,
  compat::void_t<decltype(std::declval<Range &>().emplace(
    std::declval<compat::range_value_t<Range>>()).second)>>:
    std::is_convertible<decltype(std::declval<Range &>().emplace(
      std::declval<compat::range_value_t<Range>>()).second), bool> {};

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

/** Insert arguments; defaults to emplace_back, push_back, then emplace.
 * Also replaces optional contents through its emplace member. */
template <typename T, typename = void>
struct emplace {
  template <typename... Args>
  auto operator()(T *into, Args &&...args) const
    -> decltype(detail::emplace_member(into, detail::insertion_priority<2>{},
      std::forward<Args>(args)...)) {
    return detail::emplace_member(into, detail::insertion_priority<2>{},
      std::forward<Args>(args)...);
  }
};

/** Insert only if the key is absent, returning {iterator, inserted}.
 * On collision, leave the destination and arguments unchanged. The default
 * checks find(key) before emplace so this guarantee also holds in C++11. */
template <typename Range, typename = void>
struct try_emplace {
  template <typename Key, typename... Args, typename T = Range>
  auto operator()(T *into, Key &&key, Args &&...args) const
    -> decltype(std::make_pair(into->find(key),
      bool(into->emplace(std::forward<Key>(key),
        std::forward<Args>(args)...).second))) {
    const auto found = into->find(key);
    if (found != into->end())
      return {found, false};

    const auto inserted = into->emplace(std::forward<Key>(key),
      std::forward<Args>(args)...);

    return {inserted.first, bool(inserted.second)};
  }
};

/** Remove all elements; defaults to the clear member. */
template <typename Range, typename = void>
struct clear {
  template <typename T = Range>
  auto operator()(T *into) const -> decltype(into->clear()) {
    return into->clear();
  }
};

/** Erase an existing position; defaults to the erase member. */
template <typename Range, typename = void>
struct erase {
  template <typename Position, typename T = Range>
  auto operator()(T *into, Position position) const
    -> decltype(into->erase(position)) { return into->erase(position); }
};

/** Replace an existing position with the supplied element or key/value pair.
 * The default composes the adapted erase and emplace operations. Specialize
 * when the range supplies another replacement API. */
template <typename Range, typename = void>
struct replace {
  template <typename Position, typename... Args, typename T = Range>
  auto operator()(T *into, Position position, Args &&...args) const
    -> decltype(erase<T>{}(into, position),
      emplace<T>{}(into, std::forward<Args>(args)...)) {
    erase<T>{}(into, position);
    return emplace<T>{}(into, std::forward<Args>(args)...);
  }
};

/** Associated key and mapped-value types; defaults to the nested aliases.
 * Specialize for key/value ranges exposing those types through another API. */
template <typename Range, typename = void>
struct key_value: detail::default_key_value<Range> {};

/** Whether insertion enforces unique keys (or unique set elements).
 * Defaults to detecting emplace's {iterator, inserted} result. Specialize
 * when custom insertion operations provide this property. */
template <typename Range, typename = void>
struct has_unique_key: detail::default_has_unique_key<Range> {};

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

/** Deduce the destination type and invoke its insertion adapter. */
template <typename T, typename... Args>
auto emplace(T *into, Args &&...args)
  -> decltype(traits::cp::emplace<T>{}(into, std::forward<Args>(args)...)) {
  return traits::cp::emplace<T>{}(into, std::forward<Args>(args)...);
}

/** Insert without consuming arguments when an equivalent key exists. */
template <typename Range, typename... Args>
auto try_emplace(Range *into, Args &&...args)
  -> decltype(traits::cp::try_emplace<Range>{}(into,
    std::forward<Args>(args)...)) {
  return traits::cp::try_emplace<Range>{}(into, std::forward<Args>(args)...);
}

/** Deduce the range type and remove its elements. */
template <typename Range>
auto clear(Range *into) -> decltype(traits::cp::clear<Range>{}(into)) {
  return traits::cp::clear<Range>{}(into);
}

/** Deduce the range type and erase an existing position. */
template <typename Range, typename Position>
auto erase(Range *into, Position position)
  -> decltype(traits::cp::erase<Range>{}(into, position)) {
  return traits::cp::erase<Range>{}(into, position);
}

/** Deduce the destination type and replace an existing position. */
template <typename Range, typename Position, typename... Args>
auto replace(Range *into, Position position, Args &&...args)
  -> decltype(traits::cp::replace<Range>{}(into, position,
    std::forward<Args>(args)...)) {
  return traits::cp::replace<Range>{}(into, position,
    std::forward<Args>(args)...);
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

/** Adapted key type of a key/value range. */
template <typename T>
using key_type_t = typename cp::key_value<compat::remove_cvref_t<T>>::key_type;

/** Adapted mapped-value type of a key/value range. */
template <typename T>
using mapped_type_t =
  typename cp::key_value<compat::remove_cvref_t<T>>::mapped_type;

/** Whether the associated key and mapped-value types are available. */
template <typename T, typename = void>
struct is_key_value: std::false_type {};

template <typename T>
struct is_key_value<T, compat::void_t<key_type_t<T>, mapped_type_t<T>>>:
    std::true_type {};

/** Whether equivalent keys cannot coexist in the adapted range. */
template <typename T>
using has_unique_key = cp::has_unique_key<compat::remove_cvref_t<T>>;

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
// traits::cp specializations override primary member-based defaults;
// dispatchers
// deduce the adapted type and invoke the selected struct without ADL. Traits
// check the complete dispatched protocol and its normalized result types.
// Include template-family specializations and C++11 examples; adapted types
// need neither inheritance nor explicit registration.

// FIXME: [high] [external] Remove tl_optional-emplace.patch after upstream
// fixes emplace() calling value() under -fno-exceptions; fetched copies
// are patched even when constructing the contained value cannot throw.

} // namespace traits
} // namespace omni
