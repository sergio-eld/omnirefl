#pragma once

/** @file
 * Named heterogeneous tables and expected-like lookup results.
 * Independent of reflected_scope.hpp and generated metadata.
 */

#include <omnirefl/compat.hpp>
#include <omnirefl/traits.hpp>

#if 202002L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<bit>)
#    include <bit>
#  endif
#endif
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#if 201703L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<string_view>)
#    include <string_view>
#  endif
#endif
#if !defined(__cpp_lib_string_view) || 201606L > __cpp_lib_string_view
#  include <string>
#endif
#include <tuple>
#include <type_traits>
#include <utility>

namespace omni {
namespace fn {

/** The name for which a lookup found no entry. */
template <typename Name>
struct not_found_t {
  Name name;
};

template <typename Value, typename Error>
class lookup_result_t;

template <typename Tuple>
struct lookup_selection_t;

// libc++ tuple construction is not constexpr in C++11. libstdc++ 7 tuple
// move traits also exceed the default template depth with 64 entries. Reuse
// indexed compatibility storage only where standard tuple storage hits these
// constraints; newer libraries retain std::tuple.
#if OMNI_CPLUSPLUS < 201402L \
  || (defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE <= 7)
#  define OMNI_FN_LOOKUP_INDEXED_ENTRIES 1
#else
#  define OMNI_FN_LOOKUP_INDEXED_ENTRIES 0
#endif

#if OMNI_FN_LOOKUP_INDEXED_ENTRIES
namespace detail {

template <typename... Meta>
struct lookup_entries: compat::detail::bound_values<
    compat::make_index_sequence<sizeof...(Meta)>, Meta...> {
  using storage = compat::detail::bound_values<
    compat::make_index_sequence<sizeof...(Meta)>, Meta...>;

  constexpr lookup_entries(const Meta &...meta): storage(meta...) {}
};

template <std::size_t Index, typename... Meta>
constexpr auto get(const lookup_entries<Meta...> &entries)
  -> decltype(compat::detail::bound_get<Index>(entries)) {
  return compat::detail::bound_get<Index>(entries);
}

template <std::size_t Index, typename... Meta>
constexpr auto get(lookup_entries<Meta...> &entries)
  -> decltype(compat::detail::bound_get<Index>(entries)) {
  return compat::detail::bound_get<Index>(entries);
}

template <std::size_t Index, typename... Meta>
constexpr auto get(lookup_entries<Meta...> &&entries)
  -> decltype(compat::detail::bound_get<Index>(std::move(entries))) {
  return compat::detail::bound_get<Index>(std::move(entries));
}

template <std::size_t Index, typename... Meta>
constexpr auto get(const lookup_entries<Meta...> &&entries)
  -> decltype(compat::detail::bound_get<Index>(std::move(entries))) {
  return compat::detail::bound_get<Index>(std::move(entries));
}

} // namespace detail
} // namespace fn
} // namespace omni

namespace std {

template <typename... Meta>
struct tuple_size<omni::fn::detail::lookup_entries<Meta...>>:
    integral_constant<size_t, sizeof...(Meta)> {};

template <size_t Index, typename... Meta>
struct tuple_element<Index, omni::fn::detail::lookup_entries<Meta...>>:
    tuple_element<Index, tuple<Meta...>> {};
} // namespace std

namespace omni {
namespace fn {
#endif

namespace detail {

struct lookup_value_tag {};
struct lookup_error_tag {};
struct lookup_unit {};
struct empty_lookup_value {};

// C++11 has no variant or expected. Construct only the active alternative so
// descriptors and errors need not be default-constructible.
template <typename Value, typename Error>
union lookup_union {
  Value value;
  Error error;

  lookup_union() {}

  template <typename... Args>
  constexpr lookup_union(lookup_value_tag, Args &&...args)
    : value(std::forward<Args>(args)...) {}

  template <typename... Args>
  constexpr lookup_union(lookup_error_tag, Args &&...args)
    : error(std::forward<Args>(args)...) {}

  ~lookup_union() {}
};

template <typename Value, typename Error>
struct lookup_storage_base {
  bool available;
  lookup_union<Value, Error> data;

  template <typename... Args>
  constexpr lookup_storage_base(lookup_value_tag tag, Args &&...args)
    : available(true), data(tag, std::forward<Args>(args)...) {}

  template <typename... Args>
  constexpr lookup_storage_base(lookup_error_tag tag, Args &&...args)
    : available(false), data(tag, std::forward<Args>(args)...) {}

  // The union provides writable storage even when an alternative is const.
  lookup_storage_base(const lookup_storage_base &other)
    : available(other.available) {
    if (this->available)
      ::new (static_cast<void *>(std::addressof(this->data)))
        Value(other.data.value);
    else
      ::new (static_cast<void *>(std::addressof(this->data)))
        Error(other.data.error);
  }

  lookup_storage_base(lookup_storage_base &&other)
    noexcept(std::is_nothrow_move_constructible<Value>::value
      && std::is_nothrow_move_constructible<Error>::value)
    : available(other.available) {
    if (this->available)
      ::new (static_cast<void *>(std::addressof(this->data)))
        Value(std::move(other.data.value));
    else
      ::new (static_cast<void *>(std::addressof(this->data)))
        Error(std::move(other.data.error));
  }

  ~lookup_storage_base() {
    if (this->available)
      this->data.value.~Value();
    else
      this->data.error.~Error();
  }
};

// Separate copy control keeps type traits accurate for move-only alternatives.
template <typename Value,
  typename Error,
  bool Copy = std::is_copy_constructible<Value>::value
    && std::is_copy_constructible<Error>::value>
struct lookup_storage: lookup_storage_base<Value, Error> {
  using base = lookup_storage_base<Value, Error>;
  using base::base;

