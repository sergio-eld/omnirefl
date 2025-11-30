#pragma once

// todo: copiright notice (MIT)

#include <tuple>
#include <type_traits>
#include <utility>

#if defined(__cpp_lib_variant) && __cpp_lib_variant >= 201606
#  define OMNI_HAS_STD_VARIANT

#  include <variant>
#endif

// todo: this header should only utilities available within a reflected scope
namespace omni {

namespace customization {

template <template <typename...> class>
struct visit;

#if defined(__cpp_lib_variant) && __cpp_lib_variant >= 201606

template <>
struct visit<std::variant> {
  template <typename Visitor, typename... Variants>
  decltype(auto) operator()(Visitor &&vis, Variants &&...vars) const {
    return std::visit(std::forward<Visitor>(vis),
      std::forward<Variants>(vars)...);
  }
};

#endif // __cpp_lib_variant

} // namespace customization

namespace detail {
namespace {

template <template <typename...> class Variant, typename... T>
struct _tuple_to_array {
  template <typename... U>
  constexpr std::array<Variant<T...>, sizeof...(T)> operator()(U &&...u) const {
    return {{Variant<T...>{std::forward<U>(u)}...}};
  }
};

template <std::size_t...>
struct index_sequence {};

template <class T, T I, T N, T... integers>
struct _make_integer_sequence {
  using type =
    typename _make_integer_sequence<T, I + 1, N, integers..., I>::type;
};

template <class T, T N, T... integers>
struct _make_integer_sequence<T, N, N, integers...> {
  using type = index_sequence<integers...>;
};

template <std::size_t N>
using make_index_sequence =
  typename _make_integer_sequence<std::size_t, 0, N>::type;

// drop-in apply for C++11 (tuple + index_sequence)
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
///
/// note: T is decayed by generated specialization
template <typename T, typename = void>
struct is_reflected;

enum class reflected_entity {
  tagged, // struct | class | union
  member, // tagged.field
  enumeration, // enum | enum class
};

template <template <typename> class, typename>
struct reflected_binding;

// todo: what about field pointer? `reflected_field<T::name>` is a valid
// use-case, to get a string "name" for example

// todo: typename Meta is a weak point
template <typename Tagged, typename Meta>
struct reflected_mem_binding {
  Tagged &bound;

  // type of Tagged::member
  using type = decltype(Meta::value(std::declval<Tagged>()));

  constexpr const type &value() const noexcept {
    return Meta::value(bound);
  }

  constexpr operator const type &() const noexcept {
    return value();
  }

  // todo: if field is a pointer, need to return by value
  // todo: enable_if is_mutable
  template <typename V>
  constexpr const type &set_value(V &&v) {
    return Meta::set_value(bound, std::forward<V>(v));
  }

