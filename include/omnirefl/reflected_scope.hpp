#pragma once

// todo: copiright notice (MIT)

#include <tuple>
#include <type_traits>
#include <utility>

// todo: this header should only utilities available within a reflected scope
namespace omni {

namespace detail {
namespace {

template <std::size_t...>
struct index_sequence {};

template <class T, T I, T N, T... integers>
struct _make_integer_sequence {
  using type =
    typename _make_integer_sequence<T, I + 1, N, integers..., I>::type;
};

template <class T, T N, T... integers>
struct _make_integer_sequence<T, N, N, integers...> {
  using type = std::integer_sequence<T, integers...>;
};

template <std::size_t N>
using make_index_sequence = _make_integer_sequence<std::size_t, 0, N>::type;

// todo: move to `compat.hpp`?
template <typename Visit, typename Tuple, std::size_t... I>
constexpr auto apply(Visit &&v, Tuple &&t, index_sequence<I...>)
  -> decltype(std::forward<Visit>(v)(std::get<I>(std::forward<Tuple>(t))...)) {
  return std::forward<Visit>(v)(std::get<I>(std::forward<Tuple>(t))...);
}

template <typename Visit, typename Tuple>
constexpr auto apply(Visit &&v, Tuple &&t)
  -> decltype(apply(std::forward<Visit>(v),
    std::forward<Tuple>(t),
    make_index_sequence<
      std::tuple_size<typename std::decay<Tuple>::type>::value>{})) {
  return apply(std::forward<Visit>(v),
    std::forward<Tuple>(t),
    make_index_sequence<
      std::tuple_size<typename std::decay<Tuple>::type>::value>{});
}

template <typename... Ts>
struct make_void {
  typedef void type;
};

template <typename... Ts>
using void_t = typename make_void<Ts...>::type;

// refactorme: this is a very confusing and ugle shite
#ifdef OMNI_REFLECTED_INDEXED_CALLS
// Used in header-mode to generate specializatioin for indexed types (local
// structs, unnamed)
template <int>
struct _indexed_reflected;

// fixme:
//   this will not work for non-forward-declarable 'dependent' types (if I
//   decide to index them), because as of this writing only the `reflected_call`
//   is allowed to invoke the `unique_id`, and recursive `reflected_call`s are
//   not allowed
//
// Instantiations by named non-local types will be caught by the generated
// partial specializations of `_reflected<T>` like for source-mode. This will
// prevent from calling `unique_id<T>()` and not increment the counter.
//
// For unnamed and/or local types, the default specialization will be selected,
// using generated `_indexed_reflected<N>` specialization.
template <typename T, typename = T>
struct _reflected: _indexed_reflected<unique_id<T>()> {};

// `std::true_type` specializations will be generated for reflected types to be
// picked up by SFINAE
template <int Index>
struct _is_indexed_reflected: std::false_type {};

// Specializations for forward-declarable types will generated to be picked up
// by SFINAE.
template <typename T, typename = T>
struct _is_reflected: _is_indexed_reflected<unique_id<T>()> {};

#else
// Specializations, containing reflection interface for T will be generated.
// Default argument is used to delay template instantiaton by partial
// specialization.
template <typename T, typename = T>
struct _reflected;

template <typename T, typename = T>
struct _is_reflected;
#endif // OMNI_REFLECTED_INDEXED_CALLS

} // namespace
} // namespace detail

/// todo: use the tool's pass to detect the invalid usage
/// do not add specializations
/// (!!!) do not instantiate this outside of a reflected scope
template <typename T, typename = void>
struct is_reflected;

enum class reflected_entity {
  tagged, // struct | class | union
  member, // tagged.field
  enumeration, // enum | enum class
};

// todo: what about field pointer? `reflected_field<T::name>` is a valid
// use-case, to get a string "name" for example

// todo: typename Meta is a weak point
template <typename Tagged, typename Meta>
struct reflected_mem_binding {
  Tagged &bound;

  // type of Tagged::member
  using type = decltype(Meta::value(std::declval<Tagged>()));

  constexpr const type &value() noexcept {
    return Meta::value(bound);
  }

  constexpr operator const type &() noexcept {
    return value();
  }

  template <typename V>
  constexpr const type &set_value(V &&v) {
    return Meta::set_value(std::forward<V>(v));
  }
};

// reflection for struct | class | union types
template <typename T>
struct reflected_tagged final:
    detail::_reflected<typename std::decay<T>::type> {
  static_assert(is_reflected<T>::value,
    "Type was not reflected (Calling outside a reflected scope?)");

  using meta = detail::_reflected<typename std::decay<T>::type>;
  static_assert(reflected_entity::tagged == meta::entity(),
    "Inconcistent reflection");

  using meta::type; //< yields T
  using meta::name; //< static constexpr const char(&[N]) name()`
  using meta::fields; //< yields std::tuple of fields' meta types

  static constexpr auto fields(const T &t) noexcept {
    return _fields(t);
  }

  static constexpr auto fields(T &t) noexcept {
    return _fields(t);
  }

  private:
  template <typename _T>
  struct _bind_fields_metadata {
    _T &t;
    template <typename... Meta>
    constexpr auto operator()(Meta... field_meta) const noexcept {
      return std::make_tuple(
        reflected_mem_binding<_T, decltype(field_meta)>{t}...);
    }
  };

  template <typename _T>
  static constexpr auto _fields(_T &t) noexcept {
    return detail::apply(_bind_fields_metadata<_T>{t}, meta::fields());
  }
};

template <typename T>
struct reflected_enum final: detail::_reflected<typename std::decay<T>::type> {
  static_assert(std::is_enum<T>::value, "Type is not a enum");
  static_assert(is_reflected<T>::value,
    "Type was not reflected (Calling outside a reflected scope?)");

  using meta = detail::_reflected<typename std::decay<T>::type>;
  static_assert(reflected_entity::enumeration == meta::entity(),
    "Inconcistent reflection");

  using meta::type; //< yields T
  using meta::name; //< static constexpr const char(&[N]) name()`
  using meta::enumerators; //< yields std::array of pair<type, const char*>
};

template <typename T>
using reflected_t = typename std::conditional<reflected_entity::tagged
    == detail::_reflected<T>::entity(),
  reflected_tagged<T>,
  reflected_enum<T>>::type;

// todo: comment about why 'set_value' is even needed:
// C++ doesn't allow references or pointers to bit fields. But setting them via
// a 'visitor' is ok.

// C++11 adapter, because generic lambdas (with `auto` arg) are only available
// since C++14
template <typename T>
struct set_value {
  virtual void operator()(T &&) const = 0;
  virtual void operator()(const T &) const = 0;

  protected:
  ~set_value() = default;
};

template <typename Ref,
  typename Field,
  typename ValueT =
    typename std::decay<decltype(Field::get_value(std::declval<Ref>()))>::type>
struct _set_value final: set_value<ValueT> {
  // todo: static_assert Field belongs to Ref

  // somewhere such error should be reported in a clear manner
  // todo: static_assert(std::is_const<Ref>() && !Field::is_mutable, "")
  Ref &_r;

  // note: this is necessary because of inheritance
  constexpr _set_value(Ref &r): _r(r) {
  }

  template <typename V>
  void operator()(V &&v) const {
    Field::set_value(_r, std::forward<V>(v));
  }

  void operator()(ValueT &&v) const override {
    return Field::set_value(_r, std::move(v));
  }

  void operator()(const ValueT &v) const override {
    return Field::set_value(_r, v);
  }
};

} // namespace omni
