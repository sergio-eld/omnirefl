#pragma once

#include <omnirefl/reflection.hpp>

#include <ryml.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__has_include)
#  if __has_include(<expected>)
#    include <expected>
#  endif
#  if __has_include(<variant>)
#    include <variant>
#  endif
#endif

#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
#  include <tl/expected.hpp>
#endif

#if !defined(__cpp_lib_variant) || __cpp_lib_variant < 201606L
#  include <mpark/variant.hpp>
#endif

namespace serialization {

#if defined(__cpp_lib_expected) && 202202L <= __cpp_lib_expected
template <typename T>
using result = std::expected<T, std::string>;

template <typename T>
result<T> failure(std::string error) {
  return std::unexpected<std::string>{std::move(error)};
}
#else
template <typename T>
using result = tl::expected<T, std::string>;

template <typename T>
result<T> failure(std::string error) {
  return tl::make_unexpected(std::move(error));
}
#endif

namespace detail {

#if defined(__cpp_lib_variant) && 201606L <= __cpp_lib_variant
template <typename... T>
using variant = std::variant<T...>;

template <typename Visitor, typename Value>
auto visit(Visitor &&visitor,
  Value &&value) -> decltype(std::visit(std::forward<Visitor>(visitor),
                   std::forward<Value>(value))) {
  return std::visit(std::forward<Visitor>(visitor), std::forward<Value>(value));
}
#else
template <typename... T>
using variant = mpark::variant<T...>;

template <typename Visitor, typename Value>
auto visit(Visitor &&visitor,
  Value &&value) -> decltype(mpark::visit(std::forward<Visitor>(visitor),
                   std::forward<Value>(value))) {
  return mpark::visit(std::forward<Visitor>(visitor),
    std::forward<Value>(value));
}
#endif

template <template <typename...> class, typename>
struct is_specialization: std::false_type {};

template <template <typename...> class Template, typename... T>
struct is_specialization<Template, Template<T...>>: std::true_type {};

template <typename>
struct dependent_false: std::false_type {};

template <std::size_t Index, typename T>
struct field_pointer {
  T *value;
};

template <typename... Pointer>
struct member_variable {
  c4::csubstr name;
  variant<Pointer...> pointer;
};

template <typename... Pointer>
struct object {
  std::vector<member_variable<Pointer...>> members;
};

template <std::size_t Index, typename Tuple>
using field_value_type = typename omni::compat::remove_cvref_t< //
  decltype(std::get<Index>(std::declval<Tuple &>()))>::type;

inline std::string to_string(c4::csubstr value) {
  return {value.data(), value.size()};
}

inline bool is_boolean(const ryml::ConstNodeRef &node, c4::csubstr value) {
  if (node.is_val_quoted())
    return false;

  return "true" == value || "false" == value //
    || "True" == value || "False" == value //
    || "TRUE" == value || "FALSE" == value;
}

template <typename To, typename Predicate>
result<void> parse_if(const ryml::ConstNodeRef &from,
  To *to,
  Predicate predicate,
  c4::csubstr error) {
  if (!from.has_val())
    return failure<void>(
      to_string(from.key()).append(error.data(), error.size()));

  const c4::csubstr value = from.val();
  if (!omni::compat::invoke(predicate, value))
    return failure<void>(to_string(value).append(error.data(), error.size()));

  c4::from_chars(value, to);
  return {};
}

template <typename Algorithm, typename To>
result<void> deserialize_reflected(const ryml::ConstNodeRef &from, To *to);

template <typename Algorithm, typename Binding>
result<void> deserialize_fields(const ryml::ConstNodeRef &from, Binding to);

template <typename Algorithm>
struct deserialize_pointer {
  ryml::ConstNodeRef from;

