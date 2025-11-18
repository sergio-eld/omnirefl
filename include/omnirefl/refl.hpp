#pragma once

// todo: copiright notice (MIT)

#if __cplusplus >= 201703L
#  define OMNI_HAS_CPP17
#else
#endif

// todo: reduce dependencies on system headers to improve parsing performance.
//   consider having reflection interface as a separate header.
#include <array>
#include <type_traits>
#include <utility>

#ifdef OMNI_HAS_CPP17
#  include <string_view>
#  include <variant>
namespace omni {
using std::variant;
using std::visit;

using std::string_view;
} // namespace omni
#else
#  include <mpark/variant.hpp>
#  include <nonstd/string_view.hpp>
namespace omni {
using mpark::variant;
using mpark::visit;

using nonstd::string_view;
} // namespace omni
#endif

// todo: iteration 2
//  - provide `omni` utilities
//  - should be compatible with at least C++11
//  - must only define reflection-related interface
namespace omni {
// todo: separate the functions below to a standalone header without any
// includes
//
// refactorme: move `detail` into private scope of `reflected_call_t`
namespace detail {
namespace {

#ifdef OMNI_HEADER_REFLECTION

template <int Id>
struct counter {
  struct generator {
#  if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wnon-template-friend"
#  elif defined _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4396)
#  endif

    // This does not compile on GCC < 11, and gives warning if not a template
    // template <typename...>
    friend constexpr bool generate(counter) {
      return true;
    }
  };

  // template <typename...>
  friend constexpr bool generate(counter);

#  if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#    pragma GCC diagnostic pop
#  elif defined(_MSC_VER)
#    pragma warning(pop)
#  endif

#  if defined _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4514)
#    pragma warning(disable : 4710)
#  endif

  template <typename Tag = counter, int I = (int)generate(Tag{})>
  static constexpr std::true_type exists(int) {
    return {};
  }

  static constexpr std::false_type exists(...) {
    return generator(), std::false_type{};
  }
#  if defined(_MSC_VER)
#    pragma warning(pop)
#  endif
};

template <typename T, int Id>
constexpr int unique_id(std::false_type) {
  return Id;
}

template <typename T, int Id>
constexpr int unique_id(std::true_type);

template <typename T, int Id = int{}>
constexpr int unique_id() {
  return unique_id<T, Id>(counter<Id>::exists(Id));
}

template <typename T, int Id>
constexpr int unique_id(std::true_type) {
  return unique_id<T, Id + 1>();
}

// meta type to assign index to a type upon instantiation
template <typename T, int Index = unique_id<T>()>
struct _reflected_indexed_type {};

#else

// tag used in target mode to collect reflected types
template <typename>
struct _reflected_type {};

// tag used in target mode to collect reflected implementation types
template <typename>
struct _reflected_impl {};

#endif
} // namespace
} // namespace detail

/// class to invoke a callable implementation object
struct reflected_call_t {
  template <typename Impl, typename T, typename... Args>
  auto operator()(Impl &&impl, T &&t, Args &&...args) const {
    using type = typename std::decay<T>::type;
#ifdef OMNI_HEADER_REFLECTION
    // testme: use inside inline non-template function defined in a header file
    // testme: use inside a template function defined in a header file
    //
    //   forced include may break the order of index instantiations (as long as
    //   header-mode includes headers of reflected types).
    (void)detail::_reflected_indexed_type<type>{};
#  ifdef OMNI_INCLUDED_GENERATED_REFLECTION_HEADER
    return std::forward<Impl>(
      impl)(std::forward<T>(t), std::forward<Args>(args)...);
#  endif
#else
    (void)detail::_reflected_impl<typename std::decay<Impl>::type>{};
    (void)detail::_reflected_type<type>{};
    return _call_impl(std::forward<Impl>(impl),
      std::forward<T>(t),
      std::forward<Args>(args)...);
#endif
  }