  lookup_storage(const lookup_storage &) = default;
  lookup_storage(lookup_storage &&) = default;
};

template <typename Value, typename Error>
struct lookup_storage<Value, Error, false>:
    lookup_storage_base<Value, Error> {
  using base = lookup_storage_base<Value, Error>;
  using base::base;

  lookup_storage(const lookup_storage &) = delete;
  lookup_storage(lookup_storage &&) = default;
};

template <bool Move>
struct lookup_move_control {
  constexpr lookup_move_control() = default;
  lookup_move_control(const lookup_move_control &) = default;
  lookup_move_control(lookup_move_control &&) = default;
};

template <>
struct lookup_move_control<false> {
  constexpr lookup_move_control() = default;
  lookup_move_control(const lookup_move_control &) = default;
  lookup_move_control(lookup_move_control &&) = delete;
};

template <typename Value>
using lookup_stored_value = compat::conditional_t<std::is_void<Value>::value,
  lookup_unit, Value>;

template <typename Value>
struct lookup_value_access {
  template <typename Slot>
  static constexpr Slot &&get(Slot &&value) {
    return std::forward<Slot>(value);
  }

  template <typename Function, typename Slot>
  static auto call(Function &&f, Slot &&value)
    -> decltype(compat::invoke(std::forward<Function>(f),
      std::forward<Slot>(value))) {
    return compat::invoke(std::forward<Function>(f),
      std::forward<Slot>(value));
  }
};

template <typename Tuple>
struct lookup_value_access<lookup_selection_t<Tuple>> {
  template <typename Slot>
  static constexpr Slot &&get(Slot &&value) {
    return std::forward<Slot>(value);
  }

  template <typename Function, typename Slot>
  static auto call(Function &&f, Slot &&value)
    -> decltype(value.visit(std::forward<Function>(f))) {
    return value.visit(std::forward<Function>(f));
  }
};

template <>
struct lookup_value_access<void> {
  template <typename Slot>
  static void get(Slot &&) {}

  template <typename Function, typename Slot>
  static auto call(Function &&f, Slot &&)
    -> decltype(compat::invoke(std::forward<Function>(f))) {
    return compat::invoke(std::forward<Function>(f));
  }
};

template <>
struct lookup_value_access<empty_lookup_value> {
  template <typename Slot>
  static constexpr Slot &&get(Slot &&value) {
    return std::forward<Slot>(value);
  }

  template <typename Function, typename Slot>
  static empty_lookup_value call(Function &&, Slot &&) {
    assert(false);
    return {};
  }
};

template <typename Value>
struct is_empty_lookup_value: std::false_type {};

template <>
struct is_empty_lookup_value<empty_lookup_value>: std::true_type {};

template <typename Tuple>
struct is_empty_lookup_value<lookup_selection_t<Tuple>>:
    compat::bool_constant<std::tuple_size<Tuple>::value == 0> {};

template <typename Value>
struct lookup_transform_return {
  template <typename Result, typename Apply>
  static Result call(Apply &&apply) {
    return Result::success(std::forward<Apply>(apply)());
  }
};

template <>
struct lookup_transform_return<void> {
  template <typename Result, typename Apply>
  static Result call(Apply &&apply) {
    std::forward<Apply>(apply)();
    return Result::success();
  }
};

// A named invocation closure supplies the nullary form in C++11.
template <typename Value, typename Function, typename Slot>
struct lookup_apply {
  Function &function;
  Slot &slot;

  auto operator()() const
    -> decltype(lookup_value_access<Value>::call(
      std::forward<Function>(this->function), std::forward<Slot>(this->slot))) {
    return lookup_value_access<Value>::call(
      std::forward<Function>(this->function), std::forward<Slot>(this->slot));
  }
};

template <typename Value>
struct lookup_expected_value {
  template <typename Result, typename Next>
  static Result take(Next &&next) {
    return Result::success(std::forward<Next>(next).value());
  }
};

template <>
struct lookup_expected_value<void> {
  template <typename Result, typename Next>
  static Result take(Next &&next) {
    std::forward<Next>(next).value();
    return Result::success();
  }
};

template <typename Result, typename Next>
Result lookup_from_expected(Next &&next) {
  if (next.has_value())
    return lookup_expected_value<typename Result::value_type>::
      template take<Result>(std::forward<Next>(next));

  return Result::failure(std::forward<Next>(next).error());
}

template <typename Next, typename Error>
struct lookup_bind_traits {
  using value_type = typename Next::value_type;

  template <typename Result, typename Apply>
  static Result call(Apply &&apply) {
    static_assert(traits::is_expected_like<Next>::value,
      "and_then requires an expected-like result");
    static_assert(std::is_same<typename Next::error_type, Error>::value,
      "and_then requires the same error type");
    return lookup_from_expected<Result>(std::forward<Apply>(apply)());
  }
};

// An empty table has no success alternative and must not instantiate a visitor.
template <typename Error>
struct lookup_bind_traits<empty_lookup_value, Error> {
  using value_type = empty_lookup_value;

