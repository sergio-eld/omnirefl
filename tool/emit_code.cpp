#include "tool/emit_code.hpp"

#include "tool/util.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <string_view>

namespace {
bool is_cpp_file(const std::filesystem::path &filename) {
  const auto &ext = filename.extension();
  return ".cpp" == ext || ".cxx" == ext || ".c" == ext;
}

auto fmt_include(const std::filesystem::path &p,
  fmt::format_context &ctx) noexcept {
  return fmt::format_to(ctx.out(), "#include \"{}\"", p.string());
};

auto fmt_reflected_field(const tool::refl::struct_field_data &field,
  fmt::format_context &ctx) {
  return fmt::format_to(ctx.out(),
    // ad hoc: manual offset
    R"(  struct {field_name}_t {{
    OMNI_DEFINE_NAME_FUNC("{field_name}")
    constexpr static auto get_value(const type &t) noexcept
      -> const decltype(t.{field_name})& {{
      return t.{field_name};
    }}

    template <typename V>
    static void set_value(type &t, V &&v) {{
      t.{field_name} = std::forward<V>(v);
    }}
  }} constexpr static {field_name}{{}};)",
    fmt::arg("field_name", field.name));
}

auto fmt_indexed_reflected_field(std::string_view field_name,
  fmt::format_context &ctx) {
  return fmt::format_to(ctx.out(),
    // ad hoc: manual offset
    R"(  struct {field_name}_t {{
    OMNI_DEFINE_NAME_FUNC("{field_name}")
    template <typename T>
    constexpr static auto get_value(const T &t) noexcept
      -> const decltype(t.{field_name})& {{
      return t.{field_name};
    }}

    template <typename T, typename V>
    static void set_value(T &t, V &&v) {{
      t.{field_name} = std::forward<V>(v);
    }}
  }} constexpr static {field_name}{{}};)",
    fmt::arg("field_name", field_name));
}

auto fmt_reflected_specialization(const codegen::reflected_type &refl_type,
  fmt::format_context &ctx) noexcept {
  using namespace std::string_view_literals;

  return fmt::format_to(ctx.out(),
    R"(template <typename T>
struct _reflected<{type_tag} {type_name}, T> {{
  static_assert(std::is_same<{type_tag} {type_name}, T>::value,
    "omnirefl: unexpected types mismatch, try regenerating");
  using type = T;

  // fields meta types declarations
{reflected_fields}

  using fields_t = std::tuple<
{field_names}
  >;

  constexpr reflected_binding<type, fields_t> operator()(type &t) const noexcept {{
    return {{t}};
  }}

  constexpr reflected_binding<const type, fields_t> operator()(const type &t) const noexcept {{
    return {{t}};
  }}
}};)",
    fmt::arg("type_tag", refl_type.is_class ? "class"sv : "struct"sv),
    fmt::arg("type_name", refl_type.name),
    fmt::arg("reflected_fields", //
      util::join(refl_type.fields, "\n\n", fmt_reflected_field)),
    fmt::arg("field_names",
      util::join(refl_type.fields,
        ",\n",
        [](const tool::refl::struct_field_data &field, fmt::context &ctx) {
          return fmt::format_to(ctx.out(),
            // ad hoc: manual offset
            "    {}_t",
            field.name);
        })));
};

auto fmt_indexed_reflected_specialization(size_t index,
  const std::vector<std::string> &field_names,
  fmt::context &ctx) {
  using namespace std::string_view_literals;

  return fmt::format_to(ctx.out(),
    R"(template <>
struct _indexed_reflected<{index}> {{

  // fields meta types declarations
{reflected_fields}

  using fields_t = std::tuple<
{field_names}
  >;

  // fixme: `reflected_binding<T>` is not defined
  template <typename T>
  constexpr reflected_binding<T, fields_t> operator()(T &t) const noexcept {{
    return {{t}};
  }}

  // fixme: `reflected_binding<T>` is not defined
  template <typename T>
  constexpr reflected_binding<const T, fields_t> operator()(const T &t) const noexcept {{
    return {{t}};
  }}
}};)",
    fmt::arg("index", index),
    fmt::arg("reflected_fields", //
      util::join(field_names, "\n\n", fmt_indexed_reflected_field)),
    fmt::arg("field_names",
      util::join(field_names,
        ",\n",
        // ad hoc: manual offset
        [] { return "    {}_t"; })));
}

