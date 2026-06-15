#pragma once

// todo: copiright notice (MIT)

// This header contains utilities only available within a reflected scope.

#include <tuple>
#include <type_traits>
#include <utility>

#include <omnirefl/compat.hpp>

namespace omni {

namespace detail {
namespace {

//------------------------------------------------------------------------------
// Notes for generated-header reflection: local/unnamed type support
//
// During the real compilation, `reflected_call` registers local/unnamed types
// observed by the tool. Generated `_reflected<T>` queries must only inspect
// those registrations: probing an unrelated type must not mutate reflection
// state observed by later reflected calls.
//
// Limitation: if a reflected type `T` has member field types that are not
// forward-declarable, those member types are not available for reflection.
//------------------------------------------------------------------------------

// `= T` is kept for frontend compatibility with generated metadata.
template <typename T, typename = T>
struct _reflected;

template <typename T>
using _meta = detail::_reflected<compat::decay_t<T>>;

template <typename Meta, typename = typename Meta::public_bases_t>
struct _all_public_fields;

template <typename Meta, typename... Bases>
struct _all_public_fields<Meta, std::tuple<Bases...>> {
  using type = decltype(std::tuple_cat(
    std::declval<typename _all_public_fields<_meta<Bases>>::type>()...,
    std::declval<typename Meta::own_public_fields_t>()));
};

template <typename Meta>
using _all_public_fields_t = typename _all_public_fields<Meta>::type;

} // namespace
} // namespace detail

enum class reflected_entity {
  record, // struct | class | union
  enumeration, // enum | enum class
};

// todo: what about field pointer? `reflected_field<T::name>` is a valid
// use-case, to get a string "name" for example

/// todo: use the tool's pass to detect the invalid usage
/// do not add specializations
/// (!!!) do not instantiate this outside of a reflected scope
///
/// note: T is decayed by generated specialization
template <typename T, typename = void>
struct is_reflected;

template <typename T>
struct type_t {};

#if OMNI_CPLUSPLUS >= 201402L
template <typename T>
constexpr type_t<T> type{};
#endif

#if defined(OMNI_TOOL_RUN)
template <typename T, reflected_entity = reflected_entity::record>
struct meta_t;

template <typename T, reflected_entity = reflected_entity::record>
struct binding_t;
#else
template <typename T, reflected_entity = detail::_meta<T>::entity()>
struct meta_t;

template <typename T,
  reflected_entity = detail::_meta<compat::decay_t<T>>::entity()>
struct binding_t;
#endif

template <typename Owner, typename FieldMeta>
struct field_meta_t;

template <typename Record, typename FieldMeta>
struct field_binding_t;

#if OMNI_CPLUSPLUS >= 202002L
template <typename T>
concept meta = requires {
  typename compat::remove_cvref_t<T>::omni_meta_tag;
};

template <typename T>
concept binding = requires {
  typename compat::remove_cvref_t<T>::omni_binding_tag;
};

template <typename T>
concept field_meta = requires {
  typename compat::remove_cvref_t<T>::omni_field_meta_tag;
};

template <typename T>
concept field_binding = requires {
  typename compat::remove_cvref_t<T>::omni_field_binding_tag;
};
#endif

struct reflected_call_t {
  private:
#if defined(OMNI_TOOL_RUN)
  template <typename T>
  static constexpr meta_t<T> _tool_arg(type_t<T>) noexcept;

  template <typename T,
    typename Binding = compat::conditional_t<std::is_lvalue_reference<T>::value,
      T,
      compat::decay_t<T>>>
  static constexpr binding_t<Binding> _tool_arg(T &&) noexcept;
#endif

  template <typename T>
  static constexpr meta_t<T> _reflect_arg(type_t<T>) noexcept {
    return {};
  }

  template <typename T,
    typename Binding = compat::conditional_t<std::is_lvalue_reference<T>::value,
      T,
      compat::decay_t<T>>>
  static constexpr binding_t<Binding> _reflect_arg(T &&t) noexcept(
    noexcept(binding_t<Binding>{std::forward<T>(t)})) {
    return binding_t<Binding>{std::forward<T>(t)};
  }

  public:
  template <typename Impl, typename... Args>
  auto operator()(Impl &&impl, Args &&...args) const
#if defined(OMNI_TOOL_RUN)
    -> decltype(std::declval<Impl &&>()(
      _tool_arg(std::declval<Args &&>())...));
#else
    -> decltype(std::declval<Impl &&>()(
      _reflect_arg(std::declval<Args &&>())...));
#endif
};

template <typename Owner, typename FieldMeta>
struct field_meta_t {
  using omni_field_meta_tag = void;
  using owner_type = compat::decay_t<Owner>;
  using reflected = FieldMeta;
  using type =
    compat::remove_cvref_t<decltype(reflected::value(std::declval<Owner &>()))>;