  private:
#ifndef OMNI_HEADER_REFLECTION
  // implementation will be generated for this function by omnirefl
  template <typename Impl, typename... Args>
  static auto _call_impl(Impl &&impl, Args &&...args)
    -> decltype(std::declval<Impl>()(std::declval<Args>()...));
#endif
} const reflected_call{};

namespace detail {
namespace {

// refactorme: this is a very confusing and ugle shite
#ifdef OMNI_HEADER_REFLECTION
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
#endif // OMNI_HEADER_REFLECTION

} // namespace
} // namespace detail

/**
 * interface to get reflection data for T.
 * example:
 ```
 struct reflected_user_impl {
   template <typename T>
   constexpr void operator()(const T &t) const {
   // reflected context inside this scope
   using refl = omni::reflected_t<T>;
   }
 };
 ```
 * IMPORTANT: do not instantiate outside of a reflected context.
 */
template <typename T>
using reflected_t = detail::_reflected<typename std::decay<T>::type>;

/// todo: use the tool's pass to detect the invalid usage
/// do not add specializations
/// (!!!) do not instantiate this outside of a reflected context
template <typename T, typename = void>
struct is_reflected;

namespace detail {
namespace {
template <typename... Ts>
struct make_void {
  typedef void type;
};

template <typename... Ts>
using void_t = typename make_void<Ts...>::type;

} // namespace
} // namespace detail

// C++11 adapter, because generic lambdas (with `auto` arg) are only available
// since C++14
template <typename T>
struct setter {
  virtual void operator()(T &&) const = 0;
  virtual void operator()(const T &) const = 0;

  protected:
  ~setter() = default;
};

template <typename Ref,
  typename Field,
  typename ValueT =
    typename std::decay<decltype(Field::get_value(std::declval<Ref>()))>::type>
struct _setter: setter<ValueT> {
  // todo: static_assert Field belongs to Ref

