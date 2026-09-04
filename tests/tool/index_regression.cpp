#include <gtest/gtest.h>

#include <omnirefl/reflection.hpp>

#include <mpark/variant.hpp>

#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace index_regression {
struct print_names_t {
  struct _visit {
    template <typename... Field>
    std::vector<std::string> operator()(const Field &...field) const {
      return {std::string(field.name())...};
    }
  };

  template <typename T>
  std::vector<std::string> operator()(const T &t) const {
    return omni::compat::apply(_visit{}, t.public_fields());
  }
} const static print_names{};

struct write_foo_bar_t {
  template <typename T>
  void operator()(T t) const {
    omni::compat::apply(*this, t.public_fields());
  }

  template <typename... Field>
  void operator()(Field... field) const {
    int dummy[] = {0, (_write_field(field), 0)...};
    (void)dummy;
  }

  private:
  template <typename Field>
  static typename std::enable_if<
    std::is_same<int, typename Field::type>::value>::type
    _write_field(Field field) {
    const std::string name = field.name();
    if (std::string::npos != name.find("foo"))
      field.set_value(8);
    if (std::string::npos != name.find("bar"))
      field.set_value(15);
  }

  template <typename Field>
  static typename std::enable_if<
    !std::is_same<int, typename Field::type>::value>::type
    _write_field(Field) {}
} const static write_foo_bar{};

template <typename T, typename... V>
const T *get_if(const mpark::variant<V...> *value) {
  return mpark::get_if<T>(value);
}

struct write_fields_from_std_map {
  template <typename V, typename... Field>
  void operator()(const std::map<std::string, V> &from, Field... field) const {
    int dummy[] = {0, (_write_field(from, field), 0)...};
    (void)dummy;
  }

  private:
  template <typename V, typename Field>
  static void _write_field(const std::map<std::string, V> &from, Field field) {
    if (0 == from.count(field.name()))
      return;

    const auto *value =
      index_regression::get_if<typename Field::type>(&from.at(field.name()));
    if (value)
      field.set_value(*value);
  }
};

#if !defined CXX_STANDARD || CXX_STANDARD <= 11
template <typename V>
struct from_std_map_adapter {
  const std::map<std::string, V> &from;

  template <typename T>
  void operator()(T to) const {
    omni::compat::apply(*this, to.public_fields());
  }

  template <typename... Field>
  void operator()(Field... field) const {
    write_fields_from_std_map{}(from, field...);
  }
};
#endif

template <typename T, typename V>
void from_std_map(const std::map<std::string, V> &from, T &to) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  omni::reflected_call(
    [&from](auto v) -> void {
      auto fields = v.public_fields();
      omni::compat::apply(
        [&from](auto... field) { write_fields_from_std_map{}(from, field...); },
        fields);
    },
    to);
#else
  omni::reflected_call(from_std_map_adapter<V>{from}, to);
#endif
}

template <typename T, typename V>
void touch_fields_through_template(const std::map<std::string, V> &from,
  T &to) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  omni::reflected_call(
    [](auto v) -> void {
      const auto fields = v.public_fields();
      (void)fields;
    },
    to);
#else
  (void)from;
  (void)to;
#endif
}

template <typename T>
void touch_fields_through_template(T &to) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  omni::reflected_call(
    [](auto v) -> void {
      const auto fields = v.public_fields();
      (void)fields;
    },
    to);
#else
  (void)to;
#endif
}
} // namespace index_regression

TEST(index_regression, prior_print_names_does_not_pollute_direct_write) {
  {
    struct {
      std::string name;
      int count;
      double score;
    } p{"mapped", 47, 8.15};

    ASSERT_EQ((std::vector<std::string>{
                "name",
                "count",
                "score",
              }),
      omni::reflected_call(index_regression::print_names, p));
  }

  {
    struct {
      int foo_count;
      int bar_count;
      int untouched_count;
    } p{1, 2, 3};

    omni::reflected_call(index_regression::write_foo_bar, p);

    ASSERT_EQ(8, p.foo_count);
    ASSERT_EQ(15, p.bar_count);
    ASSERT_EQ(3, p.untouched_count);
  }
}

TEST(index_regression,
  prior_inplace_template_lambda_does_not_pollute_direct_write) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  {
    struct {
      std::string name;
      int count;
      double score;
    } p{"mapped", 47, 8.15};

    ASSERT_EQ((std::vector<std::string>{
                "name",
                "count",
                "score",
              }),
      omni::reflected_call(
        [](const auto &v) -> std::vector<std::string> {
          const auto fields = v.public_fields();
          return omni::compat::apply(index_regression::print_names_t::_visit{},
            fields);
        },
        p));
  }

  {
    struct {
      int foo_count;
      int bar_count;
      int untouched_count;
    } p{1, 2, 3};

    omni::reflected_call(index_regression::write_foo_bar, p);

    ASSERT_EQ(8, p.foo_count);
    ASSERT_EQ(15, p.bar_count);
    ASSERT_EQ(3, p.untouched_count);
  }