  template <typename Result, typename Apply>
  static Result call(Apply &&) {
    assert(false);
    return Result::success(empty_lookup_value{});
  }
};

} // namespace detail

/** Expected-like value or error produced by a lookup and its transformations.
 * A lookup's value is a selection whose visit forwards one table entry.
 * transform and and_then visit that entry directly; after transformation,
 * they receive the transformed value instead. Void values call f with no args.
 * Results own transformed values and errors. Initial selections borrow their
 * table, which must outlive the result. Construction is supported; assignment
 * is deliberately unavailable.
 * @pre value access requires success; error access requires failure. */
template <typename Value, typename Error>
class lookup_result_t:
    private detail::lookup_storage<detail::lookup_stored_value<Value>, Error>,
    private detail::lookup_move_control<
      std::is_move_constructible<detail::lookup_stored_value<Value>>::value
        && std::is_move_constructible<Error>::value> {
  using stored_value = detail::lookup_stored_value<Value>;
  using storage = detail::lookup_storage<stored_value, Error>;

  template <typename Tag, typename... Args>
  explicit constexpr lookup_result_t(Tag tag, Args &&...args)
    : storage(tag, std::forward<Args>(args)...) {}

  template <typename Self, typename Function>
  using invocation = decltype(detail::lookup_value_access<Value>::call(
    std::declval<Function>(), std::declval<Self>().data.value));

  template <typename Self, typename Function>
  static auto transform_impl(Self &&self, Function &&f)
    -> lookup_result_t<compat::decay_t<invocation<Self, Function>>, Error> {
    using mapped_value = compat::decay_t<invocation<Self, Function>>;
    using result = lookup_result_t<mapped_value, Error>;
    if (!self.has_value())
      return result::failure(std::forward<Self>(self).error());

    return detail::lookup_transform_return<mapped_value>::template call<result>(
      detail::lookup_apply<Value, Function, decltype((
        std::forward<Self>(self).data.value))>{f, self.data.value});
  }

  template <typename Self, typename Function>
  static auto and_then_impl(Self &&self, Function &&f)
    -> lookup_result_t<typename detail::lookup_bind_traits<
      compat::remove_cvref_t<invocation<Self, Function>>, Error>::value_type,
      Error> {
    using bind = detail::lookup_bind_traits<
      compat::remove_cvref_t<invocation<Self, Function>>, Error>;
    using result = lookup_result_t<typename bind::value_type, Error>;
    if (!self.has_value())
      return result::failure(std::forward<Self>(self).error());

    return bind::template call<result>(
      detail::lookup_apply<Value, Function, decltype((
        std::forward<Self>(self).data.value))>{f, self.data.value});
  }

  template <typename Self, typename Function>
  static auto transform_error_impl(Self &&self, Function &&f)
    -> lookup_result_t<Value, compat::decay_t<compat::invoke_result_t<Function,
      decltype(std::forward<Self>(self).error())>>> {
    using result = lookup_result_t<Value,
      compat::decay_t<compat::invoke_result_t<Function,
        decltype(std::forward<Self>(self).error())>>>;
    if (self.has_value())
      return detail::lookup_expected_value<Value>::template take<result>(
        std::forward<Self>(self));

    return result::failure(compat::invoke(std::forward<Function>(f),
      std::forward<Self>(self).error()));
  }

  template <typename Self, typename Function>
  using recovery_type = compat::remove_cvref_t<compat::invoke_result_t<Function,
    decltype(std::declval<Self>().error())>>;

  template <typename Self, typename Function>
  using recovery_result = lookup_result_t<compat::conditional_t<
    detail::is_empty_lookup_value<Value>::value,
    typename recovery_type<Self, Function>::value_type, Value>,
    typename recovery_type<Self, Function>::error_type>;

  template <typename Self, typename Function>
  static auto recover(Self &&self, Function &&f, std::false_type)
    -> recovery_result<Self, Function> {
    using next_type = recovery_type<Self, Function>;
    static_assert(std::is_same<typename next_type::value_type, Value>::value,
      "or_else requires the same value type");
    using result = recovery_result<Self, Function>;
    if (self.has_value())
      return detail::lookup_expected_value<Value>::template take<result>(
        std::forward<Self>(self));

    return detail::lookup_from_expected<result>(compat::invoke(
      std::forward<Function>(f), std::forward<Self>(self).error()));
  }

  template <typename Self, typename Function>
  static auto recover(Self &&self, Function &&f, std::true_type)
    -> recovery_result<Self, Function> {
    assert(!self.has_value());
    return detail::lookup_from_expected<recovery_result<Self, Function>>(
      compat::invoke(std::forward<Function>(f),
        std::forward<Self>(self).error()));
  }

  template <typename Self, typename Function>
  static auto or_else_impl(Self &&self, Function &&f)
    -> recovery_result<Self, Function> {
    static_assert(traits::is_expected_like<
      recovery_type<Self, Function>>::value,
      "or_else requires an expected-like result");
    return recover(std::forward<Self>(self), std::forward<Function>(f),
      detail::is_empty_lookup_value<Value>{});
  }

public:
  using value_type = Value;
  using error_type = Error;

  lookup_result_t(const lookup_result_t &) = default;
  lookup_result_t(lookup_result_t &&) = default;
  lookup_result_t &operator=(const lookup_result_t &) = delete;
  lookup_result_t &operator=(lookup_result_t &&) = delete;

  /** Construct an available value. Void results take no arguments. */
  template <typename... Args>
  static lookup_result_t success(Args &&...args) {
    return lookup_result_t(detail::lookup_value_tag{},
      std::forward<Args>(args)...);
  }

  /** Construct an error. */
  template <typename... Args>
  static lookup_result_t failure(Args &&...args) {
    return lookup_result_t(detail::lookup_error_tag{},
      std::forward<Args>(args)...);
  }

  bool has_value() const noexcept { return this->available; }
  explicit operator bool() const noexcept { return this->has_value(); }

  auto value() & -> decltype(detail::lookup_value_access<Value>::get(
    std::declval<stored_value &>())) {
    assert(this->has_value());
    return detail::lookup_value_access<Value>::get(this->data.value);
  }

  auto value() const & -> decltype(detail::lookup_value_access<Value>::get(
    std::declval<const stored_value &>())) {
    assert(this->has_value());
    return detail::lookup_value_access<Value>::get(this->data.value);
  }

  auto value() && -> decltype(detail::lookup_value_access<Value>::get(
    std::declval<stored_value &&>())) {
    assert(this->has_value());
    return detail::lookup_value_access<Value>::get(std::move(this->data.value));
  }

  auto value() const && -> decltype(detail::lookup_value_access<Value>::get(
    std::declval<const stored_value &&>())) {
    assert(this->has_value());
    return detail::lookup_value_access<Value>::get(std::move(this->data.value));
  }

  auto operator*() & -> decltype(this->value()) { return this->value(); }
  auto operator*() const & -> decltype(this->value()) { return this->value(); }
  auto operator*() && -> decltype(std::move(*this).value()) {
    return std::move(*this).value();
  }
  auto operator*() const && -> decltype(std::move(*this).value()) {
    return std::move(*this).value();
  }

  Error &error() & {
    assert(!this->has_value());
    return this->data.error;
  }
  const Error &error() const & {
    assert(!this->has_value());
    return this->data.error;
  }
  Error &&error() && {
    assert(!this->has_value());
    return std::move(this->data.error);
  }
  const Error &&error() const && {
    assert(!this->has_value());
    return std::move(this->data.error);
  }

  /** Transform the selected entry (or mapped value), retaining an error. */
  template <typename Function>
  auto transform(Function &&f) &
    -> decltype(transform_impl(*this, std::forward<Function>(f))) {
    return transform_impl(*this, std::forward<Function>(f));
  }

  template <typename Function>
  auto transform(Function &&f) const &
    -> decltype(transform_impl(*this, std::forward<Function>(f))) {
    return transform_impl(*this, std::forward<Function>(f));
  }

  template <typename Function>
  auto transform(Function &&f) &&
    -> decltype(transform_impl(std::move(*this), std::forward<Function>(f))) {
    return transform_impl(std::move(*this), std::forward<Function>(f));
  }

  template <typename Function>
  auto transform(Function &&f) const &&
    -> decltype(transform_impl(std::move(*this), std::forward<Function>(f))) {
    return transform_impl(std::move(*this), std::forward<Function>(f));
  }

  /** Sequence an expected-like result from the selected entry or value. */
  template <typename Function>
  auto and_then(Function &&f) &
    -> decltype(and_then_impl(*this, std::forward<Function>(f))) {
    return and_then_impl(*this, std::forward<Function>(f));
  }

  template <typename Function>
  auto and_then(Function &&f) const &
    -> decltype(and_then_impl(*this, std::forward<Function>(f))) {
    return and_then_impl(*this, std::forward<Function>(f));
  }

  template <typename Function>
  auto and_then(Function &&f) &&
    -> decltype(and_then_impl(std::move(*this), std::forward<Function>(f))) {
    return and_then_impl(std::move(*this), std::forward<Function>(f));
  }

  template <typename Function>
  auto and_then(Function &&f) const &&
    -> decltype(and_then_impl(std::move(*this), std::forward<Function>(f))) {
    return and_then_impl(std::move(*this), std::forward<Function>(f));
  }

  /** Transform a missing-name or later error, retaining a value. */
  template <typename Function>
  auto transform_error(Function &&f) &
    -> decltype(transform_error_impl(*this, std::forward<Function>(f))) {
    return transform_error_impl(*this, std::forward<Function>(f));
  }

  template <typename Function>
  auto transform_error(Function &&f) const &
    -> decltype(transform_error_impl(*this, std::forward<Function>(f))) {
    return transform_error_impl(*this, std::forward<Function>(f));
  }

  template <typename Function>
  auto transform_error(Function &&f) &&
    -> decltype(transform_error_impl(std::move(*this),
      std::forward<Function>(f))) {
    return transform_error_impl(std::move(*this), std::forward<Function>(f));
  }

  template <typename Function>
  auto transform_error(Function &&f) const &&
    -> decltype(transform_error_impl(std::move(*this),
      std::forward<Function>(f))) {
    return transform_error_impl(std::move(*this), std::forward<Function>(f));
  }

  /** Recover an error with another expected-like result. */
  template <typename Function>
  auto or_else(Function &&f) &
    -> decltype(or_else_impl(*this, std::forward<Function>(f))) {
    return or_else_impl(*this, std::forward<Function>(f));
  }

  template <typename Function>
  auto or_else(Function &&f) const &
    -> decltype(or_else_impl(*this, std::forward<Function>(f))) {
    return or_else_impl(*this, std::forward<Function>(f));
  }

  template <typename Function>
  auto or_else(Function &&f) &&
    -> decltype(or_else_impl(std::move(*this), std::forward<Function>(f))) {
    return or_else_impl(std::move(*this), std::forward<Function>(f));
  }

  template <typename Function>
  auto or_else(Function &&f) const &&
    -> decltype(or_else_impl(std::move(*this), std::forward<Function>(f))) {
    return or_else_impl(std::move(*this), std::forward<Function>(f));
  }
};

namespace detail {

template <typename Tuple, typename Function,
  bool Empty = (std::tuple_size<Tuple>::value == 0)>
struct lookup_visit_type {
  using type = compat::invoke_result_t<Function,
    const typename std::tuple_element<0, Tuple>::type &>;
};

template <typename Tuple, typename Function>
struct lookup_visit_type<Tuple, Function, true> {
  using type = empty_lookup_value;
};

template <typename Tuple, typename Function, std::size_t Index>
auto invoke_lookup_entry(const Tuple &entries, Function &&f)
  -> typename lookup_visit_type<Tuple, Function>::type {
  return compat::invoke(std::forward<Function>(f),
    compat::detail::tuple_get<Index>(entries));
}

template <typename Tuple, typename Function>
empty_lookup_value visit_lookup_entry(const Tuple &, std::size_t,
  Function &&, compat::index_sequence<>) {
  assert(false);
  return {};
}

template <typename Tuple, typename Function, std::size_t... Index>
auto visit_lookup_entry(const Tuple &entries, std::size_t index,
  Function &&f, compat::index_sequence<Index...>)
  -> typename lookup_visit_type<Tuple, Function>::type {
  using result = typename lookup_visit_type<Tuple, Function>::type;
  static_assert(compat::conjunction<std::is_same<result,
    compat::invoke_result_t<Function,
      const typename std::tuple_element<Index, Tuple>::type &>>...>::value,
    "Lookup visitors must return the same type for every entry");
  using operation = result (*)(const Tuple &, Function &&);
  static constexpr operation operations[] = {
    invoke_lookup_entry<Tuple, Function, Index>...,
  };
  return operations[index](entries, std::forward<Function>(f));
}

template <typename Meta>
constexpr std::size_t lookup_name_length(std::size_t index = 0) {
  return Meta::name()[index] == '\0' ? index
    : lookup_name_length<Meta>(index + 1);
}

template <typename Meta>
constexpr auto lookup_name_size(int) -> decltype(Meta::name().size()) {
  return Meta::name().size();
}

template <typename Meta>
constexpr std::size_t lookup_name_size(long) {
  return lookup_name_length<Meta>();
}

constexpr std::size_t lookup_max_size() { return 0; }

constexpr std::size_t lookup_max_pair(std::size_t left, std::size_t right) {
  return left < right ? right : left;
}

template <typename... Rest>
constexpr std::size_t lookup_max_size(std::size_t first, Rest... rest) {
  return lookup_max_pair(first, lookup_max_size(rest...));
}

// C++11 does not require constexpr std::array accessors (notably in libc++).
// Raw backing keeps all generated table data usable in constant expressions.
template <typename T, std::size_t Size>
struct lookup_array {
  T values[Size ? Size : 1];