  static constexpr const char *name() noexcept {
    return reflected::name();
  }

  static constexpr const char *type_name() noexcept {
    return reflected::type_name();
  }

  static constexpr const char *annotation() noexcept {
    return reflected::annotation();
  }

  static constexpr std::size_t index() noexcept {
    return reflected::index();
  }

  template <typename T>
  static constexpr auto value(T &&t) noexcept
    -> decltype(reflected::value(std::forward<T>(t))) {
    return reflected::value(std::forward<T>(t));
  }

  template <typename T, typename V>
  static void set_value(T &&t, V &&v) {
    reflected::set_value(std::forward<T>(t), std::forward<V>(v));
  }
};

template <typename Record, typename FieldMeta>
struct field_binding_t {
  using omni_field_binding_tag = void;
  using type = typename FieldMeta::type;
  using meta = FieldMeta;

  Record &owner;

  static constexpr const char *name() noexcept {
    return meta::name();
  }

  static constexpr const char *type_name() noexcept {
    return meta::type_name();
  }

  static constexpr const char *annotation() noexcept {
    return meta::annotation();
  }

  static constexpr std::size_t index() noexcept {
    return meta::index();
  }

  constexpr auto value() const noexcept -> decltype(meta::value(owner)) {
    return meta::value(owner);
  }

  constexpr operator const type &() const noexcept {
    return value();
  }

  constexpr auto operator*() const noexcept -> decltype(value()) {
    return value();
  }

  // todo: enable_if is_mutable
  template <typename V>
  void set_value(V &&v) {
    meta::set_value(owner, std::forward<V>(v));
  }

  constexpr explicit field_binding_t(Record &owner): owner(owner) {}
};

#if defined(OMNI_TOOL_RUN)
template <typename T, reflected_entity Entity>
struct meta_t {
  using omni_meta_tag = void;
  using type = compat::decay_t<T>;
};

template <typename T, reflected_entity Entity>
struct binding_t {
  using omni_binding_tag = void;
  using type = compat::decay_t<T>;
};

#else
template <typename T>
struct meta_t<T, reflected_entity::record> {
  using omni_meta_tag = void;
  using reflected = detail::_meta<T>;
  using type = typename reflected::type;

  using public_fields_t =
    detail::_all_public_fields_t<reflected>; //< yields std::tuple<...>

  private:
  friend struct reflected_call_t;

  template <typename U>
  friend constexpr meta_t<compat::decay_t<U>> reflected() noexcept;

  template <typename, reflected_entity>
  friend struct binding_t;

  constexpr meta_t() noexcept = default;

  // C++11 ad hoc. `auto` lambda arguments only since C++14
  template <typename _T>
  struct _bind_fields_metadata {
    _T &t;

    template <typename... FieldMeta>
    constexpr auto operator()(FieldMeta...) const noexcept
      -> std::tuple<field_binding_t<_T, FieldMeta>...> {
      return std::make_tuple(field_binding_t<_T, FieldMeta>{t}...);
    }
  };

  template <typename _T>
  static constexpr auto _public_fields(_T &t) noexcept
    -> decltype(compat::apply(_bind_fields_metadata<_T>{t},
      public_fields_t{})) {
    return compat::apply(_bind_fields_metadata<_T>{t}, public_fields_t{});
  }

  public:
  static constexpr const char *name() noexcept {
    return reflected::name();
  }

  static constexpr const char *annotation() noexcept {
    return reflected::annotation();
  }

  static constexpr reflected_entity entity() noexcept {
    return reflected::entity();
  }

  static constexpr public_fields_t public_fields() noexcept {
    return {};
  }

  template <typename U,
    typename Binding = compat::conditional_t<std::is_lvalue_reference<U &&>::value,
      U &&,
      compat::decay_t<U>>>
  static constexpr auto bind(U &&t) noexcept(
    noexcept(binding_t<Binding>{std::forward<U>(t)}))
    -> decltype(binding_t<Binding>{std::forward<U>(t)}) {
    return binding_t<Binding>{std::forward<U>(t)};
  }

  template <typename U = type,
    typename std::enable_if<std::is_default_constructible<U>::value,
      int>::type = 0>
  static constexpr binding_t<type> bind() noexcept(
    std::is_nothrow_default_constructible<type>::value) {
    return binding_t<type>{type{}};
  }

  // todo: mutable_public_fields(T &t) and mutable_public_fields(const T &t)
};

template <typename T>
struct meta_t<T, reflected_entity::enumeration> {
  using omni_meta_tag = void;
  using reflected = detail::_meta<T>;
  using type = typename reflected::type;