  template <std::size_t Index, typename T>
  result<void> operator()(field_pointer<Index, T> field) const {
    return Algorithm::deserialize(from, field.value);
  }
};

template <typename Tuple, std::size_t... Index>
auto make_object(Tuple &fields, omni::compat::index_sequence<Index...>)
  -> object<field_pointer<Index, field_value_type<Index, Tuple>>...> {
  using result_type =
    object<field_pointer<Index, field_value_type<Index, Tuple>>...>;
  using member_type =
    member_variable<field_pointer<Index, field_value_type<Index, Tuple>>...>;

  return result_type{
    {member_type{c4::to_csubstr(std::get<Index>(fields).name()),
      field_pointer<Index, field_value_type<Index, Tuple>>{
        std::addressof(std::get<Index>(fields).ref())}}...}};
}

template <typename Algorithm, typename Binding>
result<void> deserialize_fields(const ryml::ConstNodeRef &from, Binding to) {
  auto fields = to.public_fields();
  auto object = make_object(fields,
    omni::compat::make_index_sequence<
      std::tuple_size<decltype(fields)>::value>{});
  return Algorithm::deserialize(from, std::addressof(object));
}

template <typename Algorithm>
struct reflected_record {
  ryml::ConstNodeRef from;

  template <typename T>
  result<void> operator()(omni::binding_t<T> to) const {
    return deserialize_fields<Algorithm>(from, to);
  }
};

template <typename Algorithm, typename To>
result<void> deserialize_reflected(const ryml::ConstNodeRef &from, To *to) {
  return omni::reflected_call(reflected_record<Algorithm>{from}, *to);
}

template <typename To>
result<void> deserialize_string(const ryml::ConstNodeRef &from, To *to) {
  if (!from.has_val())
    return failure<void>(to_string(from.key()) + " is not a string");

  to->assign(from.val().data(), from.val().size());
  return {};
}

template <typename Algorithm, typename To>
result<void> deserialize_sequence(const ryml::ConstNodeRef &from, To *to) {
  if (!from.is_seq())
    return failure<void>(to_string(from.key()) + " is not an array");

  to->reserve(from.num_children());
  for (const ryml::ConstNodeRef &child : from.children()) {
    to->emplace_back();
    const result<void> deserialized =
      Algorithm::deserialize(child, std::addressof(to->back()));
    if (!deserialized)
      return failure<void>(deserialized.error());
  }

  return {};
}

template <typename Algorithm, typename... Pointer>
result<void> deserialize_object(const ryml::ConstNodeRef &from,
  object<Pointer...> *to) {
  if (!from.is_map())
    return failure<void>(to_string(from.key()) + " is not a dictionary");

  std::vector<unsigned char> visited(to->members.size(), false);
  std::size_t visited_count = 0;
  std::vector<member_variable<Pointer...>> members = to->members;
  std::sort(members.begin(),
    members.end(),
    [](const member_variable<Pointer...> &left,
      const member_variable<Pointer...> &right) {
      return left.name < right.name;
    });

  for (const ryml::ConstNodeRef &child : from.children()) {
    if (visited.size() == visited_count)
      return failure<void>("unknown field '" + to_string(child.key()) + "'");

    const c4::csubstr name = child.key();
    typename std::vector<member_variable<Pointer...>>::iterator member =
      std::lower_bound(members.begin(),
        members.end(),
        name,
        [](const member_variable<Pointer...> &candidate, c4::csubstr name_) {
          return candidate.name < name_;
        });

    if (member == members.end() || member->name != name)
      return failure<void>("unknown field '" + to_string(name) + "'");

    const std::size_t index =
      static_cast<std::size_t>(std::distance(members.begin(), member));
    if (visited[index])
      return failure<void>("duplicate field '" + to_string(name) + "'");

    visited[index] = true;
    ++visited_count;
    const result<void> deserialized =
      visit(deserialize_pointer<Algorithm>{child}, member->pointer);
    if (!deserialized)
      return failure<void>(deserialized.error());
  }

  if (visited.size() == visited_count)
    return {};

  std::string missing;
  for (std::size_t index = 0; index < visited.size(); ++index) {
    if (!visited[index]) {
      if (!missing.empty())
        missing += ", ";
      missing += to_string(members[index].name);
    }
  }

  return failure<void>("missing fields: " + missing);
}

template <typename Algorithm>
struct deserialize_t {
  template <typename T>
  result<void> operator()(const ryml::ConstNodeRef &from, T *to) const {
    return omni::reflected_call(reflected_record<Algorithm>{from}, *to);
  }

  template <typename T>
  result<T> to(const ryml::ConstNodeRef &from) const {
    T value{};
    const result<void> deserialized = (*this)(from, std::addressof(value));
    if (!deserialized)
      return failure<T>(deserialized.error());

    return result<T>{std::move(value)};
  }
};

} // namespace detail
} // namespace serialization
