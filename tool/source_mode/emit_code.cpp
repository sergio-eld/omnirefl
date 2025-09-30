
#include "tool/source_mode/transforms.h"
#include "tool/util.hpp"

#include <fmt/core.h>

#include <expected>
#include <string_view>

namespace {
using namespace tool::source_mode;

auto fmt_include(const std::filesystem::path &p, fmt::context &ctx) noexcept {
  return fmt::format_to(ctx.out(),
    "#include \"{path}\"",
    fmt::arg("path", p.string()));
};

auto fmt_std_include(const std::filesystem::path &p,
  fmt::context &ctx) noexcept {
  return fmt::format_to(ctx.out(),
    "#include <{path}>",
    fmt::arg("path", p.string()));
};

auto fmt_reflected_field(const tool::refl::struct_field_data &field,
  fmt::context &ctx) {
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

auto fmt_reflected_specialization(
  const codegen::reflected_specialization_data &refl_type,
  fmt::context &ctx) noexcept {
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
    fmt::arg("type_name", refl_type.nm_qual_type),
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

auto fmt_is_reflected_specialization(
  const codegen::reflected_specialization_data &refl_type,
  fmt::context &ctx) {
  using namespace std::string_view_literals;

  return fmt::format_to(ctx.out(),
    R"(template <typename T>
struct _is_reflected<{type_tag} {type_name}, T> : std::true_type {{
  static_assert(std::is_same<{type_tag} {type_name}, T>::value,
    "omnirefl: unexpected types mismatch, try regenerating");
}};)",
    fmt::arg("type_tag", refl_type.is_class ? "class"sv : "struct"sv),
    fmt::arg("type_name", refl_type.nm_qual_type));
}

auto fmt_reflected_call(const match::func_signature &func_sig,
  fmt::context &ctx) {
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
        [](const auto &_param, fmt::context &ctx) {
          const auto &[_p, index] = _param;
          const match::function_signature_arg &param = _p;
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
        [](const auto &_arg, fmt::context &ctx) {
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

namespace tool::source_mode {

// todo: proper implementation for offset formatting of elements in 'join'
// todo: remove as many included headers as possible
// todo: optionally print the omnirefl command with args that was used

// todo: consider using forward declarations instead of includes
// for instance, a reflection data struct can be a template, thus avoiding
// the need for header inclusion until instantiations

std::expected<void, std::string> codegen::emit_reflection_cpp_file(options,
  std::ostream &os,
  const reflection_data &data) {
  using namespace std::string_view_literals;
  // ad hoc
  auto std_includes = data.std_includes;
  std_includes.emplace("tuple");
  std_includes.emplace("type_traits");
  std_includes.emplace("utility");

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

// Standard Library headers
{std_headers}

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

{is_reflected_specialization}

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
    fmt::arg("std_headers", util::join(std_includes, "\n", ::fmt_std_include)),
    fmt::arg("reflected_types_specializations",
      util::join(data.reflected_types, "\n\n", ::fmt_reflected_specialization)),
    fmt::arg("is_reflected_specialization",
      util::join(data.reflected_types,
        "\n\n",
        ::fmt_is_reflected_specialization)),
    fmt::arg("reflected_calls",
      util::join(data.reflected_calls, "\n\n", ::fmt_reflected_call))
    //
  );

  return {};
}

} // namespace tool::source_mode
