#pragma once

/** Compile-time char strings shared by extension components.
 * cpp11_static_string joins constexpr str() providers; nttp_string accepts
 * string literals with C++20 class-template arguments. Neither requires
 * reflection.
 * Views refer to static storage and outlive individual string objects. */

#include <omnirefl/extensions/compat.hpp>

#include <cassert>
#include <cstddef>
#include <type_traits>

/** Whether nttp_string is available in the current compiler mode (0 or 1). */
// Clang's macro covers more than class arguments; class support starts at 12.
// https://clang.llvm.org/cxx_status.html#cxx20
#if 202002L <= OMNI_CPLUSPLUS \
  && ((defined(__cpp_nontype_template_args) \
        && 201911L <= __cpp_nontype_template_args) \
    || (defined(__cpp_nontype_template_parameter_class) \
        && 201806L <= __cpp_nontype_template_parameter_class) \
    || (defined(__clang__) && 12 <= __clang_major__))
#  define OMNI_HAS_NTTP_STRING 1
#else
#  define OMNI_HAS_NTTP_STRING 0
#endif

namespace omni {

template <typename... Strings>
struct cpp11_static_string;

namespace detail {

// string-view-lite search can call memcmp or std::search, which older compilers
// cannot constant-evaluate. Token types retain both inputs in C++11/14.
template <typename StringIn, typename StringWhat>
constexpr std::size_t static_string_find(std::size_t position) {
  return position > StringIn::size() ? compat::string_view::npos
    : StringWhat::empty() ? position
    : StringWhat::size() > StringIn::size() - position
      ? compat::string_view::npos
    : StringWhat::compare(
        {StringIn::data() + position, StringWhat::size()}) == 0
      ? position : static_string_find<StringIn, StringWhat>(position + 1);
}

// Raw providers supply NTS text; sized tokens retain their explicit lengths.
// Private measurement avoids compiler strlen intrinsics on constexpr buffers.
struct static_text_view {
  template <typename String>
  static constexpr compat::string_view of() { return make(String::str()); }

private:
  static constexpr std::size_t length(const char *text) {
#if 201402L <= OMNI_CPLUSPLUS
    std::size_t size = 0;
    while (text[size])
      ++size;
    return size;
#else
    return *text ? 1 + length(text + 1) : 0;
#endif
  }

  static constexpr compat::string_view make(const char *text) {
    return {text, length(text)};
  }

  static constexpr compat::string_view make(compat::string_view text) {
    return text;
  }
};

// Function templates cannot be partially specialized. Callers still use the
// value-returning string_length<String>(); representations select its source.
template <typename Meta>
struct string_length_selection {
  static constexpr std::size_t get() {
    static_assert(std::is_convertible<decltype(Meta::str()),
        const char *>::value,
      "Meta::str() must supply constexpr NTS char text");
    static_assert(Meta::str() != nullptr
        && Meta::str()[static_text_view::of<Meta>().size()] == '\0',
      "Meta::str() must supply a constexpr null-terminated string");
    return static_text_view::of<Meta>().size();
  }
};

template <typename... Strings>
struct string_length_selection<cpp11_static_string<Strings...>> {
  static constexpr std::size_t get() {
    return cpp11_static_string<Strings...>::size();
  }
};

template <typename String>
constexpr std::size_t string_length() {
  return string_length_selection<String>::get();
}

// C++11 constexpr functions cannot accumulate through local variables.
#if 201402L <= OMNI_CPLUSPLUS
template <typename... Sizes>
constexpr std::size_t sum(std::size_t initial, Sizes... rest) {
  const std::size_t sizes[]{initial, rest...};
  std::size_t result = 0;
  for (const auto size : sizes)
    result += size;
  return result;
}
#else
constexpr std::size_t sum(std::size_t value) { return value; }

template <typename... Sizes>
constexpr std::size_t sum(std::size_t first, std::size_t next, Sizes... rest) {
  return sum(first + next, rest...);
}
#endif

// Raw array access is constexpr in C++11; std::array::data() is not.
template <std::size_t Size>
struct static_string_buffer { char values[Size]; };

#if 201402L <= OMNI_CPLUSPLUS
template <std::size_t Size, typename... Views>
constexpr auto make_static_string(Views... views)
  -> static_string_buffer<Size> {
  const compat::string_view parts[]{compat::string_view{""}, views...};
  static_string_buffer<Size> result{};
  std::size_t index = 0;
  for (const auto part : parts) {
    for (const auto character : part)
      result.values[index++] = character;
  }
  return result;
}
#else
// C++11 needs a character expansion; value-view arguments share helper
// instantiations across unrelated provider types.
constexpr char static_string_at(std::size_t) { return '\0'; }

template <typename... Views>
constexpr char static_string_at(std::size_t index, compat::string_view first,
  Views... rest) {
  return index < first.size() ? first[index]
    : static_string_at(index - first.size(), rest...);
}

template <std::size_t... Index, typename... Views>
constexpr auto make_static_string(compat::index_sequence<Index...>,
  Views... views)
  -> static_string_buffer<sizeof...(Index)> {
  return {{static_string_at(Index, views...)...}};
}

template <std::size_t Size, typename... Views>
constexpr auto make_static_string(Views... views)
  -> static_string_buffer<Size> {
  return make_static_string(compat::make_index_sequence<Size>{}, views...);
}
#endif

} // namespace detail

/** Concatenated compile-time text from independent string providers.
 * Raw providers expose constexpr NTS char text through str(); library string
 * tokens are also supported. No metadata protocol is required. The empty pack
 * represents an empty string.
 *
```cpp
struct work { static constexpr const char *str() { return "work."; } };
struct city { static constexpr const char *str() { return "city"; } };
using path = omni::cpp11_static_string<work, city>;
static_assert(path::starts_with("work."), "");
```
 */
template <typename... Strings>
struct cpp11_static_string {
private:
  static constexpr int compare_views(compat::string_view left,
    compat::string_view right) {
    return left.empty() ? (right.empty() ? 0 : -1)
      : right.empty() ? 1
      : static_cast<unsigned char>(left[0])
          == static_cast<unsigned char>(right[0])
        ? compare_views({left.data() + 1, left.size() - 1},
            {right.data() + 1, right.size() - 1})
        : static_cast<unsigned char>(left[0])
            < static_cast<unsigned char>(right[0]) ? -1 : 1;
  }