  constexpr explicit reflected_mem_binding(Tagged &b): bound(b) {}
};

// reflection for struct | class | union types
template <typename T>
struct reflected_tagged_t final:
    detail::_reflected<typename std::decay<T>::type> {
  static_assert(is_reflected<T>::value,
    "Type was not reflected (Calling outside a reflected scope?)");

  using meta = detail::_reflected<typename std::decay<T>::type>;
  static_assert(reflected_entity::tagged == meta::entity(),
    "Inconcistent reflection");

  using typename meta::type; //< yields T
  using typename meta::fields_t; //< yields std::tuple<...>

  using meta::name; //< static constexpr const char(&[N]) name()`
  using meta::fields; //< yields std::tuple of fields' meta types

  private:
  template <typename _T>
  struct _bind_fields_metadata {
    _T &t;

    // fixme: auto without trailing return is C++14
    template <typename... Meta>
    constexpr auto operator()(Meta... field_meta) const noexcept {
      return std::make_tuple(
        reflected_mem_binding<_T, decltype(field_meta)>{t}...);
    }
  };

  // fixme: auto without trailing return type is C++14
  template <typename _T>
  static constexpr auto _fields(_T &t) noexcept {
    return detail::apply(_bind_fields_metadata<_T>{t}, meta::fields());
  }

  public:
  // fixme: auto without trailing return is C++14
  static constexpr auto fields(const type &t) noexcept {
    return _fields(t);
  }

  // fixme: auto without trailing return is C++14
  static constexpr auto fields(type &t) noexcept {
    return _fields(t);
  }

  // todo: mutable_fields(T &t) and mutable_fields(const T &t)
};

template <typename T>
struct reflected_enum_t final:
    detail::_reflected<typename std::decay<T>::type> {
  using meta = detail::_reflected<typename std::decay<T>::type>;
  static_assert(is_reflected<T>::value,
    "Type was not reflected (Calling outside a reflected scope?)");
  static_assert(std::is_enum<typename meta::type>::value, "Type is not a enum");

  static_assert(reflected_entity::enumeration == meta::entity(),
    "Inconcistent reflection");

  using typename meta::type; //< yields T
  using meta::name; //< static constexpr const char(&[N]) name()`
  using meta::enumerators; //< yields std::array of pair<type, const char*>
};

template <typename T>
using reflected_t = typename std::conditional<reflected_entity::tagged
    == detail::_reflected<typename std::decay<T>::type>::entity(),
  reflected_tagged_t<T>,
  reflected_enum_t<T>>::type;

template <typename T>
struct reflected_binding<reflected_tagged_t, T>:
    private detail::_reflected<typename std::decay<T>::type> {
  using meta = typename reflected_tagged_t<T>::meta;

  using typename meta::type; //< unqualified type
  using meta::name; //< static constexpr const char(&[N]) name()`
  using meta::fields; //< yields std::tuple of fields' meta types

  using owning = typename std::conditional<std::is_lvalue_reference<T>::value,
    std::false_type,
    std::true_type>::type;

  using storage_t = typename std::conditional<owning::value,
    type, //< own a value
    T //< hold a reference (T is U& / const U&)
    >::type;

  storage_t bound;

  operator const type &() const {
    return bound;
  }

  constexpr auto fields() const {
    return reflected_tagged_t<type>::fields(bound);
  }

  // non-owning: T is an lvalue reference (U& / const U&)
  template <typename U,
    typename std::enable_if<!owning::value
        && std::is_convertible<U &, T>::value,
      int>::type = 0>
  constexpr explicit reflected_binding(U &u) noexcept: bound(u) {}

  // owning: T is a value type (U / const U)
  template <typename U,
    typename std::enable_if<owning::value
        && std::is_constructible<type, U &&>::value,
      int>::type = 0>
  constexpr explicit reflected_binding(U &&u) noexcept(
    std::is_nothrow_move_constructible<type>::value)
      : bound(std::forward<U>(u)) {}
};

template <typename T>
struct reflected_binding<reflected_enum_t, T>:
    private detail::_reflected<typename std::decay<T>::type> {
  using meta = typename reflected_enum_t<T>::meta;

  using typename meta::type; //< unqualified enum type
  using meta::name; //< static constexpr const char(&)[N] name()
  using meta::enumerators; //< yields std::array of pair<type, const char*>

  using owning = typename std::conditional<std::is_lvalue_reference<T>::value,
    std::false_type,
    std::true_type>::type;

  using storage_t = typename std::conditional<owning::value,
    type, //< own a value
    T //< hold a reference (T is U& / const U&)
    >::type;

  storage_t bound;

  operator const type &() const {
    return bound;
  }

  constexpr auto enumerators() const {
    return meta::enumerators();
  }

  // non-owning: T is an lvalue reference (U& / const U&)
  template <typename U,
    typename std::enable_if<!owning::value
        && std::is_convertible<U &, T>::value,
      int>::type = 0>
  constexpr explicit reflected_binding(U &u) noexcept: bound(u) {}

  // owning: T is a value type (U / const U)
  template <typename U,
    typename std::enable_if<owning::value
        && std::is_constructible<type, U &&>::value,
      int>::type = 0>
  constexpr explicit reflected_binding(U &&u) noexcept(
    std::is_nothrow_move_constructible<type>::value)
      : bound(std::forward<U>(u)) {}
};

template <typename T>
constexpr auto reflected_tagged() -> reflected_tagged_t<T> {
  return {};
}

template <typename T>
constexpr auto reflected_enum() -> reflected_enum_t<T> {
  return {};
}

template <typename T>
constexpr auto reflected() -> reflected_t<T> {
  return {};
}

// ------------ lvalue + rvalue bindings ------------

// tagged: lvalues → binding on T&
template <typename T>
constexpr auto reflected_tagged(T &t)
  -> reflected_binding<reflected_tagged_t, T &> {
  return reflected_binding<reflected_tagged_t, T &>{t};
}

// tagged: prvalues/xvalues → owning binding on decayed T
template <typename T>
constexpr auto reflected_tagged(T &&t) ->
  typename std::enable_if<!std::is_lvalue_reference<T>::value,
    reflected_binding<reflected_tagged_t, typename std::decay<T>::type>>::type {
  using U = typename std::decay<T>::type;
  return reflected_binding<reflected_tagged_t, U>{std::forward<T>(t)};
}

// enum: lvalues
template <typename T>
constexpr auto reflected_enum(T &t)
  -> reflected_binding<reflected_enum_t, T &> {
  return reflected_binding<reflected_enum_t, T &>{t};
}

// enum: prvalues/xvalues
template <typename T>
constexpr auto reflected_enum(T &&t) ->
  typename std::enable_if<!std::is_lvalue_reference<T>::value,
    reflected_binding<reflected_enum_t, typename std::decay<T>::type>>::type {
  using U = typename std::decay<T>::type;
  return reflected_binding<reflected_enum_t, U>{std::forward<T>(t)};
}

// polymorphic: lvalues
template <typename T>
constexpr auto reflected(T &t) ->
  typename std::conditional<reflected_entity::tagged
      == reflected_t<T>::entity(),
    reflected_binding<reflected_tagged_t, T &>,
    reflected_binding<reflected_enum_t, T &>>::type {
  return typename std::conditional<reflected_entity::tagged
      == reflected_t<T>::entity(),
    reflected_binding<reflected_tagged_t, T &>,
    reflected_binding<reflected_enum_t, T &>>::type(t);
}

// polymorphic: prvalues/xvalues
template <typename T>
constexpr auto reflected(
  T &&t) -> typename std::enable_if<!std::is_lvalue_reference<T>::value,
  typename std::conditional<reflected_entity::tagged
      == reflected_t<T>::entity(),
    reflected_binding<reflected_tagged_t, typename std::decay<T>::type>,
    reflected_binding<reflected_enum_t, typename std::decay<T>::type>>::type>::
  type {
  using U = typename std::decay<T>::type;

  return typename std::conditional<reflected_entity::tagged
      == reflected_t<U>::entity(),
    reflected_binding<reflected_tagged_t, U>,
    reflected_binding<reflected_enum_t, U>>::type(std::forward<T>(t));
}

// -- utility -------------------

template <template <typename...> class Variant, typename... T>
constexpr std::array<Variant<T...>, sizeof...(T)> tuple_to_array(
  std::tuple<T...> t) {
  return detail::apply(detail::_tuple_to_array<Variant, T...>{}, std::move(t));
}

#ifdef OMNI_HAS_STD_VARIANT

template <typename... T>
constexpr std::array<std::variant<T...>, sizeof...(T)> tuple_to_array(
  std::tuple<T...> t) {
  return std::apply(
    [](auto &&...elems) {
      return std::array<std::variant<T...>, sizeof...(T)>{
        std::variant<T...>{std::forward<decltype(elems)>(elems)}...};
    },
    std::move(t));
}

#endif

struct type_info_t {
  const char *name; //< refactorme: I need std::string_view-like type for this

  // todo: namespaces?
};

// convenience adapter to get type info from Variant of field bindings. Example:
// for (auto f : omni::reflected(t).fields()) {
// std::cout << omni::type_info(f).name << '\n'; //< Polymorphic access without
// calling std::visit
// }
template <template <typename...> class Variant, typename... T>
constexpr type_info_t type_info(const Variant<T...> &t) {
  // todo: constraints on T: it might be a field's generated Meta, or a
  // reflected Binding. However, it shouldn't matter, since both of them define
  // `meta::name()`
  return customization::visit<Variant>{}(
    [](const auto &t) -> type_info_t {
      return {
        t.name(),
      };
    },
    t);
}

} // namespace omni