auto fmt_forward_declaration(const codegen::reflected_type &t,
  fmt::context &ctx) {
  std::vector<std::string_view> namespaces;
  std::string_view name = t.name;
  for (size_t pos = name.find("::"); pos != name.npos; pos = name.find("::")) {
    namespaces.emplace_back(name.substr(0, pos));
    name = name.substr(pos + 2);
  }

  return fmt::format_to(ctx.out(),
    "{open_namespaces}\n{tag} {name};\n{close_namespaces}",
    fmt::arg("open_namespaces",
      util::join(namespaces, "\n", [] { return "namespace {} {{"; })),
    fmt::arg("tag", t.is_class ? "class" : "struct"),
    fmt::arg("name", name),
    fmt::arg("close_namespaces",
      util::join(namespaces, "\n", [] { return "}} // namespace {}"; })));
}

auto fmt_reflected_call(const tool::refl::func_signature &func_sig,
  fmt::format_context &ctx) {
  return fmt::format_to(ctx.out(),
    R"(
template<>
void omni::reflected_call_t::_call_impl(
  {params}) {{
  {invoke}({args});
}})",
    fmt::arg("params",
      util::join( //
        util::indexed(func_sig.args),
        // ad hoc: manual offset
        ",\n  ",
        [](const auto &_param, fmt::format_context &ctx) {
          const auto &[_p, index] = _param;
          const tool::refl::function_signature_arg &param = _p;
          // todo:
          std::string param_name = 0 == index //
            ? std::string("impl")
            : std::to_string(index);
          return fmt::format_to(ctx.out(),
            "{nm_qual_type} {const}{ref}_{param_name}",
            fmt::arg("nm_qual_type", param.nm_qual_type),
            fmt::arg("const", param.is_const ? "const " : ""),
            fmt::arg("ref",
              [](tool::refl::reference_type r) -> std::string_view {
                switch (r) {
                case tool::refl::reference_type::ref_lval:
                  return "&";
                case tool::refl::reference_type::ref_rval:
                  return "&&";
                case tool::refl::ref_none:
                  return "";
                }
                return "/*unresolved_reference_type*/";
              }(param.ref_type)),
            fmt::arg("param_name", std::move(param_name)));
        })),
    fmt::arg("invoke", "_impl"),
    fmt::arg("args",
      util::join(
        // todo: sliced(func_sig.args, 1)
        util::indexed(func_sig.args),
        "",
        [](const auto &_arg, fmt::format_context &ctx) {
          const auto &[_, index] = _arg;
          using namespace std::string_view_literals;
          // ad hoc: manual delimiter (need `sliced` implementation)
          if (0 == index)
            return fmt::format_to(ctx.out(), "");
          if (1 == index)
            return fmt::format_to(ctx.out(), "_{}", index);
          return fmt::format_to(ctx.out(), ", _{}", index);
        })));
};

} // namespace

