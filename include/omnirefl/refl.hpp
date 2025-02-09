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

// C++11 adapter, because generic lambdas (with `auto` arg) are only available since C++14
template <typename T>
struct setter {
  virtual void operator()(T &&) const = 0;
  virtual void operator()(const T &) const = 0;

  protected:
  ~setter() = default;
};

template <typename Ref,
  typename Field,
  typename ValueT = typename std::decay<decltype(Field::get_value(std::declval<Ref>()))>::type>
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
template <typename Callable, typename Ref, typename Field, typename = void>
struct _is_get_set_invocable: std::false_type {
  using return_t = decltype(std::declval<Callable>()(Field::get_value(std::declval<const Ref>())));
};

template <typename Callable, typename Ref, typename Field>
struct _is_get_set_invocable<Callable,
  Ref,
  Field,
  void_t<decltype(std::declval<Callable>()(Field::get_value(std::declval<const Ref>()),
    std::declval<const _setter<Ref, Field>>()))>>: std::true_type {
  using return_t = decltype(std::declval<Callable>()(Field::get_value(std::declval<const Ref>()),
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
      return std::forward<Callable>(_c)(Field::get_value(_r), _setter<Ref, Field>{_r});
    }

    template <typename Field, typename ReturnT>
    constexpr ReturnT _invoke(std::false_type /*only get*/) const {
      return std::forward<Callable>(_c)(Field::get_value(_r));
    }

    // todo: enable if either
    // - is invocable with getter and setter, or
    // - is invocable with getter and NOT setter
    // - catch case to static_assert and reference the Callable type and value type

    template <typename Field,
      typename Invocable = detail::_is_get_set_invocable<Callable, Ref, Field>>
    constexpr auto operator()(Field) const -> typename Invocable::return_t {
      return _invoke<Field, typename Invocable::return_t>(
        std::integral_constant<bool, Invocable::value>{});
    }
  };

  template <typename Callable>
  friend constexpr auto visit(Callable &&c, variant_field<R, Fields...> v)
    // yes, this trailing decltype has to be this complicated, since it doesn't compile otherwise
    -> decltype(omni::visit(std::declval<_visitor<Callable &&, R &>>(),
      std::declval<omni::variant<Fields...>>())) {
    return omni::visit(_visitor<Callable, R>{std::forward<Callable>(c), v._ref}, v._v);
  }
};

template <typename T>
struct reflected_t;

/// (!!!) do not instantiate this outside of a reflected context
template <typename, typename = void>
struct is_reflected: std::false_type {};

/// (!!!) do not instantiate this outside of a reflected context
template <typename T>
struct is_reflected<T, detail::void_t<decltype(sizeof(reflected_t<T>))>>: std::true_type {};

// reflected binding is needed to hold a reference to the reflected object
template <typename T /*reflected_t<T>*/, typename /*Ref*/, typename = typename T::fields_t>
struct reflected_binding;

/// runtime adapter to conveniently operate on object's reflection data
template <typename T, typename R, typename... Fields>
struct reflected_binding<reflected_t<T>, R, std::tuple<Fields...>> {
  // R is the same as T, but a (possibly) const qualified reference
  R &_ref;
  std::array<variant_field<R, Fields...>, sizeof...(Fields)> fields{
    {{Fields::name(), _ref, omni::variant<Fields...>{Fields{}}}...}};

  constexpr reflected_binding(R &r): _ref(r) {
  }
};

/// access reflection data using a (potentially const) reference to the reflected object.
/// `reflected_t<T>` must be specialized at the point of invocation
template <typename T>
constexpr auto reflected(T &t) noexcept
  -> reflected_binding<reflected_t<typename std::decay<T>::type>, T> {
  return reflected_t<typename std::decay<T>::type>{}(t);
}

namespace detail {
namespace {
// tag used to collect reflected types
template <typename>
struct _reflected_type {};

// tag used to collect reflected implementation types
template <typename>
struct _reflected_impl {};
} // namespace
} // namespace detail

/// meta function to register the type for reflection
template <typename T>
void reflect(const T &) {
  (void)detail::_reflected_type<typename std::decay<T>::type>{};
}

/// meta function to register the type for reflection
template <typename T>
void reflect() {
  (void)detail::_reflected_type<typename std::decay<T>::type>{};
}

/// meta function to register the type to be used as implementation
template <typename T>
void use_impl(const T &) {
  (void)detail::_reflected_impl<typename std::decay<T>::type>{};
}

/// meta function to register the type to be used as implementation
template <typename T>
void use_impl() {
  (void)detail::_reflected_impl<typename std::decay<T>::type>{};
}

/// class to invoke a callable implementation object
struct reflected_call_t {
  template <typename Impl, typename T>
  void operator()(Impl &&impl, T &&t) const {
    use_impl(impl);
    reflect(t);
    _call_impl(std::forward<Impl>(impl), std::forward<T>(t));
  }

  template <typename Impl, typename T, typename R>
  void operator()(Impl &&impl, T &&t, R &result) const {
    use_impl(impl);
    reflect(t);
    _call_impl(std::forward<Impl>(impl), std::forward<T>(t), result);
  }

  // todo: do I need the other 2 versions??
  template <typename Impl, typename T, typename... Args>
  void operator()(Impl &&impl, T &&t, Args &&...args) const {
    use_impl(impl);
    reflect(t);
    _call_impl(std::forward<Impl>(impl), std::forward<T>(t), std::forward<Args>(args)...);
  }

  private:
  // implementation will be generated for this function by omnirefl
  template <typename Impl, typename... Args>
  static void _call_impl(Impl &&impl, Args &&...args);
} const reflected_call{};

} // namespace omni

// todo: pragma to enable the exposition
#include <tuple>

namespace omni {
struct _exposition {
  int field_a;
  const char *field_b;
};
} // namespace omni

#ifndef OMNI_DEFINE_NAME_FUNC
#  define OMNI_DEFINE_NAME_FUNC(STR) \
    constexpr static auto name() noexcept -> const char(&)[sizeof(STR)] { \
      return STR; \
    }
#endif

// todo: default value for a field
// this specialization will be generated by the tool
template <>
struct omni::reflected_t<omni::_exposition> {
  using type = omni::_exposition;

  struct field_a_t {
    OMNI_DEFINE_NAME_FUNC("field_a");
    constexpr static auto get_value(const type &t) noexcept -> const decltype(t.field_a) & {
      return t.field_a;
    }

    template <typename Value>
    static void set_value(type &t, Value &&v) noexcept {
      t.field_a = std::forward<Value>(v);
    }
  } constexpr static field_a{};

  struct field_b_t {
    OMNI_DEFINE_NAME_FUNC("field_b");
    constexpr static auto get_value(const type &t) noexcept -> const decltype(t.field_b) & {
      return t.field_b;
    }

    template <typename Value>
    static void set_value(type &t, Value &&v) noexcept {
      t.field_b = std::forward<Value>(v);
    }
  } constexpr static field_b{};

  using fields_t = std::tuple<field_a_t, field_b_t>;

  constexpr reflected_binding<reflected_t<type>, type> operator()(type &t) const noexcept {
    return {t};
  }
  constexpr reflected_binding<reflected_t<type>, const type> operator()(
    const type &t) const noexcept {
    return {t};
  }
};

#undef OMNI_DEFINE_NAME_FUNC