  constexpr const T &operator[](std::size_t index) const {
    return this->values[index];
  }
  constexpr const T *data() const { return this->values; }
  constexpr std::size_t size() const { return Size; }
};

template <std::size_t Width>
struct lookup_name {
  lookup_array<char, Width> bytes;
  std::size_t size;

  template <typename Key>
  bool matches(const Key &key) const {
    return this->size == key.size()
      && (!this->size
        || std::memcmp(this->bytes.data(), key.data(), this->size) == 0);
  }
};

template <typename Meta, std::size_t... Index>
constexpr lookup_name<sizeof...(Index)> make_lookup_name(
  compat::index_sequence<Index...>) {
  return {
    {{(Index < lookup_name_size<Meta>(0) ? Meta::name()[Index] : '\0')...}},
    lookup_name_size<Meta>(0),
  };
}

constexpr std::uint64_t lookup_hash_step(std::uint64_t hash, char byte) {
  return (hash ^ static_cast<unsigned char>(byte)) * UINT64_C(1099511628211);
}

template <std::size_t Width>
constexpr std::uint64_t lookup_static_hash(const lookup_name<Width> &name,
  std::size_t index = 0, std::uint64_t hash = UINT64_C(14695981039346656037)) {
  return index == name.size ? hash
    : lookup_static_hash(name, index + 1,
        lookup_hash_step(hash, name.bytes[index]));
}

template <std::size_t Width>
constexpr bool lookup_names_equal(const lookup_name<Width> &left,
  const lookup_name<Width> &right, std::size_t index = 0) {
  return left.size == right.size
    && (index == left.size || (left.bytes[index] == right.bytes[index]
      && lookup_names_equal(left, right, index + 1)));
}

constexpr std::size_t lookup_bucket_count(std::size_t count,
  std::size_t capacity = 1) {
  return capacity >= count * 2 ? capacity
    : lookup_bucket_count(count, capacity * 2);
}

template <typename Data, bool Fits = (Data::count <= 64)>
struct lookup_bitmask_data;

template <typename Data, bool Small = (Data::count <= 8)>
struct lookup_search_policy;

template <typename... Meta>
struct lookup_table_data {
  static constexpr std::size_t count = sizeof...(Meta);
  static constexpr std::size_t width =
    lookup_max_size(std::size_t{1}, lookup_name_size<Meta>(0)...);
  static constexpr std::size_t bucket_count = lookup_bucket_count(count);
  using names_type = lookup_array<lookup_name<width>, count>;
  static constexpr names_type names{{
    make_lookup_name<Meta>(compat::make_index_sequence<width>{})...,
  }};

