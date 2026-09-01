#include <omnirefl/reflection.hpp>

#include <gtest/gtest.h>
#include <ryml.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

struct foo {
  std::string a;
  int b;

  bool operator==(const foo &) const = default;
};

struct bar {
  double d;
  foo f;
};

struct scalar_values {
  bool enabled;
  unsigned retries;
  int delta;
  std::vector<int> values;
  std::string label;
  std::vector<foo> records;
};

namespace {

namespace impl {

template <typename>
struct is_vector: std::false_type {};

template <typename... T>
struct is_vector<std::vector<T...>>: std::true_type {};

template <template <typename...> class, typename>
struct is: std::false_type {};

template <template <typename...> class A, typename... T>
struct is<A, A<T...>>: std::true_type {};

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

bool is_boolean(const ryml::ConstNodeRef &n, c4::csubstr value) {
  if (n.is_val_quoted())
    return false;
  return "true" == value || "false" == value //
    || "True" == value || "False" == value //
    || "TRUE" == value || "FALSE" == value;
}

template <typename Value, typename Predicate, typename Error>
auto ensure(Value &&value, Predicate predicate, Error error) {
  using result = std::expected<std::decay_t<Value>,
    std::decay_t<std::invoke_result_t<Error &, Value &>>>;

  if (std::invoke(predicate, value))
    return result{std::in_place, std::forward<Value>(value)};

  return result{std::unexpected(std::invoke(error, value))};
}

template <typename To>
std::expected<void, std::string> deserialize(const ryml::ConstNodeRef &from,
  To &to) noexcept;

// Current Omnirefl bindings replace the generated `mem_vars<T>` tuple used by
// the original implementation. The deserialization algorithm below is kept
// unchanged and still consumes the same type-erased `object` representation.
template <typename Binding>
std::expected<void, std::string>
  deserialize_fields(const ryml::ConstNodeRef &from, Binding to) noexcept {
  auto _obj = std::apply(
    [](auto... field) {
      using _mem_var = mem_var<typename std::decay_t<decltype(field)>::type...>;
      return object{
        std::vector{_mem_var{field.name(), {std::addressof(field.ref())}}...}};
    },
    to.public_fields());
  auto res = deserialize(from, _obj);
  if (!res)
    return std::unexpected(std::move(res).error());

  return {};
}

// todo: protect from invalid node
// todo: additional argument to allow in-place reflection (re)definition +
// reflection definition for local structs
template <typename To>
std::expected<void, std::string> deserialize(const ryml::ConstNodeRef &from,
  To &to) noexcept {
  static_assert(!std::is_pointer_v<To>);
  static constexpr auto to_string_view = [](c4::csubstr s) -> std::string_view {
    return {s.data(), s.size()};
  };
  using _to = std::decay_t<To>;
  using result = std::expected<void, std::string>;

  if constexpr (std::is_fundamental_v<_to>) {
    const auto parse_if = //
      [&from](auto predicate, std::string_view error, _to *to) -> result {
      return ensure(from.val(),
        std::move(predicate),
        [error](c4::csubstr value) {
          return std::string{to_string_view(value)}.append(error);
        })
        .transform([to](auto value) { c4::from_chars(value, to); });
    };

    if constexpr (std::is_same_v<_to, bool>) {
      return parse_if(std::bind_front(is_boolean, std::cref(from)),
        " is not a boolean",
        &to);
    } else if constexpr (std::is_integral_v<_to> && std::is_unsigned_v<_to>) {
      return parse_if(&c4::csubstr::is_unsigned_integer,
        " is not an unsigned integer",
        &to);
    } else if constexpr (std::is_integral_v<_to>) {
      return parse_if(&c4::csubstr::is_integer, " is not an integer", &to);
    } else if constexpr (std::is_floating_point_v<_to>) {
      return parse_if(&c4::csubstr::is_real, " is not a real number", &to);
    }
    // todo: null, nan, etc?
    // todo: is_string, and use an allocator
  } else if constexpr (is<std::basic_string, _to>()) {
    // todo: is_string()
    if (!from.is_val_quoted()) {
      return std::unexpected(
        std::string(to_string_view(from.val())) + " is not a string");
    }
    to = std::string(to_string_view(from.val()));
    return {};
  } else if constexpr (is<std::vector, _to>::value) {
    // todo: is_array
    if (!from.is_seq()) {
      return std::unexpected(
        std::string(to_string_view(from.key())) + " is not an array");
    }
    to.reserve(from.num_children());
    for (const ryml::ConstNodeRef &c : from.children()) {
      auto res = deserialize(c, to.emplace_back());
      if (!res)
        return std::unexpected(std::move(res).error());
    }
    return {};
  } else if constexpr (is<object, _to>()) {
    // todo: is_map
    if (!from.is_map()) {
      return std::unexpected(
        std::string(to_string_view(from.key())) + " is not a dictionary");
    }

    // todo: bitsets + custom allocator + profile
    std::vector<uint8_t> visited(to.mem_vars.size(), false);
    size_t n_visited = 0;
    auto mem_vars = to.mem_vars;
    std::sort(mem_vars.begin(),
      mem_vars.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.name < rhs.name; });

    // todo: profile the difference between iterating json nodes vs iterating
    // struct fields
    for (const ryml::ConstNodeRef &c : from.children()) {
      if (visited.size() == n_visited) {
        return std::unexpected(
          "unknown field '" + std::string(to_string_view(c.key())) + "'");
      }

      const auto name = to_string_view(c.key());
      const auto mv = std::lower_bound(mem_vars.begin(),
        mem_vars.end(),
        name,
        [](const auto &m, std::string_view name) { return m.name < name; });

      if (mv == mem_vars.end())
        return std::unexpected("unknown field '" + std::string(name) + "'");

      if (auto &v = visited[std::distance(mem_vars.begin(), mv)]; v) {
        return std::unexpected("duplicate field '" + std::string(name) + "'");
      } else {
        v = true;
        ++n_visited;
      }

      auto res = std::visit(
        [&c](auto *mem_var) -> std::expected<void, std::string> {
          auto res = deserialize(c, *mem_var);
          if (!res)
            return std::unexpected(std::move(res).error());
          return {};
        },
        mv->ptr);

      if (!res)
        return std::unexpected(std::move(res).error());
    }

    if (visited.size() > n_visited) {
      std::string unvisited;
      for (size_t i = 0; i < visited.size(); ++i) {
        if (!visited[i])
          unvisited +=
            (!unvisited.empty() ? ", " : "") + std::string(mem_vars[i].name);
      }

      return std::unexpected("missing fields: " + unvisited);
    }

    return {};
  } else if constexpr (is<std::variant, _to>()) {
    // todo: support for std::variant
    // we can't distinguish between custom user-defined type without providing
    // additional context via arguments or type traits, which would complicate
    // the logic of this function. hense, only the common types should be
    // supported here, and composition should be used outside to further
    // figure-out custom user-defined classes
  } else if constexpr (false) {
    // todo: support for std::optional
    // in order to distinguish between the unspecified field or explicit
    // `null` a union of 3 types should be used: undefined, null, value
  } else if constexpr (is<std::map, _to>()) {
    // todo: not only std::map, but preferrably flat_map
    // todo: support for std::map
  } else if constexpr (omni::is_reflected<_to>::value) {
    return omni::reflected_call(
      [&from](omni::binding auto binding) -> result {
        return deserialize_fields(from, binding);
      },
      to);
  } else {
    static_assert(!sizeof(_to), "unexpected type");
  }
}

} // namespace impl
} // namespace

