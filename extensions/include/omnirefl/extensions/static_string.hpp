#pragma once

/** Compile-time char strings shared by extension components.
 * cpp11_static_string joins constexpr str() providers; nttp_string accepts
 * string literals from C++20. Neither representation requires reflection.
 * Views refer to static storage and outlive individual string objects. */

#include <omnirefl/extensions/compat.hpp>

#include <cassert>
#include <cstddef>

namespace omni {
namespace detail {

constexpr std::size_t static_string_length(const char *text) {
  return *text ? 1 + static_string_length(text + 1) : 0;
}

constexpr int static_string_compare(compat::string_view left,
  compat::string_view right, std::size_t index = 0) {
  return index == left.size() ? (index == right.size() ? 0 : -1)
    : index == right.size() ? 1
    : static_cast<unsigned char>(left[index])
        == static_cast<unsigned char>(right[index])
      ? static_string_compare(left, right, index + 1)
      : static_cast<unsigned char>(left[index])
          < static_cast<unsigned char>(right[index]) ? -1 : 1;
}

// string-view-lite's search and comparison require C++14 constant evaluation.
// Recursive view operations retain C++11 constant evaluation.
constexpr std::size_t static_string_find(compat::string_view text,
  compat::string_view needle, std::size_t position) {
  return position > text.size() ? compat::string_view::npos
    : needle.empty() ? position
    : needle.size() > text.size() - position ? compat::string_view::npos
    : static_string_compare(
        {text.data() + position, needle.size()}, needle) == 0 ? position
    : static_string_find(text, needle, position + 1);
}

template <typename String>
struct static_string_interface {
  using size_type = std::size_t;
  static constexpr size_type npos = compat::string_view::npos;

  static constexpr const char *data() noexcept { return String::view().data(); }
  static constexpr const char *str() noexcept { return data(); }
  static constexpr size_type size() noexcept { return String::view().size(); }
  static constexpr bool empty() noexcept { return size() == 0; }

  /** @pre position is less than size(). */
  constexpr char operator[](size_type position) const noexcept {
    return assert(position < size()), data()[position];
  }

  constexpr operator compat::string_view() const noexcept {
    return String::view();
  }

  static constexpr bool starts_with(compat::string_view prefix) noexcept {
#if 202002L <= OMNI_CPLUSPLUS
    return String::view().starts_with(prefix);
#else
    return prefix.size() <= size()
      && static_string_compare({data(), prefix.size()}, prefix) == 0;
#endif
  }

  static constexpr bool starts_with(char prefix) noexcept {
    return starts_with(compat::string_view{&prefix, 1});
  }

  static constexpr bool ends_with(compat::string_view suffix) noexcept {
#if 202002L <= OMNI_CPLUSPLUS
    return String::view().ends_with(suffix);
#else
    return suffix.size() <= size()
      && static_string_compare(
        {data() + size() - suffix.size(), suffix.size()}, suffix) == 0;
#endif
  }

  static constexpr bool ends_with(char suffix) noexcept {
    return ends_with(compat::string_view{&suffix, 1});
  }

  static constexpr size_type find(compat::string_view needle,
    size_type position = 0) noexcept {
#if 201402L <= OMNI_CPLUSPLUS
    // The fallback's find misses an empty match at the end of the string.
    return needle.empty() ? (position <= size() ? position : npos)
      : String::view().find(needle, position);
#else
    return static_string_find(String::view(), needle, position);
#endif
  }

  static constexpr size_type find(char needle,
    size_type position = 0) noexcept {
    return find(compat::string_view{&needle, 1}, position);
  }

  static constexpr bool contains(compat::string_view needle) noexcept {
    return find(needle) != npos;
  }

  static constexpr bool contains(char needle) noexcept {
    return find(needle) != npos;
  }

  static constexpr int compare(compat::string_view other) noexcept {
    return static_string_compare(String::view(), other);
  }