tl::expected<codegen::target_mode_reflection_data, std::string>
  codegen::prepare_input(tool::refl::context ctx,
    tool::cli::target_mode mode) noexcept {
  tl::expected<target_mode_reflection_data, std::string> r{tl::in_place};

  r->includes = {std::make_move_iterator(ctx.std_includes.begin()),
    std::make_move_iterator(ctx.std_includes.end())};

  for (const auto &[nm_qual_type, fields] : ctx.reflected_types) {
    const auto _definition = ctx.definitions.find(nm_qual_type);
    if (_definition == ctx.definitions.cend())
      return tl::unexpected(
        fmt::format("no definition for reflected type {}", nm_qual_type));

    const auto &[_, definition] = *_definition;
    // fixme:
    // if (std::holds_alternative<tool::cli::target_mode>(mode)) {
    //   using td_flags = tool::refl::type_definition_flags;
    //   // todo: consider validating the flags beforehand, so all the info can
    //   be
    //   // reported at once.
    //   if (td_flags::local & definition.definition_flags)
    //     // todo: additional info
    //     return tl::unexpected(
    //       fmt::format("local types are not supported in target mode: {}",
    //         nm_qual_type));

    //   if (::is_cpp_file(definition.source_file)) {
    //     return tl::unexpected(fmt::format(
    //       "types defined in .cpp are not supported in target mode: {}",
    //       nm_qual_type));
    //   }
    // }

    r->refl_includes.emplace(definition.source_file);
    // r->reflected_types.insert({
    //   .name = nm_qual_type,
    //   .field_names = //
    //   // refactorme: use util transform
    //   [](const std::vector<tool::refl::struct_field_data> &fields)
    //     -> std::vector<std::string> {
    //     std::vector<std::string> r;
    //     r.reserve(fields.size());
    //     std::transform(fields.cbegin(),
    //       fields.cend(),
    //       std::back_inserter(r),
    //       [](const tool::refl::struct_field_data &fd) { return fd.name; });
    //     return r;
    //   }(fields),
    //   .is_class = definition.is_class,
    // });
  }

  for (const auto &nm_qual_type : ctx.reflected_implementations) {
    const auto definition = ctx.definitions.find(nm_qual_type);
    if (definition == ctx.definitions.cend())
      return tl::unexpected(
        fmt::format("no definition for reflected implementation type {}",
          nm_qual_type));
    const auto &sf = definition->second.source_file;
    if (!r->refl_includes.contains(sf))
      r->refl_impl_includes.emplace(sf.string());
  }

  std::set unique_func_signatures{
    std::make_move_iterator(ctx.reflected_calls.begin()),
    std::make_move_iterator(ctx.reflected_calls.end()),
    [](const tool::refl::func_signature &lhs,
      const tool::refl::func_signature &rhs) -> bool {
      // because c++ std is UGLEEEEEEE
      return std::lexicographical_compare(lhs.args.cbegin(),
        lhs.args.cend(),
        rhs.args.cbegin(),
        rhs.args.cend(),
        [](const tool::refl::function_signature_arg &lhs,
          const tool::refl::function_signature_arg &rhs) -> bool {
          return std::tie(lhs.nm_qual_type, lhs.is_const, lhs.ref_type)
            < std::tie(rhs.nm_qual_type, rhs.is_const, rhs.ref_type);
        });
    }};

  r->reflected_calls = {std::make_move_iterator(unique_func_signatures.begin()),
    std::make_move_iterator(unique_func_signatures.end())};

  return r;
}

// todo: proper implementation for offset formatting of elements in 'join'
// todo: remove as many included headers as possible
// todo: optionally print the omnirefl command with args that was used

// todo: consider using forward declarations instead of includes
// for instance, a reflection data struct can be a template, thus avoiding
// the need for header inclusion until instantiations
/*
 ```
 template <typename T, typename = T>
 struct reflected_t;

 namespace user_ns {
 class user_type;
 }

 template <typename T>
 struct reflected_t<user_ns::user_type, T> {
  static_assert(std::is_same<user_ns::user_type, T>::value, "");
  using type = T;

  // [](const type& t) { return t.field; }
 };

 // if detected
 template <>
 struct reflected_t<_type_index<0>> : reflected_t<user_ns::user_type> {};
 ```
 */

