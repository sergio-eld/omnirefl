// This is an exposition of a generated reflected .cpp Unit.

#include <omnirefl/reflected_call.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <array>
#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// User types (normally brought in via preprocessor includes)
// -----------------------------------------------------------------------------
// #include "/path/to/ring_style.hpp"
// #include "/path/to/championship.hpp"
// #include "/path/to/person_john_cena.hpp"

enum class ring_style {
  rs_technical,
  rs_high_flying,
  rs_power,
};

struct championship {
  std::string name;
  std::string title;
};

struct person_john_cena {
  std::string name;
  unsigned age;
  ring_style style;
  championship main_title;
};

// -----------------------------------------------------------------------------
// Visitors (normally brought in via preprocessor includes)
// -----------------------------------------------------------------------------
// #include "/path/to/print_type_info_visitor.hpp"
// #include "/path/to/print_enum_type_info_visitor.hpp"

struct print_type_info_t {
  struct result {
    std::string name;
    std::vector<std::string> namespaces;
  };

  template <typename T>
  result operator()(const T &) const {
    using meta = omni::reflected_t<T>; // frontend
    // Namespaces not modeled yet.
    return {meta::name(), {}};
  }
} constexpr print_type_info{};

struct print_enum_type_info_t {
  struct result {
    print_type_info_t::result type_info;
    std::vector<std::string> names;
  };

  template <typename T,
    typename = typename std::enable_if<std::is_enum<T>::value>::type>
  result operator()(const T &) const {
    using meta = omni::reflected_t<T>;

    std::vector<std::string> names;
    for (const auto &p : meta::enumerators()) {
      names.emplace_back(p.second);
    }

    return {
      /*type_info=*/print_type_info(T{}),
      /*names=*/std::move(names),
    };
  }
} constexpr print_enum_type_info{};

// -----------------------------------------------------------------------------
// Generated _reflected specializations (metadata only)
// -----------------------------------------------------------------------------

namespace omni {
namespace detail {
namespace { // anonymous, TU-local

// ------------------------------------
// enum ring_style
// ------------------------------------
template <typename T>
struct _reflected<enum ring_style, T> {
  static_assert(std::is_same<enum ring_style, T>::value,
    "omnirefl: unexpected types mismatch, try regenerating");

  using type = T;

  constexpr static reflected_entity entity() noexcept {
    return reflected_entity::enumeration;
  }

  constexpr static auto name() noexcept -> const
    char (&)[sizeof("ring_style")] {
    return "ring_style";
  }

  constexpr static auto enumerators() noexcept
    -> std::array<std::pair<type, const char *>, 3> {
    return {{
      {type::rs_technical, "rs_technical"},
      {type::rs_high_flying, "rs_high_flying"},
      {type::rs_power, "rs_power"},
    }};
  }
};

// ------------------------------------
// struct championship
// ------------------------------------
template <typename T>
struct _reflected<struct championship, T> {
  static_assert(std::is_same<struct championship, T>::value,
    "omnirefl: unexpected types mismatch, try regenerating");

  using type = T;

  constexpr static reflected_entity entity() noexcept {
    return reflected_entity::record;
  }

  constexpr static auto name() noexcept -> const
    char (&)[sizeof("championship")] {
    return "championship";
  }

  struct name_t {
    constexpr static std::size_t index() noexcept {
      return 0;
    }

    constexpr static auto name() noexcept -> const char (&)[sizeof("name")] {
      return "name";
    }

    constexpr static auto value(const type &t) noexcept -> const
      decltype(t.name) & {
      return t.name;
    }

    template <typename V>
    static auto set_value(type &t, V &&v) noexcept(
      noexcept(t.name = std::forward<V>(v))) -> const decltype(t.name) & {
      t.name = std::forward<V>(v);
      return t.name;
    }
  };

  struct title_t {
    constexpr static std::size_t index() noexcept {
      return 1;
    }

    constexpr static auto name() noexcept -> const char (&)[sizeof("title")] {
      return "title";
    }

    constexpr static auto value(const type &t) noexcept -> const
      decltype(t.title) & {
      return t.title;
    }

    template <typename V>
    static auto set_value(type &t, V &&v) noexcept(
      noexcept(t.title = std::forward<V>(v))) -> const decltype(t.title) & {
      t.title = std::forward<V>(v);
      return t.title;
    }
  };

