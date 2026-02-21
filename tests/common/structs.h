#pragma once

// #include <eld/pattern_matching.hpp>
// #include <mpark/variant.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <map>
#include <string>
#include <type_traits>
#include <vector>

// todo: maybe add support for
// std::string_view field_name =
//   reflected_call(field_name_as_string, &struct::name);
//
// but I think it might be already achievable

// fixme: remove. this is for debugging
namespace example_types {
struct wrestler;
}

namespace example_impl {

// todo: non-reflectable types as arguments (std::tuple, std::vector, etc.)

struct print_type_info_t {
  struct result {
    std::string name;
    std::vector<std::string> namespaces;
  };

  template <typename T>
  result operator()(const T &) const {
    // todo: implement
    //
    // const auto& namespaces = omni::reflected(t).namespaces;
    // return {
    // .name = omni::reflected(t).type_name,
    // .namespaces = {namespaces.begin(), namespaces.end()},
    // };
    return {};
  }
} const static print_type_info{};

static const struct print_enum_type_info_t {
  struct result {
    print_type_info_t::result type_info;
    std::vector<std::string> names;
  };

  template <typename T,
    typename = typename std::enable_if<std::is_enum<T>::value>::type>
  result operator()(const T &t) const {
    return {
      /*type_info=*/print_type_info(t),
      /*names=*/{}, //< todo: implement
    };
  }
} print_enum_type_info{};

static const struct print_field_names_simple_t {
  // C++11 compat (auto lambda args support since C++14)
  struct _get_field_names {
    template <typename... Field>
    std::vector<std::string> operator()(const Field &...f) const noexcept {
      return std::vector<std::string>{f.name()...};
    }
  };

  template <typename T>
  std::vector<std::string> operator()(const T &t) const {
    return omni::compat::apply(_get_field_names{}, omni::reflected(t).public_fields());
  }
} print_field_names_simple{};

static const struct print_field_names_recursive_t {
  // C++11 compat
  struct _print {
    template <typename... Fields>
    std::vector<std::string> operator()(const Fields &...f) const noexcept {
      // todo: implement
      return std::vector<std::string>{f.name()...};
    }
  };

  template <typename T>
  std::vector<std::string> operator()(const T &t) const {
    return omni::compat::apply(_print{}, omni::reflected(t).public_fields());
  }
} print_field_names_recursive{};

// fixme: enable after refining
// todo: return vector
// struct print_field_values_recursive_t {
//   template <typename T>
//   void operator()(const T &t, std::vector<std::string> &out) const {
//     namespace pm = pattern_matching;
//     for (auto f : omni::reflected(t).fields) {
//       f
//         | pm::matched_in_place( //
//           pm::m_if<std::is_fundamental>([&](const auto &v) { //
//             out.emplace_back(std::string(f.name) + ": " + std::to_string(v));
//           }),
//           pm::m_is<std::basic_string>([&](const auto &v) { //
//             out.emplace_back(std::string(f.name) + ": \"" + v + "\"");
//           }),
//           pm::m_is<std::vector>([&](const auto &v) {
//             // !!! nested type in vector is expected to be a struct for this
//             // example
//             using nested_type = typename
//             std::decay_t<decltype(v)>::value_type; std::vector<std::string>
//             sub;
//             // ad hoc. as of this writing calling `.fields` is only possible
//             on
//             // a reflected_binding, which requires a reference. todo: add
//             // similar method, like `omni::reflected_t<nested_type>::fields`
//             static const nested_type ad_hoc_dummy{};
//             sub.reserve(omni::reflected(ad_hoc_dummy).fields.size());
//             for (size_t i = 0; i < v.size(); ++i) {
//               (*this)(v[i], sub);
//               for (const auto &s : sub) {
//                 out.emplace_back(
//                   std::string(f.name) + "[" + std::to_string(i) + "]." + s);
//               }
//               sub.clear();
//             }
//           }),
//           pm::m_if<omni::is_reflected>([&](const auto &v) {
//             std::vector<std::string> sub;
//             sub.reserve(omni::reflected(v).fields.size());
//             (*this)(v, sub);
//             for (const auto &s : sub)
//               out.emplace_back(std::string(f.name) + "." + s);
//           }),
//           pm::m_any([](const auto &) { /*no-op*/ }));
//     }
//   }
// } const static print_field_values_recursive{};

// fixme: enable after refining
// naive implementation, does not support containers
// struct simple_from_map_t {
//   // todo: modify with type_identity<T> as a reflected type. It may be not
//   // default-constructible
//   template <typename T>
//   void operator()(T &to,
//     const std::map<std::string, std::string> &from) const noexcept {
//     namespace pm = pattern_matching;
//     for (auto f : omni::reflected(to).fields) {
//       const auto it = from.find(std::string(f.name));
//       if (it == from.cend())
//         continue;
//       f
//         | pm::matched_in_place(
//           [&](const int &, const omni::setter<int> &set_value) {
//             set_value(std::stoi(it->second));
//           },
//           [&](const std::string &, const omni::setter<std::string>
//           &set_value) {
//             set_value(it->second);
//           });
//     }
//   }
// } const static simple_from_map{};

} // namespace example_impl

namespace example_types {

struct championship {
  std::string name;
  std::string title;
};

struct wrestler {
  std::string name;
  int age;
  std::string catchphrase{};
  std::vector<championship> titles{};

  // fixme: implement support in header-mode
  class info_t {
    public:
    std::string ring_name;
    std::string signature_move;
    int debut_year;
  } info{};
  // todo: add support for an unnamed nested struct (specialization is possible
  // via `decltype(std::declval<person>().unnamed_nested)`
};

struct settable {
  std::string str;
  int i;
};

enum ring_style {
  rs_technical,
  rs_high_flying,
  rs_power,
};

enum class brand {
  raw,
  smackdown,
  nxt,
};

struct enum_holder {
  enum alignment {
    face,
    heel,
    tweener,
  };

  enum class status {
    active,
    injured,
    retired,
  };

  alignment current_alignment{};
  status current_status{};
};

struct with_unnamed_enum_member {
  enum {
    flag_a,
    flag_b,
  } flags{};
};

// -- Types reflected as dependencies
namespace dependency {

struct member_sub {
  std::string ms_str;
  int ms_int;
};

struct vec_elem {
  std::string ve_str;
};

struct map_key {
  std::string mk_str;

  bool operator<(const map_key &o) const {
    return mk_str < o.mk_str;
  }
};

struct tuple_elem {
  std::string te_str;
};

struct variant_elem {
  std::string ve_str;
};

struct alias_type {
  std::string at_str;
};

enum title_rank {
  tr_midcard,
  tr_main_event,
};

enum class promotion {
  wwe,
  aew,
  njpw,
};

} // namespace dependency

struct with_member {
  dependency::member_sub member;
};

struct with_vec {
  std::vector<dependency::vec_elem> vec;
};

struct with_map_key {
  std::map<dependency::map_key, int> mp;
};

struct with_tuple {
  std::tuple<dependency::tuple_elem, int> tp;
};

// fixme: enable after refining
// fixme: for some reason this picks up `mpark::variant` as a dependency type,
// but it is not a type - it is a template
// struct with_variant {
//   mpark::variant<dependency::variant_elem, int> vr;
// };

struct with_alias {
  using type = dependency::alias_type;
};

struct with_enum_dependency_unscoped {
  dependency::title_rank rank;
};

struct with_enum_dependency_scoped {
  dependency::promotion company;
};

} // namespace example_types
