#pragma once

/**
 * This is a default implementation using rapidyaml library.
 */

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include <ryml.hpp>
#include <tl/expected.hpp>

// refactorme:
// this header is expected to be included in the generated .cpp file, hence everything here should
// have internal linkage
namespace {
namespace impl {

// todo: error should be more readable
template <typename T>
constexpr auto mem_vars = [] { static_assert(!sizeof(T), "mem_vars is not specialized"); };

template <typename, typename = void>
struct is_reflected: std::false_type {};

template <typename T>
struct is_reflected<T, std::void_t<std::tuple_size<decltype(mem_vars<T>)>>>: std::true_type {};

template <typename>
struct is_vector: std::false_type {};

template <typename... T>
struct is_vector<std::vector<T...>>: std::true_type {};

template <template <typename...> class, typename>
struct is: std::false_type {};

template <template <typename...> class A, typename... T>
struct is<A, A<T...>>: std::true_type {};

template <typename T, typename M>
struct mem_refl {
  using value_type = M;

  M T::*mem_ptr;
  std::string_view name;
};

template <typename T, typename M>
mem_refl(M T::*, std::string_view) -> mem_refl<T, M>;

template <typename T, typename... M>
constexpr std::tuple<mem_refl<T, M>...> make_reflect(mem_refl<T, M>... m) {
  // todo: static asssert that all the fields have been specified
  return {m...};
}

template <typename... T>
struct mem_var {
  std::string_view name;
  std::variant<T *...> ptr;
};

// todo: implement for type erasure
// todo: nested objects
template <typename... T>
struct object {
  // todo: fix-sized (static) view
  std::vector<mem_var<T...>> mem_vars;
};

template <typename... T>
object(std::vector<mem_var<T...>>) -> object<T...>;

bool is_boolean(const ryml::ConstNodeRef &n) {
  if (n.is_val_quoted())
    return false;
  const auto v = n.val();
  return "true" == v || "false" == v //
    || "True" == v || "False" == v //
    || "TRUE" == v || "FALSE" == v;
}

[[maybe_unused]] bool is_number(const ryml::ConstNodeRef &n) {
  return n.val().is_number();
}

bool is_integer(const ryml::ConstNodeRef &n) {
  return n.val().is_integer();
}

bool is_unsigned_integer(const ryml::ConstNodeRef &n) {
  return n.val().is_unsigned_integer();
}

bool is_real(const ryml::ConstNodeRef &n) {
  return n.val().is_real();
}

// todo: implement serialize

// todo: protect from invalid node
// todo: additional argument to allow in-place reflection (re)definition + reflection definition for
// local structs
// todo: some fields might have a user-defined custom deserialization function, which need to be
// called. which is currently resolved by `omni::deserialize_t`.
// todo: aliased fields. Could be done by a adapter for ConstNodeRef (when enumerating json
// children, names can be substituted by adapter)
template <typename To>
tl::expected<void, std::string> deserialize(const ryml::ConstNodeRef &from, To &to) noexcept {
  static_assert(!std::is_pointer_v<To>);
  constexpr auto to_string_view = [](c4::csubstr s) -> std::string_view {
    return {s.data(), s.size()};
  };
  using _to = std::decay_t<To>;
  using result = tl::expected<void, std::string>;

  if constexpr (std::is_fundamental_v<_to>) {
    // just to reduce boilerplate
    const auto _deserialize = //
      [to_string_view](const ryml::ConstNodeRef &from,
        To &to,
        bool valid,
        std::string_view err) -> result {
      const auto val = from.val();
      const auto _val = to_string_view(val);
      if (!valid)
        return tl::unexpected(std::string(_val) + err.data());

      using c4::from_chars;
      from_chars(val, &to);
      return {};
    };

    if constexpr (std::is_same_v<_to, bool>) {
      return _deserialize(from, to, is_boolean(from), " is not a boolean");
    } else if constexpr (std::is_integral_v<_to> && std::is_unsigned_v<_to>) {
      return _deserialize(from, to, is_unsigned_integer(from), " is not an unsigned integer");
    } else if constexpr (std::is_integral_v<_to>) {
      return _deserialize(from, to, is_integer(from), " is not an integer");
    } else if constexpr (std::is_floating_point_v<_to>) {
      return _deserialize(from, to, is_real(from), " is not a real number");
    }
    // todo: null, nan, etc?
    // todo: is_string, and use an allocator
  } else if constexpr (is<std::basic_string, _to>()) {
    // todo: is_string()
    if (!from.is_val_quoted()) {
      return tl::unexpected(std::string(to_string_view(from.val())) + " is not a string");
    }
    to = std::string(to_string_view(from.val()));
    return {};
  } else if constexpr (is<std::vector, _to>::value) {
    // todo: is_array
    if (!from.is_seq()) {
      return tl::unexpected(std::string(to_string_view(from.key())) + " is not an array");
    }
    to.reserve(from.num_children());
    for (const ryml::ConstNodeRef &c : from.children()) {
      auto res = deserialize(c, to.emplace_back());
      if (!res)
        return tl::unexpected(std::move(res).error());
    }
    return {};
  } else if constexpr (is<object, _to>()) {
    // todo: is_map
    if (!from.is_map()) {
      return tl::unexpected(std::string(to_string_view(from.key())) + " is not a dictionary");
    }

    // todo: bitsets + custom allocator + profile
    std::vector<uint8_t> visited(to.mem_vars.size(), false);
    size_t n_visited = 0;
    auto mem_vars = to.mem_vars;
    std::sort(mem_vars.begin(), mem_vars.end(), [](const auto &lhs, const auto &rhs) {
      return lhs.name < rhs.name;
    });

    // todo: profile the difference between iterating json nodes vs iterating struct fields
    // todo: support for aliased fields, preferrably without modifying the original json
    for (const ryml::ConstNodeRef &c : from.children()) {
      if (visited.size() == n_visited) {
        return tl::unexpected("unknown field '" + std::string(to_string_view(c.key())) + "'");
      }

      const auto name = to_string_view(c.key());
      const auto mv = std::lower_bound(mem_vars.cbegin(),
        mem_vars.cend(),
        name,
        [](const auto &m, std::string_view name) { return m.name < name; });

      if (mv == mem_vars.cend() || mv->name != name) {
        return tl::unexpected("unknown field '" + std::string(name) + "'");
      }

      if (auto &v = visited[std::distance(mem_vars.cbegin(), mv)]; v) {
        return tl::unexpected("duplicate field '" + std::string(name) + "'");
      } else {
        v = true;
        ++n_visited;
      }

      auto res = std::visit(
        [&c](auto *mem_var) -> tl::expected<void, std::string> {
          auto res = deserialize(c, *mem_var);
          if (!res)
            return tl::unexpected(std::move(res).error());
          return {};
        },
        mv->ptr);

      if (!res)
        return tl::unexpected(std::move(res).error());
    }

    if (visited.size() > n_visited) {
      std::string unvisited;
      for (size_t i = 0; i < visited.size(); ++i) {
        if (!visited[i])
          unvisited += (!unvisited.empty() ? ", " : "") + std::string(mem_vars[i].name);
      }

      return tl::unexpected("missing fields: " + unvisited);
    }

    return {};
  } else if constexpr (is<std::variant, _to>()) {
    // todo: support for std::variant
    // we can't distinguish between custom user-defined type without providing additional context
    // via arguments or type traits, which would complicate the logic of this function.
    // hense, only the common types should be supported here, and composition should be used outside
    // to further figure-out custom user-defined classes
    return tl::unexpected("variant support is not implemented");
  } else if constexpr (is<std::optional, _to>()) {
    // todo: implement
    // in order to distinguish between the unspecified field or explicit `null` a union of 3 types
    // should be used: undefined, null, value
    return tl::unexpected("optioinal support is not implemented");
  } else if constexpr (is<std::map, _to>()) {
    // todo: not only std::map, but preferrably flat_map
    // todo: implement
    return tl::unexpected("map support is not implemented");
  } else if constexpr (is_reflected<_to>()) {
    // todo: implement nested type erasure
    auto _obj = std::apply(
      [&to](auto... mvars) {
        using _mem_var = mem_var<typename std::decay_t<decltype(mvars)>::value_type...>;
        return object{std::vector{_mem_var{mvars.name, {std::addressof(to.*mvars.mem_ptr)}}...}};
      },
      mem_vars<_to>);
    auto res = deserialize(from, _obj);
    if (!res)
      return tl::unexpected(std::move(res).error());

    return {};
  } else {
    static_assert(!sizeof(_to), "unexpected type");
  }
}

/*
 * todo: type erasure + bitsets
 * compoud types: object, vector, map // 3 bits
 * sum: variant // 1 bit
 * scalar: string, bool, numeric(float, double, (u)int8, (u)int16, (u)int32, (u)int64) // 12 bits
 *
 * bits:
 * variant object vector map string bool float double (u)int8, (u)int16, (u)int32, (u)int64
 *
 * possible: variant + one or many other bits
 * possible: map | object | vector
 * possible: one of scalar types
 *
 * const auto flags = to.type();
 */

template <typename T, typename = std::enable_if_t<is_reflected<T>::value>>
tl::expected<T, std::string> deserialize(const ryml::ConstNodeRef &n) {
  T out;

  if (auto res = deserialize(n, out); !res)
    return tl::unexpected(std::move(res).error());
  // todo: implement
  return {std::move(out)};
}
} // namespace impl
} // namespace