  // somewhere such error should be reported in a clear manner
  // todo: static_assert(std::is_const<Ref>() && !Field::is_mutable, "")
  Ref &_r;
  constexpr _setter(Ref &r): _r(r) {
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

namespace detail {
namespace {
// refactorme: simplify
template <typename Callable, typename Ref, typename Field, typename = void>
struct _is_get_set_invocable: std::false_type {
  using return_t = decltype(std::declval<Callable>()(
    Field::get_value(std::declval<const Ref>())));
};

template <typename Callable, typename Ref, typename Field>
struct _is_get_set_invocable<Callable,
  Ref,
  Field,
  void_t<decltype(std::declval<Callable>()(
    Field::get_value(std::declval<const Ref>()),
    std::declval<const _setter<Ref, Field>>()))>>: std::true_type {
  using return_t = decltype(std::declval<Callable>()(
    Field::get_value(std::declval<const Ref>()),
    std::declval<_setter<Ref, Field>>()));
};
} // namespace
} // namespace detail

template <typename R, typename... Fields>
struct variant_field {
  // reflected structure's field name
  omni::string_view name;

  // implementation-related data
  R &_ref;
  omni::variant<Fields...> _v;

  private:
  template <typename Callable, typename Ref>
  struct _visitor {
    Callable &&_c;
    Ref &_r;

    template <typename Field, typename ReturnT>
    constexpr ReturnT _invoke(std::true_type /*get, set*/) const {
      return std::forward<Callable>(
        _c)(Field::get_value(_r), _setter<Ref, Field>{_r});
    }

    template <typename Field, typename ReturnT>
    constexpr ReturnT _invoke(std::false_type /*only get*/) const {
      return std::forward<Callable>(_c)(Field::get_value(_r));
    }

    // todo: enable if either
    // - is invocable with getter and setter, or
    // - is invocable with getter and NOT setter
    // - catch case to static_assert and reference the Callable type and value
    // type

    template <typename Field,
      typename Invocable = detail::_is_get_set_invocable<Callable, Ref, Field>>
    constexpr auto operator()(Field) const -> typename Invocable::return_t {
      return _invoke<Field, typename Invocable::return_t>(
        std::integral_constant<bool, Invocable::value>{});
    }
  };

  template <typename Callable>
  friend constexpr auto visit(Callable &&c, variant_field<R, Fields...> v)
    // yes, this trailing decltype has to be this complicated, since it doesn't
    // compile otherwise
    -> decltype(omni::visit(std::declval<_visitor<Callable &&, R &>>(),
      std::declval<omni::variant<Fields...>>())) {
    return omni::visit(_visitor<Callable, R>{std::forward<Callable>(c), v._ref},
      v._v);
  }
};

// refactorme: default template arguments are not good for forward declarations
// reflected binding is needed to hold a reference to the reflected object
template <typename T, typename = typename reflected_t<T>::fields_t>
struct reflected_binding;

/// runtime adapter to conveniently operate on object's reflection data
template <typename T, typename... Fields>
struct reflected_binding<T, std::tuple<Fields...>> {
  // possibly const qualified reference
  T &_ref;

  // refactorme: can this be simplified?
  // array of fields that allows runtime loop-based iteration
  std::array<variant_field<T, Fields...>, sizeof...(Fields)> fields{
    {{Fields::name(), _ref, omni::variant<Fields...>{Fields{}}}...}};

  // requied for C++11, since `_ref` is used for `fields` default initialization
  constexpr reflected_binding(T &ref): _ref(ref) {
  }
  constexpr reflected_binding(const reflected_binding &) = default;
};

/// access reflection data using a (potentially const) reference to the
/// reflected object. `reflected_t<T>` must be specialized at the point of
/// invocation. warning: DO NOT call this function outside of the reflected
/// implementation. todo: implement check within the tool to validate that this
/// is not called outside of the reflected implementation
template <typename T>
constexpr reflected_binding<T> reflected(T &t) noexcept {
  // todo: error text or remove this and use sfinae?
  static_assert(is_reflected<T>::value,
    "omni::reflected(obj) must be called only in a reflected context for reflected type");
  return {t};
}

} // namespace omni

// refactorme:
//   exposition should be generated as part of packaging the tool, otherwise I
//   won't be able to keep it up to date.
#ifdef OMNI_ENABLE_EXPOSITION

#  include <tuple>

namespace omni {
struct _exposition {
  int field_a;
  const char *field_b;
};
} // namespace omni

#  ifndef OMNI_DEFINE_NAME_FUNC
#    define OMNI_DEFINE_NAME_FUNC(STR) \
      constexpr static auto name() noexcept -> const char (&)[sizeof(STR)] { \
        return STR; \
      }
#  endif

// todo: actualize exposition example
// todo: default value for a field
// this specialization will be generated by the tool
template <>
struct omni::reflected_t<omni::_exposition> {
  using type = omni::_exposition;

  struct field_a_t {
    OMNI_DEFINE_NAME_FUNC("field_a");
    constexpr static auto get_value(const type &t) noexcept -> const
      decltype(t.field_a) & {
      return t.field_a;
    }

    template <typename Value>
    static void set_value(type &t, Value &&v) noexcept {
      t.field_a = std::forward<Value>(v);
    }
  } constexpr static field_a{};

  struct field_b_t {
    OMNI_DEFINE_NAME_FUNC("field_b");
    constexpr static auto get_value(const type &t) noexcept -> const
      decltype(t.field_b) & {
      return t.field_b;
    }

    template <typename Value>
    static void set_value(type &t, Value &&v) noexcept {
      t.field_b = std::forward<Value>(v);
    }
  } constexpr static field_b{};

  using fields_t = std::tuple<field_a_t, field_b_t>;

  constexpr reflected_binding<reflected_t<type>, type> operator()(
    type &t) const noexcept {
    return {t};
  }
  constexpr reflected_binding<reflected_t<type>, const type> operator()(
    const type &t) const noexcept {
    return {t};
  }
};

#endif

#undef OMNI_DEFINE_NAME_FUNC