  template <std::size_t... Index>
  static constexpr lookup_array<std::uint64_t, count> make_hashes(
    compat::index_sequence<Index...>) {
    return {{lookup_static_hash(names[Index])...}};
  }

  // Compare cached hashes before complete names in pairwise uniqueness checks;
  // older constexpr evaluators otherwise retain repeated full-name comparisons.
  static constexpr lookup_array<std::uint64_t, count> hashes =
    make_hashes(compat::make_index_sequence<count>{});

  // Divide the pair range to bound C++11 constant-evaluation recursion depth.
  static constexpr bool unique(std::size_t begin = 0,
    std::size_t size = count * count) {
    return size == 0 ? true
      : size == 1 ? (begin / count >= begin % count
        || hashes[begin / count] != hashes[begin % count]
        || !lookup_names_equal(names[begin / count], names[begin % count]))
      : unique(begin, size / 2) && unique(begin + size / 2, size - size / 2);
  }

  static constexpr std::size_t head(std::size_t bucket, std::size_t index = 0) {
    return index == count ? count
      : (hashes[index] & (bucket_count - 1)) == bucket
        ? index : head(bucket, index + 1);
  }

  static constexpr std::size_t next(std::size_t index) {
    return head(hashes[index] & (bucket_count - 1),
      index + 1);
  }

