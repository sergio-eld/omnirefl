#pragma once

// todo: copiright notice (MIT)

// This header contains utilities only available within a reflected scope.

#include <tuple>
#include <type_traits>
#include <utility>

#if defined(__cpp_lib_variant) && __cpp_lib_variant >= 201606
#  define OMNI_HAS_STD_VARIANT

#  include <variant>
#endif

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

} // namespace
} // namespace detail

namespace compat {

#if defined(__cpp_lib_apply) && __cpp_lib_apply >= 201603L
using std::apply;
#else
using detail::apply;
#endif

#if defined(__cpp_lib_void_t) && __cpp_lib_void_t >= 201411L
using std::void_t;
#else
using detail::void_t;
#endif

#if defined(__cpp_lib_transformation_trait_aliases) \
  && __cpp_lib_transformation_trait_aliases >= 201304
using std::conditional_t;
#else
template <bool B, class T, class F>
using conditional_t = typename std::conditional<B, T, F>::type;
#endif

#if defined(__cpp_lib_transformation_trait_aliases) \
  && __cpp_lib_transformation_trait_aliases >= 201304
using std::decay_t;
#else
template <typename T>
using decay_t = typename std::decay<T>::type;
#endif

#if defined(__cpp_lib_logical_traits) && __cpp_lib_logical_traits >= 201510L
using std::disjunction;
#else
template <class...>
struct disjunction: std::false_type {};

template <class B1>
struct disjunction<B1>: B1 {};

template <class B1, class... Bn>
struct disjunction<B1, Bn...>:
    std::conditional<bool(B1::value), B1, disjunction<Bn...>>::type {};
#endif

#if defined(__cpp_lib_remove_cvref) && __cpp_lib_remove_cvref >= 201711L
using std::remove_cvref;
using std::remove_cvref_t;
#else
template <class T>
struct remove_cvref {
  using type =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};

template <class T>
using remove_cvref_t = typename remove_cvref<T>::type;
#endif

} // namespace compat

namespace detail {
namespace {

template <template <typename...> class Variant, typename... T>
struct _tuple_to_array {
  template <typename... U>
  constexpr std::array<Variant<T...>, sizeof...(T)> operator()(U &&...u) const {
    return {{Variant<T...>{std::forward<U>(u)}...}};
  }
};

#ifdef OMNI_REFLECTED_INDEXED_CALLS
//------------------------------------------------------------------------------
// Indexed header-mode: local/unnamed type support
//
// Omnirefl records the integer index `N` that `unique_id<T>()` evaluates to
// while parsing the AST. During the real compilation, `unique_id<T>()` yields
// the same `N`, so `_indexed_reflected<unique_id<T>()>` selects the matching
// generated `_indexed_reflected<N>` specialization directly.
//
// Limitation: if a reflected type `T` has member field types that are not
// forward-declarable, those member types cannot be indexed and therefore will
// not be available for reflection.
//------------------------------------------------------------------------------

// `N` is the index value observed for `unique_id<T>()` during the AST pre-run.
template <int N>
struct _indexed_reflected;

// Routes local/unnamed types through the indexed path.
template <typename T, typename = T>
struct _reflected: _indexed_reflected<unique_id<T>()> {};
#else
//------------------------------------------------------------------------------
// Source-mode frontend declaration
//
// In source-mode, the tool emits specializations of `_reflected<T>` (and
// related traits) into the generated `.cpp`.
//------------------------------------------------------------------------------

// `= T` is kept for frontend compatibility with header-mode usage.
template <typename T, typename = T>
struct _reflected;
#endif // OMNI_REFLECTED_INDEXED_CALLS

} // namespace
} // namespace detail

enum class reflected_entity {
  tagged, // struct | class | union
  member, // tagged.field
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

template <typename T,
  typename Meta = detail::_reflected<compat::decay_t<T>>,
  reflected_entity = Meta::entity()>
struct reflected_t;

// struct | class | union
template <typename T>
using reflected_tagged_t = reflected_t<T,
  detail::_reflected<compat::decay_t<T>>,
  reflected_entity::tagged>;

template <typename T>
using reflected_enum_t = reflected_t<T,
  detail::_reflected<compat::decay_t<T>>,
  reflected_entity::enumeration>;

template <typename T, reflected_entity = reflected_t<T>::entity()>
struct reflected_binding;

template <typename T>
struct mem_binding {
  using value_type = T;

  const T &value() const noexcept {
    return _vtable->value(_owner);
  }

  operator const T &() const noexcept {
    return value();
  }
  const T &operator*() const noexcept {
    return value();
  }

  const char *name() const noexcept {
    return _vtable->name();
  }
  std::size_t index() const noexcept {
    return _vtable->index();
  }

  protected:
  // can point to const or non-const Owner
  const void *_owner = nullptr;

  template <typename Owner, typename Meta>
  mem_binding(Owner *owner, Meta) noexcept
      : _owner(owner)
      , _vtable(get_vtable<Owner, Meta>()) {}

  private:
  struct vtable {
    virtual const T &value(const void *owner) const noexcept = 0;
    virtual const char *name() const noexcept = 0;
    virtual std::size_t index() const noexcept = 0;
    virtual ~vtable() {}
  };