  using public_fields_t = std::tuple<name_t, title_t>;

  // static meta::public_fields() used by reflected_record
  constexpr static public_fields_t public_fields() noexcept {
    return public_fields_t{};
  }
};

// ------------------------------------
// struct person_john_cena
// ------------------------------------
template <typename T>
struct _reflected<struct person_john_cena, T> {
  static_assert(std::is_same<struct person_john_cena, T>::value,
    "omnirefl: unexpected types mismatch, try regenerating");

  using type = T;

  constexpr static reflected_entity entity() noexcept {
    return reflected_entity::record;
  }

  constexpr static auto name() noexcept -> const
    char (&)[sizeof("person_john_cena")] {
    return "person_john_cena";
  }

  struct name_t {
    constexpr static std::size_t index() noexcept {
      return 0;
    }

    constexpr static auto name() noexcept -> const char (&)[sizeof("name")] {
      return "name";
    }

    constexpr static auto value(const type &t) noexcept -> const
      decltype(t.name) & {
      return t.name;
    }

    template <typename V>
    static auto set_value(type &t, V &&v) noexcept(
      noexcept(t.name = std::forward<V>(v))) -> const decltype(t.name) & {
      t.name = std::forward<V>(v);
      return t.name;
    }
  };

  struct age_t {
    constexpr static std::size_t index() noexcept {
      return 1;
    }

    constexpr static auto name() noexcept -> const char (&)[sizeof("age")] {
      return "age";
    }

    constexpr static auto value(const type &t) noexcept -> const
      decltype(t.age) & {
      return t.age;
    }

    template <typename V>
    static auto set_value(type &t, V &&v) noexcept(
      noexcept(t.age = std::forward<V>(v))) -> const decltype(t.age) & {
      t.age = std::forward<V>(v);
      return t.age;
    }
  };

  struct style_t {
    constexpr static std::size_t index() noexcept {
      return 2;
    }

    constexpr static auto name() noexcept -> const char (&)[sizeof("style")] {
      return "style";
    }

    constexpr static auto value(const type &t) noexcept -> const
      decltype(t.style) & {
      return t.style;
    }

    template <typename V>
    static auto set_value(type &t, V &&v) noexcept(
      noexcept(t.style = std::forward<V>(v))) -> const decltype(t.style) & {
      t.style = std::forward<V>(v);
      return t.style;
    }
  };

  struct main_title_t {
    constexpr static std::size_t index() noexcept {
      return 3;
    }

    constexpr static auto name() noexcept -> const
      char (&)[sizeof("main_title")] {
      return "main_title";
    }

    constexpr static auto value(const type &t) noexcept -> const
      decltype(t.main_title) & {
      return t.main_title;
    }

    template <typename V>
    static auto set_value(type &t, V &&v) noexcept(noexcept(
      t.main_title = std::forward<V>(v))) -> const decltype(t.main_title) & {
      t.main_title = std::forward<V>(v);
      return t.main_title;
    }
  };

  using public_fields_t = std::tuple<name_t, age_t, style_t, main_title_t>;

  // static meta::public_fields() used by reflected_record
  constexpr static public_fields_t public_fields() noexcept {
    return public_fields_t{};
  }
};

} // namespace
} // namespace detail
} // namespace omni

// -----------------------------------------------------------------------------
// is_reflected specialization for this TU
// -----------------------------------------------------------------------------

template <typename T, typename>
struct omni::is_reflected: std::false_type {};

template <typename T>
struct omni::is_reflected<T,
  omni::compat::void_t<
    typename omni::detail::_reflected<typename std::decay<T>::type>::type>>:
    std::true_type {};

// -----------------------------------------------------------------------------
// main: use visitors
// -----------------------------------------------------------------------------

int main() {
  person_john_cena john{
    "John Cena",
    46u,
    ring_style::rs_power,
    championship{"WWE", "World Heavyweight Championship"},
  };

  auto person_info = print_type_info(john);
  auto ring_info = print_enum_type_info(john.style);

  std::cout << "person_john_cena type name: " << person_info.name << '\n';

  std::cout << "ring_style type name: " << ring_info.type_info.name << '\n';
  std::cout << "ring_style enumerators:\n";
  for (const auto &n : ring_info.names) {
    std::cout << "  - " << n << '\n';
  }

  return 0;
}