  static constexpr bool equals(compat::string_view other) noexcept {
    return compare(other) == 0;
  }
};

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
template <typename String>
constexpr typename static_string_interface<String>::size_type
  static_string_interface<String>::npos;
#endif

template <typename... Strings>
struct static_string_parts;

template <>
struct static_string_parts<> {
  static constexpr std::size_t size = 0;
  static constexpr char at(std::size_t) { return '\0'; }
};

template <typename First, typename... Rest>
struct static_string_parts<First, Rest...> {
  static constexpr std::size_t first_size = static_string_length(First::str());
  static constexpr std::size_t size = first_size
    + static_string_parts<Rest...>::size;

  static constexpr char at(std::size_t index) {
    return index < first_size ? First::str()[index]
      : static_string_parts<Rest...>::at(index - first_size);
  }
};

// Raw array access is constexpr in C++11; std::array::data() is not.
template <std::size_t Size>
struct static_string_buffer { char values[Size]; };

} // namespace detail

/** Concatenated compile-time text from independent string providers.
 * Each Strings::str() returns constexpr, null-terminated char text. No fields
 * or metadata protocol are required. The empty pack represents an empty string.
 *
```cpp
struct work { static constexpr const char *str() { return "work."; } };
struct city { static constexpr const char *str() { return "city"; } };
using path = omni::cpp11_static_string<work, city>;
static_assert(path::starts_with("work."), "");
```
 */
template <typename... Strings>
struct cpp11_static_string:
    detail::static_string_interface<cpp11_static_string<Strings...>> {
private:
  using parts = detail::static_string_parts<Strings...>;
  using storage_type = detail::static_string_buffer<parts::size + 1>;

  template <std::size_t... Index>
  static constexpr storage_type make(compat::index_sequence<Index...>) {
    return {{parts::at(Index)...}};
  }

  static constexpr storage_type storage =
    make(compat::make_index_sequence<parts::size + 1>{});

public:
  static constexpr compat::string_view view() noexcept {
    return {storage.values, parts::size};
  }
};

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
template <typename... Strings>
constexpr typename cpp11_static_string<Strings...>::storage_type
  cpp11_static_string<Strings...>::storage;
#endif

#if 202002L <= OMNI_CPLUSPLUS
namespace detail {

// Public array storage makes the literal a structural C++20 template argument.
template <std::size_t Size>
struct static_string_literal {
  char values[Size];

  constexpr static_string_literal(const char (&text)[Size]) noexcept {
    for (std::size_t index = 0; index < Size; ++index)
      this->values[index] = text[index];
  }
};

} // namespace detail

/** Compile-time text identified by a C++20 string-literal template argument.
 * Embedded null characters count toward size(), as in std::string_view.
 *
```cpp
using path = omni::nttp_string<"work.city">;
static_assert(path::ends_with("city"));
```
 */
template <detail::static_string_literal Text>
struct nttp_string: detail::static_string_interface<nttp_string<Text>> {
  static constexpr compat::string_view view() noexcept {
    return {Text.values, sizeof(Text.values) - 1};
  }
};
#endif

} // namespace omni

/** (QoL) A compile-time string token from a character string literal.
 * text is the literal; the result is nttp_string in C++20 and
 * cpp11_static_string with an anonymous str() provider in earlier standards.
 *
```cpp
const auto prototype = OMNI_META_STRING("work.city");
constexpr decltype(prototype) path{};
static_assert(path.ends_with("city"), "");
```
 * Before C++17, invoking the lambda is not a constant expression. Its result
 * type still carries the text, avoiding separately named provider structs.
 * C++20 can initialize a constexpr object directly from the macro. */
#if 202002L <= OMNI_CPLUSPLUS
#  define OMNI_META_STRING(text) ::omni::nttp_string<text>{}
#else
#  define OMNI_META_STRING(text) \
    ::omni::compat::invoke([] { \
      struct omni_string_source { \
        static constexpr const char *str() noexcept { return text; } \
      }; \
      return ::omni::cpp11_static_string<omni_string_source>{}; \
    })
#endif