  template <typename Owner, typename Meta>
  static const vtable *get_vtable() noexcept {
    struct impl_t final: vtable {
      const T &value(const void *owner) const noexcept override {
        using COwner = typename std::add_const<Owner>::type;
        return Meta::value(*static_cast<const COwner *>(owner));
      }

      const char *name() const noexcept override {
        return Meta::name();
      }

      std::size_t index() const noexcept override {
        return Meta::index();
      }
    };
    static const impl_t impl{};
    return &impl;
  }

  const vtable *_vtable = nullptr;

  template <typename, typename>
  friend struct reflected_mem_binding;

  template <typename>
  friend struct mutable_mem_binding;
};

template <typename T>
struct mutable_mem_binding: mem_binding<T> {
  using base = mem_binding<T>;
  using value_type = T;

  using base::value;
  using base::operator const T &;
  using base::operator*;
  using base::name;
  using base::index;

  template <typename V>
  void set_value(V &&v) const {
    T tmp = static_cast<T>(std::forward<V>(v));
    _vtable->set_value(const_cast<void *>(this->_owner), std::move(tmp));
  }

  private:
  struct vtable {
    virtual void set_value(void *owner, T v) const = 0;
    virtual ~vtable() {}
  };

  const vtable *_vtable = nullptr;

  template <typename, typename>
  friend struct reflected_mem_binding;

  template <typename Owner, typename Meta>
  mutable_mem_binding(Owner *owner, Meta) noexcept
      : base(owner, Meta{})
      , _vtable(get_vtable<Owner, Meta>()) {
    static_assert(!std::is_const<Owner>::value,
      "mutable_mem_binding cannot bind const Owner");
  }

  template <typename Owner, typename Meta>
  static const vtable *get_vtable() noexcept {
    struct impl_t final: vtable {
      void set_value(void *owner, T v) const override {
        Meta::set_value(*static_cast<Owner *>(owner), std::move(v));
      }
    };
    static const impl_t impl{};
    return &impl;
  }
};

// todo: typename Meta is a weak point
template <typename Tagged, typename Meta>
struct reflected_mem_binding {
  using type =
    compat::remove_cvref_t<decltype(Meta::value(std::declval<Tagged>()))>;

  using meta = Meta;

  Tagged &owner;

  static constexpr const char *name() noexcept {
    return meta::name();
  }

  static constexpr std::size_t index() noexcept {
    return meta::index();
  }

  // todo:
  // if member
  // - is a pointer,
  // - is a bit field (detected by the tool)
  // need to return by value
  constexpr const type &value() const noexcept {
    return Meta::value(owner);
  }

  constexpr operator const type &() const noexcept {
    return value();
  }

  constexpr const type &operator*() const noexcept {
    return value();
  }

  // todo: enable_if is_mutable
  template <typename V>
  void set_value(V &&v) {
    Meta::set_value(owner, std::forward<V>(v));
  }

  // Meta-erased const binding
  operator mem_binding<type>() const noexcept {
    return mem_binding<type>(owner, Meta{});
  }

  // Meta-erased mutable binding
  template <typename U = Tagged>
  typename std::enable_if<!std::is_const<U>::value,
    mutable_mem_binding<type>>::type
    mutable_binding() const noexcept {
    return mutable_mem_binding<type>(owner, Meta{});
  }

  constexpr explicit reflected_mem_binding(Tagged &owner): owner(owner) {}
};

template <typename T>
struct reflected_t<T,
  detail::_reflected<compat::decay_t<T>>,
  reflected_entity::tagged> {
  using type = compat::decay_t<T>; //< reflecting pointers is pointless
  using meta = detail::_reflected<type>;

  static_assert(is_reflected<type>::value,
    "Type was not reflected (Calling outside a reflected scope?)");

  static_assert(reflected_entity::tagged == meta::entity(),
    "Inconcistent reflection");

  private:
  template <typename _T>
  struct _bind_fields_metadata {
    _T &t;

    template <typename... FieldMeta>
    constexpr auto operator()(FieldMeta...) const noexcept
      -> std::tuple<reflected_mem_binding<_T, FieldMeta>...> {
      return std::make_tuple(reflected_mem_binding<_T, FieldMeta>{t}...);
    }
  };

  template <typename _T>
  static constexpr auto _fields(_T &t) noexcept
    -> decltype(compat::apply(_bind_fields_metadata<_T>{t}, meta::fields())) {
    return compat::apply(_bind_fields_metadata<_T>{t}, meta::fields());
  }

  public:
  static constexpr const char *name() noexcept {
    return meta::name();
  }

  static constexpr reflected_entity entity() noexcept {
    return meta::entity();
  }

  using fields_t = typename meta::fields_t; //< yields std::tuple<...>

  static constexpr fields_t fields() noexcept {
    return {};
  }

  static constexpr auto fields(const type &t) noexcept -> decltype(_fields(t)) {
    return _fields(t);
  }

  static constexpr auto fields(type &t) noexcept -> decltype(_fields(t)) {
    return _fields(t);
  }

  // todo: mutable_fields(T &t) and mutable_fields(const T &t)
};

template <typename T>
struct reflected_t<T,
  detail::_reflected<compat::decay_t<T>>,
  reflected_entity::enumeration> {
  using type = compat::decay_t<T>;
  using meta = detail::_reflected<type>;

  static_assert(std::is_enum<type>::value, "Type is not a enum");
  static_assert(is_reflected<type>::value,
    "Type was not reflected (Calling outside a reflected scope?)");

  static_assert(reflected_entity::enumeration == meta::entity(),
    "Inconcistent reflection");

  static constexpr const char *name() noexcept {
    return meta::name();
  }

  static constexpr reflected_entity entity() noexcept {
    return meta::entity();
  }

  // yields std::array of pair<type, const char*>
  static constexpr auto enumerators() noexcept
    -> decltype(meta::enumerators()) {
    return meta::enumerators();
  }
};

template <typename T>
struct reflected_binding<T, reflected_entity::tagged> {
  using type = compat::decay_t<T>;
  using meta = typename reflected_tagged_t<type>::meta;