  template <std::size_t... Index>
  static constexpr lookup_array<std::size_t, bucket_count> make_heads(
    compat::index_sequence<Index...>) { return {{head(Index)...}}; }

  template <std::size_t... Index>
  static constexpr lookup_array<std::size_t, count> make_next(
    compat::index_sequence<Index...>) { return {{next(Index)...}}; }

  static constexpr lookup_array<std::size_t, bucket_count> heads =
    make_heads(compat::make_index_sequence<bucket_count>{});
  static constexpr lookup_array<std::size_t, count> links =
    make_next(compat::make_index_sequence<count>{});

  template <typename Key>
  static std::size_t find_hashed(const Key &key) {
    if (!count)
      return 0;

    auto hash = UINT64_C(14695981039346656037);
    for (std::size_t index = 0; index != key.size(); ++index)
      hash = lookup_hash_step(hash, key[index]);

    auto index = heads[hash & (bucket_count - 1)];
    while (index != count && !names[index].matches(key))
      index = links[index];

    return index;
  }

  template <typename Key>
  static std::size_t find(const Key &key) {
    return lookup_search_policy<lookup_table_data>::find(key);
  }
};

template <typename... Meta>
constexpr typename lookup_table_data<Meta...>::names_type
  lookup_table_data<Meta...>::names;

template <typename... Meta>
constexpr lookup_array<std::uint64_t, lookup_table_data<Meta...>::count>
  lookup_table_data<Meta...>::hashes;

template <typename... Meta>
constexpr lookup_array<std::size_t, lookup_table_data<Meta...>::bucket_count>
  lookup_table_data<Meta...>::heads;

template <typename... Meta>
constexpr lookup_array<std::size_t, lookup_table_data<Meta...>::count>
  lookup_table_data<Meta...>::links;

inline std::size_t lookup_first_bit(std::uint64_t bits) {
  assert(bits);
#if defined(__cpp_lib_bitops) && 201907L <= __cpp_lib_bitops
  return static_cast<std::size_t>(std::countr_zero(bits));
#elif defined(__GNUC__) || defined(__clang__)
  return static_cast<std::size_t>(__builtin_ctzll(bits));
#else
  std::size_t index = 0;
  while (!(bits & 1)) {
    bits >>= 1;
    ++index;
  }
  return index;
#endif
}

template <typename Data>
using lookup_mask = compat::conditional_t<(Data::count <= 8), std::uint8_t,
  compat::conditional_t<(Data::count <= 16), std::uint16_t,
    compat::conditional_t<(Data::count <= 32), std::uint32_t, std::uint64_t>>>;

// Row constants keep each C++11 evaluation bounded independently of key length.
template <typename Data, std::size_t Position>
struct lookup_mask_row {
  using mask = lookup_mask<Data>;
  using row = lookup_array<mask, 256>;

  // Keep the recursive evaluator's input narrow: indexing the complete name
  // table for each byte retains excessive constexpr state in older compilers.
  // Character 256 excludes names that have no byte at this position.
  template <std::size_t... Index>
  static constexpr lookup_array<unsigned short, Data::count> make_characters(
    compat::index_sequence<Index...>) {
    return {{static_cast<unsigned short>(Position < Data::names[Index].size
      ? static_cast<unsigned char>(Data::names[Index].bytes[Position])
      : 256)...}};
  }

  static constexpr lookup_array<unsigned short, Data::count> characters =
    make_characters(compat::make_index_sequence<Data::count>{});

  static constexpr mask bits(unsigned byte, std::size_t index = 0) {
    return index == Data::count ? mask{}
      : static_cast<mask>((characters[index] == byte
          ? (UINT64_C(1) << index) : 0) | bits(byte, index + 1));
  }

  template <std::size_t... Byte>
  static constexpr row make(compat::index_sequence<Byte...>) {
    return {{bits(Byte)...}};
  }

  static constexpr row value = make(compat::make_index_sequence<256>{});
};

template <typename Data, std::size_t Position>
constexpr lookup_array<unsigned short, Data::count>
  lookup_mask_row<Data, Position>::characters;

template <typename Data, std::size_t Position>
constexpr typename lookup_mask_row<Data, Position>::row
  lookup_mask_row<Data, Position>::value;

template <typename Data>
struct lookup_bitmask_data<Data, true> {
  using mask = lookup_mask<Data>;
  using rows = lookup_array<lookup_array<mask, 256>, Data::width>;

  template <std::size_t... Position>
  static constexpr rows make_rows(compat::index_sequence<Position...>) {
    return {{lookup_mask_row<Data, Position>::value...}};
  }

  static constexpr mask length_bits(std::size_t length, std::size_t index = 0) {
    return index == Data::count ? mask{}
      : static_cast<mask>((Data::names[index].size == length
          ? (UINT64_C(1) << index) : 0) | length_bits(length, index + 1));
  }

  template <std::size_t... Length>
  static constexpr lookup_array<mask, Data::width + 1> make_lengths(
    compat::index_sequence<Length...>) { return {{length_bits(Length)...}}; }

  static constexpr rows byte_masks =
    make_rows(compat::make_index_sequence<Data::width>{});
  static constexpr lookup_array<mask, Data::width + 1> length_masks =
    make_lengths(compat::make_index_sequence<Data::width + 1>{});