  using storage_type = detail::static_string_buffer<
    detail::sum(0, detail::string_length<Strings>()...) + 1>;

  static constexpr storage_type storage =
    detail::make_static_string<sizeof(storage_type::values)>(
      detail::static_text_view::of<Strings>()...);

public:
  using size_type = std::size_t;

  static constexpr size_type npos = compat::string_view::npos;

  static constexpr const char *data() noexcept { return view().data(); }
  static constexpr const char *str() noexcept { return data(); }
  static constexpr size_type size() noexcept { return view().size(); }
  static constexpr bool empty() noexcept { return size() == 0; }

  /** @pre position is less than size(). */
  constexpr char operator[](size_type position) const noexcept {
    return assert(position < size()), data()[position];
  }

  constexpr operator compat::string_view() const noexcept { return view(); }

  static constexpr bool starts_with(compat::string_view prefix) noexcept {
#if defined(__cpp_lib_starts_ends_with) \
  && 201711L <= __cpp_lib_starts_ends_with
    return view().starts_with(prefix);
#else
    return prefix.size() <= size()
      && compare_views({data(), prefix.size()}, prefix) == 0;
#endif
  }

  static constexpr bool starts_with(char prefix) noexcept {
    return !empty() && data()[0] == prefix;
  }

  static constexpr bool ends_with(compat::string_view suffix) noexcept {
#if defined(__cpp_lib_starts_ends_with) \
  && 201711L <= __cpp_lib_starts_ends_with
    return view().ends_with(suffix);
#else
    return suffix.size() <= size()
      && compare_views(
        {data() + size() - suffix.size(), suffix.size()}, suffix) == 0;
#endif
  }

  static constexpr bool ends_with(char suffix) noexcept {
    return !empty() && data()[size() - 1] == suffix;
  }

  /** String-token search; e.g. find(OMNI_META_STRING("city")). */
  // The trailing return constrains What to tokens with a static view().
  template <typename What>
  static constexpr auto find(What, size_type position = 0)
    -> decltype(What::view(), size_type{}) {
#if defined(__cpp_lib_string_view) && 201606L <= __cpp_lib_string_view
    return view().find(What::view(), position);
#else
    return detail::static_string_find<cpp11_static_string, What>(position);
#endif
  }

#if defined(__cpp_lib_string_view) && 201606L <= __cpp_lib_string_view
  static constexpr size_type find(compat::string_view needle,
    size_type position = 0) noexcept { return view().find(needle, position); }
#endif