tl::expected<void, std::string> codegen::emit_reflection_cpp_file(options,
  std::ostream &os,
  const target_mode_reflection_data &data) {
  using namespace std::string_view_literals;
  os << fmt::format(
    R"(// This file has been generated by omnirefl tool (todo: timestamp, data, etc).
// Do not modify this file manually.

// Headers of reflected types
{headers_reflected_types}

// Headers of reflected implementations
{headers_reflected_implementations}

// Other headers
{other_headers}

#include <omnirefl/refl.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

namespace omni {{
namespace detail {{
namespace {{

#ifndef OMNI_DEFINE_NAME_FUNC
// macro for static member function definition to
// to reduce boilerplate ... in a boilerplate
#  define OMNI_DEFINE_NAME_FUNC(STR) \
constexpr static auto name() noexcept -> const char(&)[sizeof(STR)] {{ return STR; }}
#endif

// Generated specializations for types' reflection
{reflected_types_specializations}

}} // namespace
}} // namespace detail
}} // namespace omni

// Generated implementations for reflected calls
{reflected_calls}

// end)",
    fmt::arg("headers_reflected_types",
      util::join(data.refl_includes, "\n", ::fmt_include)),
    fmt::arg("headers_reflected_implementations",
      util::join(data.refl_impl_includes, "\n", ::fmt_include)),
    fmt::arg("other_headers", util::join(data.includes, "\n", ::fmt_include)),
    fmt::arg("reflected_types_specializations",
      util::join(data.reflected_types, "\n\n", ::fmt_reflected_specialization)),
    fmt::arg("reflected_calls",
      util::join(data.reflected_calls, "\n\n", ::fmt_reflected_call))
    //
  );

  return {};
}

/*
 * todo:
 // the types below are generated

// todo: what about templates and template specializations???
// partial specializations depend on particular types
// let's just use includes for now
namespace user_ns {
    struct user_str;
}

// first - list of (named) non-local reflected types (reflected recursively)
template <typename T>
struct reflected_t<user_ns::user_str, T> {
    static_assert(std::is_same<user_ns::user_str, T>::value, "");
    using type = T;
    struct name_t {
        constexpr static auto get_value(const type &v) noexcept ->
decltype(v.name) { return v.name;
        }
    } constexpr static name{};
};

template <typename T>
struct fetch_reflected<user_ns::user_str, T> {
    using type = reflected_t<T>;
};

// second - list of indexed types (reflected via reflected_call)
// uncommented for check
// template <typename T>
// struct reflected_t<indexed_type<4>, T> : reflected_t<typename T::type> {};

template <typename _T>
struct reflected_t<indexed_type<5>, _T> {
    struct age_t {
        template <typename T>
        constexpr static auto get_value(const T &v) noexcept -> decltype(v.age)
{ return v.age;
        }
    } constexpr static age{};
};
*/

tl::expected<void, std::string> codegen::emit_inplace_reflection_header_file(
  options,
  std::ostream &os,
  const inplace_mode_reflection_data &data) {
  using namespace std::string_view_literals;

  // todo: if/when needed
  // // Headers of reflected types
  // {headers_reflected_types}

  os << fmt::format(
    R"(// This header has been generated by omnirefl tool (todo: timestamp, data, etc).
// Do not modify this file manually.

// Forward declarations of reflected types
{forward_declarations}


// todo: 
//   these headers might show as unused.
//   consider writing own implementations instead of including these
#include <tuple>
#include <type_traits>
#include <utility>

namespace omni {{

template <typename, typename>
struct reflected_binding;

namespace detail {{
namespace {{

template <int>
struct _indexed_reflected;

template <typename T, typename>
struct _reflected;

// macro for static member function definition to
// to reduce boilerplate ... in a boilerplate
#  define OMNI_DEFINE_NAME_FUNC(STR) \
constexpr static auto name() noexcept -> const char(&)[sizeof(STR)] {{ return STR; }}

// Generated reflection specializations for types
{reflected_types_specializations}

// Generated reflection specializations for indexed types
{indexed_types_specializations}

}} // namespace
}} // namespace detail
}} // namespace omni

#define OMNI_INCLUDED_GENERATED_REFLECTION_HEADER
)",
    fmt::arg("forward_declarations",
      util::join(data.reflected_types, "\n\n", ::fmt_forward_declaration)),
    // todo: if/when needed
    // fmt::arg("headers_reflected_types",
    //   util::join(data.refl_includes, "\n", ::fmt_include)),
    fmt::arg("reflected_types_specializations",
      util::join(data.reflected_types, "\n\n", ::fmt_reflected_specialization)),
    fmt::arg("indexed_types_specializations",
      util::join(data.reflected_indexed_types,
        "\n\n",
        [](const auto &key_val, fmt::context &ctx) {
          const auto &[index, fields] = key_val;
          return ::fmt_indexed_reflected_specialization(index, fields, ctx);
        })));

  return {};
}