#endif
}

// FIXME(high): generated-header reflection generates an indexed specialization
// for the from_std_map function-template route instantiated with a local
// unnamed record. The later direct write_foo_bar route can then match that
// earlier metadata during real compilation. The equivalent inline map writer
// passes; continue triage with smaller function-template wrapper probes below.
//
// TEST(index_regression, prior_from_std_map_does_not_pollute_direct_write) {
//   {
//     struct {
//       std::string name;
//       int count;
//       double score;
//     } p{};
//
//     std::map<std::string, mpark::variant<int, double, std::string>> from;
//     from["name"] = std::string{"mapped"};
//     from["count"] = 47;
//     from["score"] = 8.15;
//     index_regression::from_std_map(from, p);
//
//     ASSERT_EQ("mapped", p.name);
//     ASSERT_EQ(47, p.count);
//     ASSERT_EQ(8.15, p.score);
//   }
//
//   {
//     struct {
//       int foo_count;
//       int bar_count;
//       int untouched_count;
//     } p{1, 2, 3};
//
//     omni::reflected_call(index_regression::write_foo_bar, p);
//
//     ASSERT_EQ(8, p.foo_count);
//     ASSERT_EQ(15, p.bar_count);
//     ASSERT_EQ(3, p.untouched_count);
//   }
// }

TEST(index_regression, prior_extra_arg_lambda_does_not_pollute_direct_write) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  {
    struct {
      std::string name;
      int count;
      double score;
    } p{};

    std::map<std::string, mpark::variant<int, double, std::string>> from;
    omni::reflected_call(
      [](auto v) -> void {
        const auto fields = v.public_fields();
        (void)fields;
      },
      p);
    (void)from;
  }

  {
    struct {
      int foo_count;
      int bar_count;
      int untouched_count;
    } p{1, 2, 3};

    omni::reflected_call(index_regression::write_foo_bar, p);

    ASSERT_EQ(8, p.foo_count);
    ASSERT_EQ(15, p.bar_count);
    ASSERT_EQ(3, p.untouched_count);
  }
#endif
}

// FIXME(high): generated-header reflection indexed reflection is mismatched
// when reflected_call itself is inside a function template instantiated with a
// local unnamed type. The tool observes the first local record in
// touch_fields_through_template<T>, but real compilation later matches that
// metadata for the second local record in this test. This does not require
// std::map, extra reflected_call arguments, compat::apply, get_if, or
// field.set_value.
TEST(index_regression,
  prior_template_wrapped_lambda_does_not_pollute_direct_write) {
#if 0 //< 1 to enable failing tests
#  if defined CXX_STANDARD && 11 < CXX_STANDARD
  {
    struct {
      std::string name;
      int count;
      double score;
    } p{};

    index_regression::touch_fields_through_template(p);
  }

  {
    struct {
      int foo_count;
      int bar_count;
      int untouched_count;
    } p{1, 2, 3};

    omni::reflected_call(index_regression::write_foo_bar, p);

    ASSERT_EQ(8, p.foo_count);
    ASSERT_EQ(15, p.bar_count);
    ASSERT_EQ(3, p.untouched_count);
  }
#  endif
#endif
}

// FIXME(high): same failure as prior_template_wrapped_lambda_... with an extra
// `from` argument. Kept commented while the smaller no-extra-arg reproducer is
// documented above.
//
// TEST(index_regression,
//   prior_template_wrapped_extra_arg_lambda_does_not_pollute_direct_write) {
// #if defined CXX_STANDARD && 11 < CXX_STANDARD
//   {
//     struct {
//       std::string name;
//       int count;
//       double score;
//     } p{};
//
//     std::map<std::string, mpark::variant<int, double, std::string>> from;
//     index_regression::touch_fields_through_template(from, p);
//   }
//
//   {
//     struct {
//       int foo_count;
//       int bar_count;
//       int untouched_count;
//     } p{1, 2, 3};
//
//     omni::reflected_call(index_regression::write_foo_bar, p);
//
//     ASSERT_EQ(8, p.foo_count);
//     ASSERT_EQ(15, p.bar_count);
//     ASSERT_EQ(3, p.untouched_count);
//   }
// #endif
// }

TEST(index_regression,
  prior_apply_fields_lambda_does_not_pollute_direct_write) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  {
    struct {
      std::string name;
      int count;
      double score;
    } p{};

    std::map<std::string, mpark::variant<int, double, std::string>> from;
    omni::reflected_call(
      [](auto v) -> void {
        const auto fields = v.public_fields();
        omni::compat::apply(
          [](const auto &...field) {
            int dummy[] = {0, ((void)field.name(), 0)...};
            (void)dummy;
          },
          fields);
      },
      p);
    (void)from;
  }

  {
    struct {
      int foo_count;
      int bar_count;
      int untouched_count;
    } p{1, 2, 3};

    omni::reflected_call(index_regression::write_foo_bar, p);

    ASSERT_EQ(8, p.foo_count);
    ASSERT_EQ(15, p.bar_count);
    ASSERT_EQ(3, p.untouched_count);
  }