  static constexpr size_type find(char needle,
    size_type position = 0) noexcept {
#if defined(__cpp_lib_string_view) && 201606L <= __cpp_lib_string_view
    return view().find(needle, position);
#else
    return position >= size() ? npos : data()[position] == needle ? position
      : find(needle, position + 1);
#endif
  }

  // The trailing return constrains What to tokens with a static view().
  template <typename What>
  static constexpr auto contains(What needle)
    -> decltype(What::view(), bool{}) { return find(needle) != npos; }

#if defined(__cpp_lib_string_view) && 201606L <= __cpp_lib_string_view
  static constexpr bool contains(compat::string_view needle) noexcept {
    return find(needle) != npos;
  }
#endif

  static constexpr bool contains(char needle) noexcept {
    return find(needle) != npos;
  }

  static constexpr int compare(compat::string_view other) noexcept {
#if defined(__cpp_lib_string_view) && 201606L <= __cpp_lib_string_view
    return view().compare(other);
#else
    return compare_views(view(), other);
#endif
  }

  static constexpr bool equals(compat::string_view other) noexcept {
    return compare(other) == 0;
  }

  static constexpr compat::string_view view() noexcept {
    return {storage.values, sizeof(storage.values) - 1};
  }
};

/** (QoL) Concatenated compile-time strings with a flattened provider pack. */
template <typename... Left, typename... Right>
constexpr auto operator+(cpp11_static_string<Left...>,
  cpp11_static_string<Right...>) -> cpp11_static_string<Left..., Right...> {
  return {};
}

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
template <typename... Strings>
constexpr typename cpp11_static_string<Strings...>::storage_type
  cpp11_static_string<Strings...>::storage;

template <typename... Strings>
constexpr typename cpp11_static_string<Strings...>::size_type
  cpp11_static_string<Strings...>::npos;
#endif

#if OMNI_HAS_NTTP_STRING
namespace detail {

// Public array storage makes the literal a structural C++20 template argument.
template <std::size_t Size>
struct static_string_literal {
  char values[Size];

  consteval explicit static_string_literal(compat::string_view text) noexcept {
    for (std::size_t index = 0; index + 1 < Size; ++index)
      this->values[index] = text[index];
    this->values[Size - 1] = '\0';
  }

  consteval static_string_literal(const char (&text)[Size]) noexcept {
    for (std::size_t index = 0; index < Size; ++index)
      this->values[index] = text[index];
  }
};

// Preserve the literal's size, including embedded null characters.
template <static_string_literal Text>
struct nttp_string_source {
  static constexpr compat::string_view str() noexcept {
    return {Text.values, sizeof(Text.values) - 1};
  }
};

template <static_string_literal Text>
struct string_length_selection<nttp_string_source<Text>> {
  static constexpr auto get() noexcept { return sizeof(Text.values) - 1; }
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
using nttp_string = cpp11_static_string<detail::nttp_string_source<Text>>;

namespace detail {

// Sized NTTP text has its extent independently of any terminating character.
template <static_string_literal Text>
struct string_length_selection<nttp_string<Text>> {
  static constexpr auto get() noexcept { return sizeof(Text.values) - 1; }
};

} // namespace detail
#endif

} // namespace omni

/** (QoL) A compile-time string token from a character string literal.
 * text is the literal; the result is nttp_string when OMNI_HAS_NTTP_STRING
 * is 1, otherwise cpp11_static_string with an anonymous str() provider.
 *
```cpp
const auto prototype = OMNI_META_STRING("work.city");
constexpr decltype(prototype) path{};
static_assert(path.ends_with("city"), "");
```
 * Before C++17, invoking the lambda is not a constant expression. Its result
 * type still carries the text, avoiding separately named provider structs.
 * From C++17, the macro can initialize a constexpr object directly. */
#if OMNI_HAS_NTTP_STRING
#  define OMNI_META_STRING(text) ::omni::nttp_string<text>{}
#else
#  define OMNI_META_STRING(text) \
    ::omni::compat::invoke([] { \
      struct omni_string_source { \
        static constexpr const char *str() noexcept { return text; } }; \
      return ::omni::cpp11_static_string<omni_string_source>{}; \
    })
#endif

// REFACTORME: [high] Move generic compile-time strings and name-matching
// operations into Omnirefl core: field-name filtering is useful across
// extensions, independently of serialization.