  using owning = typename std::conditional<std::is_lvalue_reference<T>::value,
    std::false_type,
    std::true_type>::type;

  using storage_t = typename std::conditional<owning::value,
    type, //< own a value
    T //< hold a reference (T is U& / const U&)
    >::type;

  storage_t bound;

  static constexpr const char *name() noexcept {
    return meta::name();
  }

  constexpr operator const type &() const {
    return bound;
  }

  // fixme: C++11 doesn't support constexpr here
  auto fields()
    -> decltype(reflected_tagged_t<type>::fields(std::declval<storage_t &>())) {
    return reflected_tagged_t<type>::fields(bound);
  }

  constexpr auto fields() const -> decltype(reflected_tagged_t<type>::fields(
    std::declval<const storage_t &>())) {
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
struct reflected_binding<T, reflected_entity::enumeration> {
  using type = compat::decay_t<T>;
  using meta = typename reflected_enum_t<type>::meta;

  using owning = typename std::conditional<std::is_lvalue_reference<T>::value,
    std::false_type,
    std::true_type>::type;

  using storage_t = typename std::conditional<owning::value,
    type, //< own a value
    T //< hold a reference (T is U& / const U&)
    >::type;

  storage_t bound;

  static constexpr const char *name() noexcept {
    return meta::name();
  }

  operator const type &() const {
    return bound;
  }

  static constexpr auto enumerators() -> decltype(meta::enumerators()) {
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
constexpr auto reflected() -> reflected_t<T> {
  return {};
}

template <typename T>
constexpr auto reflected_tagged() -> reflected_tagged_t<T> {
  return {};
}

template <typename T>
constexpr auto reflected_enum() -> reflected_enum_t<T> {
  return {};
}

template <typename T,
  typename Binding = compat::conditional_t<std::is_lvalue_reference<T>::value,
    T, //< lvalues: keep reference (and const)
    compat::decay_t<T> //< rvalues: owning, decayed
    >>
constexpr auto reflected_tagged(T &&t)
  -> reflected_binding<Binding, reflected_entity::tagged> {
  return reflected_binding<Binding, reflected_entity::tagged>(
    std::forward<T>(t));
}

template <typename T,
  typename Binding = compat::conditional_t<std::is_lvalue_reference<T>::value,
    T, //< lvalues: keep reference (and const)
    compat::decay_t<T> //< rvalues: owning, decayed
    >>
constexpr auto reflected_enum(T &&t)
  -> reflected_binding<Binding, reflected_entity::enumeration> {
  return reflected_binding<Binding, reflected_entity::enumeration>(
    std::forward<T>(t));
}

template <typename T,
  typename Binding = compat::conditional_t<std::is_lvalue_reference<T>::value,
    T, //< lvalues: keep reference (and const)
    compat::decay_t<T> //< rvalues: owning, decayed
    >>
constexpr reflected_binding<Binding> reflected(T &&t) {
  return reflected_binding<Binding>(std::forward<T>(t));
}

// -- utility -------------------

template <template <typename...> class Variant, typename... T>
constexpr std::array<Variant<T...>, sizeof...(T)> tuple_to_array(
  std::tuple<T...> t) {
  return compat::apply(detail::_tuple_to_array<Variant, T...>{}, std::move(t));
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

// todo: do I even need this here?
// convenience adapter to get type info from Variant of field bindings. Example:
// for (auto f : omni::reflected(t).fields()) {
// std::cout << omni::type_info(f).name << '\n'; //< Polymorphic access without
// calling std::visit
// }
// template <template <typename...> class Variant, typename... T>
// constexpr type_info_t type_info(const Variant<T...> &t) {
//   // todo: constraints on T: it might be a field's generated Meta, or a
//   // reflected Binding. However, it shouldn't matter, since both of them
//   define
//   // `meta::name()`
//   return customization::visit<Variant>{}(
//     [](const auto &t) -> type_info_t {
//       return {
//         t.name(),
//       };
//     },
//     t);
// }

} // namespace omni