  static_assert(std::is_enum<type>::value, "Type is not a enum");
  static_assert(is_reflected<type>::value,
    "Type was not reflected (Calling outside a reflected scope?)");

  static_assert(reflected_entity::enumeration == reflected::entity(),
    "Inconcistent reflection");

  private:
  friend struct reflected_call_t;

  template <typename U>
  friend constexpr meta_t<compat::decay_t<U>> reflected() noexcept;

  constexpr meta_t() noexcept = default;

  public:
  static constexpr const char *name() noexcept {
    return reflected::name();
  }

  static constexpr const char *annotation() noexcept {
    return reflected::annotation();
  }

  static constexpr reflected_entity entity() noexcept {
    return reflected::entity();
  }

  // yields std::array of pair<type, const char*>
  static constexpr auto enumerators() noexcept
    -> decltype(reflected::enumerators()) {
    return reflected::enumerators();
  }
};

template <typename T>
struct binding_t<T, reflected_entity::record> {
  using omni_binding_tag = void;
  using type = compat::decay_t<T>;
  using reflected = typename meta_t<type>::reflected;

  using owning = typename std::conditional<std::is_lvalue_reference<T>::value,
    std::false_type,
    std::true_type>::type;

  using storage_t = typename std::conditional<owning::value,
    type, //< own a value
    T //< hold a reference (T is U& / const U&)
    >::type;

  storage_t _value;

  static constexpr const char *name() noexcept {
    return reflected::name();
  }

  static constexpr const char *annotation() noexcept {
    return reflected::annotation();
  }

  constexpr operator const type &() const {
    return _value;
  }

  // fixme: C++11 doesn't support constexpr here
  auto public_fields() & -> decltype(meta_t<type>::_public_fields(
    std::declval<storage_t &>())) {
    return meta_t<type>::_public_fields(_value);
  }

  constexpr auto public_fields() const &
    -> decltype(meta_t<type>::_public_fields(
      std::declval<const storage_t &>())) {
    return meta_t<type>::_public_fields(_value);
  }

  auto public_fields() && -> decltype(meta_t<type>::_public_fields(
    std::declval<storage_t &>())) = delete;

  auto public_fields() const && -> decltype(meta_t<type>::_public_fields(
    std::declval<const storage_t &>())) = delete;

  private:
  friend struct reflected_call_t;
  friend struct meta_t<type>;

  // non-owning: T is an lvalue reference (U& / const U&)
  template <typename U,
    typename std::enable_if<!owning::value
        && std::is_convertible<U &, T>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &u) noexcept: _value(u) {}

  // owning: T is a value type (U / const U)
  template <typename U,
    typename std::enable_if<owning::value
        && std::is_constructible<type, U &&>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept(
    std::is_nothrow_move_constructible<type>::value)
      : _value(std::forward<U>(u)) {}
};

template <typename T>
struct binding_t<T, reflected_entity::enumeration> {
  using omni_binding_tag = void;
  using reflected = typename meta_t<T>::reflected;
  using type = typename reflected::type;

  using owning = typename std::conditional<std::is_lvalue_reference<T>::value,
    std::false_type,
    std::true_type>::type;

  using storage_t = typename std::conditional<owning::value,
    type, //< own a value
    T //< hold a reference (T is U& / const U&)
    >::type;

  storage_t _value;

  static constexpr const char *name() noexcept {
    return reflected::name();
  }

  static constexpr const char *annotation() noexcept {
    return reflected::annotation();
  }

  operator const type &() const {
    return _value;
  }

  static constexpr auto enumerators() -> decltype(reflected::enumerators()) {
    return reflected::enumerators();
  }

  private:
  friend struct reflected_call_t;

  // non-owning: T is an lvalue reference (U& / const U&)
  template <typename U,
    typename std::enable_if<!owning::value
        && std::is_convertible<U &, T>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &u) noexcept: _value(u) {}

  // owning: T is a value type (U / const U)
  template <typename U,
    typename std::enable_if<owning::value
        && std::is_constructible<type, U &&>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept(
    std::is_nothrow_move_constructible<type>::value)
      : _value(std::forward<U>(u)) {}
};
#endif

template <typename T>
constexpr meta_t<compat::decay_t<T>> reflected() noexcept {
  return {};
}

template <typename T>
constexpr auto reflected(T &&t) noexcept(
  noexcept(meta_t<compat::decay_t<T>>::bind(std::forward<T>(t))))
  -> decltype(meta_t<compat::decay_t<T>>::bind(std::forward<T>(t))) {
  return meta_t<compat::decay_t<T>>::bind(std::forward<T>(t));
}

} // namespace omni