  template <typename Key>
  static std::size_t find(const Key &key) {
    if (key.size() > Data::width)
      return Data::count;

    auto candidates = length_masks[key.size()];
    std::size_t position = 0;
    for (; candidates && position + 4 <= key.size(); position += 4) {
      candidates &= byte_masks[position][
        static_cast<unsigned char>(key[position])];
      candidates &= byte_masks[position + 1][
        static_cast<unsigned char>(key[position + 1])];
      candidates &= byte_masks[position + 2][
        static_cast<unsigned char>(key[position + 2])];
      candidates &= byte_masks[position + 3][
        static_cast<unsigned char>(key[position + 3])];
    }
    for (; candidates && position != key.size(); ++position)
      candidates &= byte_masks[position][
        static_cast<unsigned char>(key[position])];

    return candidates ? lookup_first_bit(candidates) : Data::count;
  }
};

template <typename Data>
constexpr typename lookup_bitmask_data<Data, true>::rows
  lookup_bitmask_data<Data, true>::byte_masks;

template <typename Data>
constexpr lookup_array<lookup_mask<Data>, Data::width + 1>
  lookup_bitmask_data<Data, true>::length_masks;

template <typename Data>
struct lookup_bitmask_data<Data, false> {
  template <typename Key>
  static std::size_t find(const Key &key) { return Data::find_hashed(key); }
};

template <typename Data>
constexpr bool lookup_uniform_lengths(std::size_t index = 0) {
  return index == Data::count || (Data::names[index].size == Data::width
    && lookup_uniform_lengths<Data>(index + 1));
}

template <typename Data,
  bool Uniform = (Data::count <= 64 && lookup_uniform_lengths<Data>())>
struct lookup_sampled_search: lookup_bitmask_data<Data> {};

// Equal-length names favor two discriminating bytes and one fixed-size
// comparison in measured lookups. Instantiate only those rows when sufficient.
template <typename Data>
struct lookup_sampled_search<Data, true> {
  struct choice {
    std::size_t position;
    std::size_t collisions;
  };

  using signatures = lookup_array<unsigned short, Data::count>;

  // Cache only the selected bytes while scoring pairs, rather than retaining
  // the complete names in every C++11 constant-evaluation recursion frame.
  template <std::size_t... Index>
  static constexpr signatures make_signatures(std::size_t position,
    std::size_t prior, compat::index_sequence<Index...>) {
    return {{static_cast<unsigned short>(
      static_cast<unsigned char>(Data::names[Index].bytes[position])
        | (prior == Data::width ? 0u
          : 256u * static_cast<unsigned char>(
            Data::names[Index].bytes[prior])))...}};
  }

  // Balanced pair traversal bounds C++11 constant-evaluation recursion depth.
  static constexpr std::size_t count_collisions(const signatures &bytes,
    std::size_t begin, std::size_t size) {
    return size == 0 ? 0
      : size == 1 ? (begin / Data::count < begin % Data::count
          && bytes[begin / Data::count] == bytes[begin % Data::count])
      : count_collisions(bytes, begin, size / 2)
        + count_collisions(bytes, begin + size / 2, size - size / 2);
  }

  static constexpr std::size_t collisions(std::size_t position,
    std::size_t prior) {
    return count_collisions(make_signatures(position, prior,
      compat::make_index_sequence<Data::count>{}), 0,
      Data::count * Data::count);
  }

  static constexpr choice better(choice left, choice right) {
    return left.collisions <= right.collisions ? left : right;
  }

  static constexpr choice best(std::size_t prior, std::size_t begin = 0,
    std::size_t size = Data::width) {
    return size == 1 ? choice{begin, collisions(begin, prior)}
      : better(best(prior, begin, size / 2),
          best(prior, begin + size / 2, size - size / 2));
  }

  static constexpr std::size_t first = best(Data::width).position;
  static constexpr std::size_t second = best(first).position;

  template <typename Key>
  static std::size_t find(const Key &key, std::true_type) {
    if (key.size() != Data::width)
      return Data::count;

    const auto candidates = lookup_mask_row<Data, first>::value[
      static_cast<unsigned char>(key[first])]
      & lookup_mask_row<Data, second>::value[
        static_cast<unsigned char>(key[second])];
    if (!candidates)
      return Data::count;

    const auto index = lookup_first_bit(candidates);
    return std::memcmp(Data::names[index].bytes.data(), key.data(),
      Data::width) == 0 ? index : Data::count;
  }

  template <typename Key>
  static std::size_t find(const Key &key, std::false_type) {
    return lookup_bitmask_data<Data>::find(key);
  }

  template <typename Key>
  static std::size_t find(const Key &key) {
    return find(key, compat::bool_constant<collisions(second, first) == 0>{});
  }
};

template <typename Data,
  std::size_t Index = 0,
  bool End = (Index == Data::count)>
struct lookup_linear_scan {
  template <typename Key>
  static std::size_t find(const Key &key) {
    return Data::names[Index].matches(key) ? Index
      : lookup_linear_scan<Data, Index + 1>::find(key);
  }
};

template <typename Data, std::size_t Index>
struct lookup_linear_scan<Data, Index, true> {
  template <typename Key>
  static std::size_t find(const Key &) { return Data::count; }
};

// Measured short tables favor literal comparisons. Larger tables use byte-mask
// intersections; above one word the collision-checked hash table is used.
template <typename Data>
struct lookup_search_policy<Data, true>: lookup_linear_scan<Data> {};

template <typename Data>
struct lookup_search_policy<Data, false>: lookup_sampled_search<Data> {};

template <typename Key, typename = void>
struct is_lookup_key: std::false_type {};

template <typename Key>
struct is_lookup_key<Key, compat::void_t<
  decltype(std::declval<const Key &>().size()),
  decltype(std::declval<const Key &>().data()),
  decltype(std::declval<const Key &>()[std::size_t{}])>>:
    compat::conjunction<
      std::is_integral<compat::remove_cvref_t<decltype(
        std::declval<const Key &>().size())>>,
      std::is_convertible<decltype(std::declval<const Key &>().data()),
        const char *>,
      std::is_convertible<decltype(std::declval<const Key &>()[std::size_t{}]),
        char>> {};

} // namespace detail

/** One borrowed table entry, retaining its concrete type for visitation.
 * All visitor alternatives must return the same type.
 * @pre The source table remains alive. */
template <typename Tuple>
struct lookup_selection_t {
  const Tuple *entries;
  std::size_t index;