namespace omni {

struct deserialize_t {
  template <typename T>
  std::expected<void, std::string> operator()(const ryml::ConstNodeRef &data,
    T &to) const {
    return reflected_call(
      [&data](binding auto value) -> std::expected<void, std::string> {
        return impl::deserialize_fields(data, value);
      },
      to);
  }

  template <typename T>
  std::expected<T, std::string> to(const ryml::ConstNodeRef &data) const {
    std::expected<T, std::string> to{std::in_place};
    auto res = (*this)(data, *to);
    if (res)
      return to;
    return std::unexpected(std::move(res).error());
  }
};

constexpr inline deserialize_t deserialize{};

} // namespace omni

TEST(serialization_json, deserializes_nested_record) {
  const auto result = omni::deserialize.to<bar>(
    ryml::parse_in_arena(R"({"d": 23.42, "f": {"a": "oceanic", "b": 815}})"));

  ASSERT_TRUE(result) << result.error();
  EXPECT_DOUBLE_EQ(23.42, result->d);
  EXPECT_EQ("oceanic", result->f.a);
  EXPECT_EQ(815, result->f.b);
}

TEST(serialization_json, deserializes_scalars_and_sequence) {
  const auto result =
    omni::deserialize.to<scalar_values>(ryml::parse_in_arena(R"({
      "enabled": true,
      "retries": 108,
      "delta": -42,
      "values": [8, 15],
      "label": "oceanic",
      "records": [
        {"a": "oceanic", "b": 815},
        {"a": "sunset", "b": 108}
      ]
    })"));

  ASSERT_TRUE(result) << result.error();
  EXPECT_TRUE(result->enabled);
  EXPECT_EQ(108U, result->retries);
  EXPECT_EQ(-42, result->delta);
  EXPECT_EQ((std::vector<int>{8, 15}), result->values);
  EXPECT_EQ("oceanic", result->label);
  EXPECT_EQ(
    (std::vector<foo>{{.a = "oceanic", .b = 815}, {.a = "sunset", .b = 108}}),
    result->records);
}

TEST(serialization_json, rejects_invalid_nested_integer) {
  const auto result = omni::deserialize.to<bar>(ryml::parse_in_arena(
    R"({"d": 23.42, "f": {"a": "oceanic", "b": "invalid"}})"));

  ASSERT_FALSE(result);
  EXPECT_EQ("invalid is not an integer", result.error());
}

TEST(serialization_json, rejects_unknown_field) {
  const auto result = omni::deserialize.to<bar>(ryml::parse_in_arena(
    R"({"d": 23.42, "f": {"a": "oceanic", "b": 815}, "extra": 1})"));

  ASSERT_FALSE(result);
  EXPECT_EQ("unknown field 'extra'", result.error());
}

TEST(serialization_json, reports_missing_field) {
  const auto result =
    omni::deserialize.to<bar>(ryml::parse_in_arena(R"({"d": 23.42})"));

  ASSERT_FALSE(result);
  EXPECT_EQ("missing fields: f", result.error());
}
