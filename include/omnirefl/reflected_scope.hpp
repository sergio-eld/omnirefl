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

template <typename T>
using _meta = detail::_reflected<compat::decay_t<T>>;

template <typename Meta, typename = typename Meta::public_bases_t>
struct _all_public_fields;

template <typename Meta, typename... Bases>
struct _all_public_fields<Meta, std::tuple<Bases...>> {
  using type = decltype(std::tuple_cat(
    std::declval<typename _meta<Bases>::own_public_fields_t>()...,
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

template <typename T, reflected_entity = detail::_meta<T>::entity()>
struct reflected_t;

// struct | class | union
template <typename T>
using reflected_record_t = reflected_t<T, reflected_entity::record>;

template <typename T>
using reflected_enum_t = reflected_t<T, reflected_entity::enumeration>;

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
template <typename Record, typename Meta>
struct reflected_mem_binding {
  using type =
    compat::remove_cvref_t<decltype(Meta::value(std::declval<Record>()))>;

  using meta = Meta;

  Record &owner;

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
  template <typename U = Record>
  typename std::enable_if<!std::is_const<U>::value,
    mutable_mem_binding<type>>::type
    mutable_binding() const noexcept {
    return mutable_mem_binding<type>(owner, Meta{});
  }

  constexpr explicit reflected_mem_binding(Record &owner): owner(owner) {}
};

template <typename T>
struct reflected_t<T, reflected_entity::record> {
  using meta = detail::_meta<T>;
  using type = typename meta::type;

  using public_fields_t =
    detail::_all_public_fields_t<meta>; //< yields std::tuple<...>

  private:
  // C++11 ad hoc. `auto` lambda arguments only since C++14
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
  static constexpr auto _public_fields(_T &t) noexcept
    -> decltype(compat::apply(_bind_fields_metadata<_T>{t},
      public_fields_t{})) {
    return compat::apply(_bind_fields_metadata<_T>{t}, public_fields_t{});
  }

  public:
  static constexpr const char *name() noexcept {
    return meta::name();
  }

  static constexpr reflected_entity entity() noexcept {
    return meta::entity();
  }

  static constexpr public_fields_t public_fields() noexcept {
    return {};
  }

  static constexpr auto public_fields(const type &t) noexcept
    -> decltype(_public_fields(t)) {
    return _public_fields(t);
  }

  static constexpr auto public_fields(type &t) noexcept
    -> decltype(_public_fields(t)) {
    return _public_fields(t);
  }

  // todo: mutable_public_fields(T &t) and mutable_public_fields(const T &t)
};

template <typename T>
struct reflected_t<T, reflected_entity::enumeration> {
  using meta = detail::_meta<T>;
  using type = typename meta::type;

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
struct reflected_binding<T, reflected_entity::record> {
  using type = compat::decay_t<T>;
  using meta = typename reflected_record_t<type>::meta;

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
  auto public_fields() -> decltype(reflected_record_t<type>::public_fields(
    std::declval<storage_t &>())) {
    return reflected_record_t<type>::public_fields(bound);
  }

  constexpr auto public_fields() const
    -> decltype(reflected_record_t<type>::public_fields(
      std::declval<const storage_t &>())) {
    return reflected_record_t<type>::public_fields(bound);
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
  using meta = typename reflected_enum_t<T>::meta;
  using type = typename meta::type;

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
constexpr auto reflected_record() -> reflected_record_t<T> {
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
constexpr auto reflected_record(T &&t)
  -> reflected_binding<Binding, reflected_entity::record> {
  return reflected_binding<Binding, reflected_entity::record>(
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

} // namespace omni