  template <typename Function>
  auto visit(Function &&f) const
    -> typename detail::lookup_visit_type<Tuple, Function>::type {
    assert(this->index < std::tuple_size<Tuple>::value);
    return detail::visit_lookup_entry(*this->entries, this->index,
      std::forward<Function>(f),
      compat::make_index_sequence<std::tuple_size<Tuple>::value>{});
  }
};

/** A heterogeneous table of named descriptors.
 * Meta-like entries expose a static constexpr name() returning a narrow,
 * null-terminated string or a string-like value with size() and operator[].
 * Names must be unique. Supplied entries are copied into the table.
 * lookup keys provide integral size(), data() convertible to const char*,
 * and indexed characters. Results own a decayed key on failure and borrow
 * the selected descriptor on success.
 * @pre The table outlives its lookup results and their untransformed values. */
template <typename... Meta>
struct lookup_table_t {
  static_assert(detail::lookup_table_data<Meta...>::unique(),
    "lookup_table requires unique entry names");
#if OMNI_FN_LOOKUP_INDEXED_ENTRIES
  using entries_type = detail::lookup_entries<Meta...>;
#else
  using entries_type = std::tuple<Meta...>;
#endif
  entries_type entries;

  /** Find a name and retain the selected descriptor's concrete type.
   * lookup :: name -> result<selection<meta...>, not_found<name>>
   * transform/and_then pass the selected meta directly to their callback.
   * or_else can return another lookup to resolve an alias. */
  OMNI_CONSTRAINED_TEMPLATE((typename Key),
    (detail::is_lookup_key<typename std::remove_reference<Key>::type>::value
      && std::is_constructible<compat::decay_t<Key>, Key &&>::value))
  auto lookup(Key &&key) const &
    -> lookup_result_t<lookup_selection_t<entries_type>,
      not_found_t<compat::decay_t<Key>>> {
    using result = lookup_result_t<lookup_selection_t<entries_type>,
      not_found_t<compat::decay_t<Key>>>;
    const auto index = detail::lookup_table_data<Meta...>::find(key);
    if (index == sizeof...(Meta))
      return result::failure(not_found_t<compat::decay_t<Key>>{
        std::forward<Key>(key)});

    return result::success(lookup_selection_t<entries_type>{
      std::addressof(this->entries), index});
  }

  /** Look up a literal; C++17 uses a view, earlier standards a string. */
  template <std::size_t Size>
  auto lookup(const char (&key)[Size]) const &
#if defined(__cpp_lib_string_view) && 201606L <= __cpp_lib_string_view
    -> decltype(this->lookup(std::string_view{key, Size - 1})) {
    return this->lookup(std::string_view{key, Size - 1});
#else
    -> decltype(this->lookup(std::string{key, Size - 1})) {
    return this->lookup(std::string{key, Size - 1});
#endif
  }

  /** Deleted: lookup results borrow the table. */
  template <typename Key>
  void lookup(Key &&) const && = delete;
};

/** Construct a constexpr table from meta-like descriptors with static names.
 * lookup_table :: meta... -> table<meta...>
 * The table copies the supplied descriptors and rejects duplicate names. */
template <typename... Meta>
constexpr lookup_table_t<Meta...> lookup_table(const Meta &...meta) {
  return {/*entries=*/typename lookup_table_t<Meta...>::entries_type{meta...}};
}

// Tuple input must be unpacked when indexed storage replaces std::tuple.
#if OMNI_FN_LOOKUP_INDEXED_ENTRIES
namespace detail {

template <typename... Meta, std::size_t... Index>
constexpr lookup_table_t<Meta...> lookup_table_from_tuple(
  const std::tuple<Meta...> &meta, compat::index_sequence<Index...>) {
  return lookup_table(std::get<Index>(meta)...);
}
} // namespace detail

template <typename... Meta>
constexpr lookup_table_t<Meta...> lookup_table(
  const detail::lookup_entries<Meta...> &meta) {
  return {/*entries=*/meta};
}
#endif

/** Construct a named table from an existing heterogeneous tuple. */
template <typename... Meta>
constexpr lookup_table_t<Meta...> lookup_table(
  const std::tuple<Meta...> &meta) {
#if OMNI_FN_LOOKUP_INDEXED_ENTRIES
  return detail::lookup_table_from_tuple(meta,
    compat::make_index_sequence<sizeof...(Meta)>{});
#else
  return {/*entries=*/meta};
#endif
}

} // namespace fn
} // namespace omni

#undef OMNI_FN_LOOKUP_INDEXED_ENTRIES

// TODO: [medium] Investigate slower small-table lookup on shuffled queries
// versus literal metadata-name comparisons; see
// extras/benchmarks/lookup_table/main.cpp. Preserve ordered-query performance.