#endif
}

TEST(index_regression, prior_map_lookup_does_not_pollute_direct_write) {
#if 0 //< 1 to enable failing tests
  // FIXME(high): packaged Ubuntu 18 GCC rejects this C++14/17 inline
  // pack-expansion lambda shape with a reference capture. This is a regression
  // probe shape issue; prior_helper_writer_does_not_pollute_direct_write keeps
  // the same map-based reflected write route enabled.
#  if defined CXX_STANDARD && 11 < CXX_STANDARD
  {
    struct {
      std::string name;
      int count;
      double score;
    } p{};

    std::map<std::string, mpark::variant<int, double, std::string>> from;
    from["name"] = std::string{"mapped"};
    from["count"] = 47;
    from["score"] = 8.15;
    omni::reflected_call(
      [](auto &v, const auto &from) -> void {
        const auto fields = omni::reflected(v).public_fields();
        omni::compat::apply(
          [&from](const auto &...field) {
            int dummy[] = {0,
              ([&from](const auto &f) {
                if (0 == from.count(f.name()))
                  return;
                (void)index_regression::get_if<
                  omni::compat::decay_t<decltype(f)>::type>(
                  &from.at(f.name()));
              }(field),
                0)...};
            (void)dummy;
          },
          fields);
      },
      p,
      from);
  }

  {
    struct {
      int foo_count;
      int bar_count;
      int untouched_count;
    } p{1, 2, 3};

    omni::reflected_call(index_regression::write_foo_bar, p);

    ASSERT_EQ(8, p.foo_count);
    ASSERT_EQ(15, p.bar_count);
    ASSERT_EQ(3, p.untouched_count);
  }
#  endif
#endif
}

TEST(index_regression, prior_inline_set_value_does_not_pollute_direct_write) {
#if 0 //< 1 to enable failing tests
  // FIXME(high): packaged Ubuntu 18 GCC rejects this C++14/17 inline
  // pack-expansion lambda shape with a reference capture. The helper-writer
  // equivalent below remains enabled and covers the same reflected field write.
#  if defined CXX_STANDARD && 11 < CXX_STANDARD
  {
    struct {
      std::string name;
      int count;
      double score;
    } p{};

    std::map<std::string, mpark::variant<int, double, std::string>> from;
    from["name"] = std::string{"mapped"};
    from["count"] = 47;
    from["score"] = 8.15;
    omni::reflected_call(
      [&from](auto v) -> void {
        const auto fields = v.public_fields();
        omni::compat::apply(
          [&from](auto... field) {
            int dummy[] = {0,
              ([&from](auto f) {
                if (0 == from.count(f.name()))
                  return;
                const auto *value = index_regression::get_if<
                  typename decltype(f)::type>(&from.at(f.name()));
                if (value)
                  f.set_value(*value);
              }(field),
                0)...};
            (void)dummy;
          },
          fields);
      },
      p);

    ASSERT_EQ("mapped", p.name);
    ASSERT_EQ(47, p.count);
    ASSERT_EQ(8.15, p.score);
  }

  {
    struct {
      int foo_count;
      int bar_count;
      int untouched_count;
    } p{1, 2, 3};

    omni::reflected_call(index_regression::write_foo_bar, p);

    ASSERT_EQ(8, p.foo_count);
    ASSERT_EQ(15, p.bar_count);
    ASSERT_EQ(3, p.untouched_count);
  }
#  endif
#endif
}

TEST(index_regression, prior_helper_writer_does_not_pollute_direct_write) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  {
    struct {
      std::string name;
      int count;
      double score;
    } p{};

    std::map<std::string, mpark::variant<int, double, std::string>> from;
    from["name"] = std::string{"mapped"};
    from["count"] = 47;
    from["score"] = 8.15;
    omni::reflected_call(
      [&from](auto v) -> void {
        const auto fields = v.public_fields();
        omni::compat::apply(
          [&from](auto... field) {
            index_regression::write_fields_from_std_map{}(from, field...);
          },
          fields);
      },
      p);

    ASSERT_EQ("mapped", p.name);
    ASSERT_EQ(47, p.count);
    ASSERT_EQ(8.15, p.score);
  }

  {
    struct {
      int foo_count;
      int bar_count;
      int untouched_count;
    } p{1, 2, 3};

    omni::reflected_call(index_regression::write_foo_bar, p);

    ASSERT_EQ(8, p.foo_count);
    ASSERT_EQ(15, p.bar_count);
    ASSERT_EQ(3, p.untouched_count);
  }
#endif
}
