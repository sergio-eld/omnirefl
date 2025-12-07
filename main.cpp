
#pragma push_macro("_FORTIFY_SOURCE")
#ifdef _FORTIFY_SOURCE
#  undef _FORTIFY_SOURCE
#endif
#define _FORTIFY_SOURCE 0

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTTypeTraits.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchersInternal.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Job.h>
#include <clang/Driver/Options.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Option/Arg.h>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/Option.h>
#include <llvm/TargetParser/Host.h>
#pragma GCC diagnostic pop

#pragma pop_macro("_FORTIFY_SOURCE")

#include <CLI/CLI.hpp>
#include <CLI/Error.hpp>
#include <CLI/Validators.hpp>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace util {

template <typename T>
concept string_view_like = requires(const T &v) {
  { v.data() } -> std::convertible_to<const char *>;
  { v.size() } -> std::convertible_to<std::size_t>;
};

template <string_view_like T>
constexpr auto to_string_view(const T &v) noexcept {
  return std::basic_string_view{v.data(), v.size()};
}

// fixme: I think this is not entirely correct
template <std::ranges::range R>
constexpr auto indexed(R &&r, size_t start = 0) {
  return std::views::zip(std::views::iota(start), std::forward<R>(r));
}

struct concat_t {
  template <typename T, typename... TT>
  std::vector<T> operator()(std::vector<T> lhs, std::vector<TT>... rhs) const {
    size_t rhs_size = 0;
    ((rhs_size += rhs.size()), ...);
    lhs.reserve(lhs.size() + rhs_size);
    (std::move(rhs.begin(), rhs.end(), std::back_inserter(lhs)), ...);
    return lhs;
  }
} constexpr const concat{};

// todo: extend formatting context (padding, what else?)
// todo: do I need a way to combine the adaptors? each of them should serve
// (exactly?) one purpose:
// - custom format
// - for range:
//   - element format (achievable now via std::views)
//   - joining with delim (implemented by join)
//   - ???
template <typename T, typename Formatter>
struct use_fmt {
  const T &value;
  Formatter fmt;
};

// refactorme:
// consider this api:
// util::joined{ .rng = includes, .fmt = "#include <{}>", .delim = '\n', }
// .fmt defaults to "{}"

template <std::ranges::viewable_range R, typename Formatter>
struct joined {
  // fixme: for some reason I need to do `| std::views::all` for regular
  // containers
  const R &rng;
  Formatter delim; //< (?) compile-time string or lambda
};

} // namespace util

template <typename T, typename Formatter>
struct std::formatter<util::use_fmt<T, Formatter>> {
  /// holds parsed {:specs} for strings – width, alignment, etc.
  mutable std::formatter<std::string_view> _base;

  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) {
    /// delegate parsing of {:specs} to the string formatter
    return _base.parse(ctx);
  }

  template <typename FormatContext>
    requires std::invocable<Formatter, const T &, FormatContext &>
  constexpr auto format(const util::use_fmt<T, Formatter> &wrapped,
    FormatContext &ctx) const {
    return wrapped.fmt(wrapped.value, ctx);
  }

  template <typename FormatContext>
    requires std::invocable<Formatter, const T &>
    && std::convertible_to<std::invoke_result_t<Formatter, const T &>,
      std::basic_string_view<typename FormatContext::char_type>>
  constexpr auto format(const util::use_fmt<T, Formatter> &wrapped,
    FormatContext &ctx) const {
    return _base.format(wrapped.fmt(wrapped.value), ctx);
  }
};

template <std::ranges::view R, typename S>
struct std::formatter<util::joined<R, S>> {
  mutable std::formatter<std::string_view> _base;

  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) {
    return _base.parse(ctx);
  }

  template <typename FormatContext>
  constexpr auto format(const util::joined<R, S> &j, FormatContext &ctx) const {
    std::string result;

    for (const auto &[idx, val] : util::indexed(j.rng)) {
      (0 == idx)
        ? std::format_to(std::back_inserter(result), "{}", val)
        : std::format_to(std::back_inserter(result), "{}{}", j.delim, val);
    }

    return _base.format(result, ctx);
  }
};

namespace fs = std::filesystem;

namespace {

struct source_file {
  fs::path path;
  std::vector<std::string> flags;
};

namespace cli {

template <typename T>
constexpr auto str_by = {};

template <typename T>
constexpr std::expected<T, std::string> from_string(std::string_view v) {
  if (const auto &found = std::ranges::find_if(str_by<T>,
        [v](const auto &key_val) { return v == key_val.first; });
    found != str_by<T>.cend())
    return found->second;

  return std::unexpected(std::format("Invalid enum value '{}'", v));
}

template <typename T>
  requires std::is_enum_v<T>
constexpr std::string_view to_string(const T &v) {
  if (const auto &found = std::ranges::find_if(str_by<T>,
        [v](const auto &keyValue) { return v == keyValue.second; });
    str_by<T>.cend() != found)
    return found->first;
  return "unknown";
}

/// compilation flags
struct cl_flags {
  std::vector<std::string> values;
};

/// compile_commands.json
struct json_cl_db {
  fs::path path;
};

struct cli_source_file {
  fs::path path;

  /// used to deduplicate if compilation_db contains several commands for one
  /// file
  std::string output_substr;
};

/// CLI11 uses ADL to specialize parsing cli args to custom types
bool lexical_cast(const std::string &arg, cli_source_file &src) {
  std::string_view sv = arg;
  if (sv.empty())
    return false;

  // case 1: unquoted (simpler) – handle first.
  if ('"' != sv.front()) {
    auto v_parts = sv | std::views::split(':')
      | std::views::transform(
        [](auto &&subrng) { return std::string_view(subrng); });

    const std::vector parts(v_parts.begin(), v_parts.end());

    // Valid only if 1 or 2 parts.
    if (parts.empty() || 2 < parts.size())
      return false;

    // Path before ':' must not be empty.
    if (parts.front().empty())
      return false;

    src.path = fs::path(parts.front());
    src.output_substr =
      (2 == parts.size()) ? std::string(parts.back()) : std::string{};
    return true;
  }

  // case 2: starts with '"' → quoted logic.
  std::ranges::view auto view = sv | std::views::split('"')
    | std::views::transform(
      [](auto &&subrng) { return std::string_view(subrng); })
    | std::views::filter([](std::string_view s) { return !s.empty(); });

  const std::vector parts(view.begin(), view.end());

  if (parts.empty() || 3 < parts.size())
    return false;

  if (1 == parts.size()) {
    // `"path"`
    // Reject empty quoted path: ""...
    if (2 <= sv.size() && '"' == sv[1])
      return false;

    src.path = fs::path(parts[0]);
    src.output_substr.clear();
    return true;
  }

  if (2 == parts.size()) {
    // `"path":out` -> [path, :out]
    const auto &p0 = parts[0];
    const auto &p1 = parts[1];

    if (p0.empty())
      return false;

    if (p1.empty())
      return false;

    if (':' != p1.front())
      return false;

    // second "path" (after ':') must not be empty
    if (1 == p1.size())
      return false;

    src.path = fs::path(p0);
    src.output_substr.assign(p1.substr(1)); // after ':'
    return true;
  }

  // 3 == parts.size(): `"path":"out"` -> [path, :, out]
  const auto &p0 = parts[0];
  const auto &p1 = parts[1];
  const auto &p2 = parts[2];

  if (p0.empty() || p2.empty())
    return false;

  if (!(1 == p1.size() && ':' == p1.front()))
    return false;

  src.path = fs::path(p0);
  src.output_substr.assign(p2);
  return true;
}

// parsed cli args (in correct state)
struct options {
  // .cpp sources
  std::vector<source_file> sources;

  // source mode: path for output file (may include the file name)
  // header mode: output dir
  fs::path out;

  fs::path resource_dir;

  enum mode_t {
    header,
    source,
    dump,
  } mode;
};

template <>
constexpr std::array str_by<options::mode_t> = std::invoke([] {
  using namespace std::string_view_literals;

  auto map = std::array{
    std::pair{"header"sv, options::header},
    std::pair{"source"sv, options::source},
    std::pair{"dump"sv, options::dump},
  };
  std::ranges::sort(map,
    [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });
  return map;
});

std::string format_options(const options &o) {
  // todo: consider printing flags
  return std::format(
    "mode:{}"
    "\nsources:[{}]"
    "\nout:{}"
    "\nresource_dir:{}",
    // 0:
    to_string(o.mode),

    // 1:
    util::joined{
      .rng = o.sources | std::views::transform([](const source_file &s) {
        return s.path.generic_string();
      }),
      .delim = ",\n",
    },

    // 2:
    o.out.generic_string(),

    // 3:
    o.resource_dir.generic_string());
}

std::expected<options, std::pair<int, std::string>> parse(int argc,
  const char *const *argv) noexcept;

} // namespace cli

/// configure ast-related compiler invocation parameters
struct compiler_invocation_from_args_t {
  // note: struct is used to hide `args` from global scope
  struct args {
    const fs::path &resource_dir;
    clang::DiagnosticsEngine &diag;
    const llvm::IntrusiveRefCntPtr<clang::FileManager> &file_manager;
  };

  std::expected<std::shared_ptr<clang::CompilerInvocation>, std::string>
    operator()(const source_file &sf, args a) const noexcept;
} constexpr const compiler_invocation_from_args{};

// todo: this is a candidate for reusability (in a separate header)
namespace ast {

template <typename Match, typename Node>
struct pattern_t {
  std::type_identity<Node> node_type;
  Match match;
};

template <typename Node, typename... Filter>
constexpr auto pattern(
  const clang::ast_matchers::internal::VariadicAllOfMatcher<Node> &m,
  Filter... f) {
  return pattern_t{
    .node_type = std::type_identity<Node>{},
    .match = m(f...),
  };
}

template <typename Node, typename Base, typename... Filter>
constexpr auto pattern(
  const clang::ast_matchers::internal::VariadicDynCastAllOfMatcher<Base, Node>
    &m,
  Filter... f) {
  return pattern_t{
    .node_type = std::type_identity<Node>{},
    .match = m(f...),
  };
}

template <typename Match, typename Node, typename Reduce>
struct rule {
  pattern_t<Match, Node> pattern;
  Reduce reduce;
  clang::TraversalKind traversal_kind = clang::TraversalKind::TK_AsIs;
};

// todo: use concept `of_template<rule>`
template <typename Accum, typename... Rule>
Accum reduce_matches(clang::ASTUnit &ast, //< for some reason MatchFinder needs
                                          // a mutable ASTContext&. SMH...
  Accum a,
  Rule... rule) {
  using namespace clang::ast_matchers;
  constexpr std::string_view binding_tag = "binding_tag";

  constexpr auto callback_from_rule = //
    [binding_tag]<typename M, typename N, typename R>(Accum &accum,
      const ast::rule<M, N, R> &rule) {
      const auto bound_matcher =
        traverse(rule.traversal_kind, rule.pattern.match.bind(binding_tag));
      using matcher_t = decltype(bound_matcher);

      struct _callback: MatchFinder::MatchCallback {
        matcher_t matcher;
        Accum &accum;
        R reduce;

        std::string_view binding_tag;

        void run(const MatchFinder::MatchResult &result) override {
          const std::map nodeById = result.Nodes.getMap();
          const auto found = nodeById.find(binding_tag);
          if (nodeById.end() == found) {
            std::cerr << "DEBUG: Match::Callback false-fired.\n";
            return;
          }

          const auto *node = found->second.get<N>();
          if (!node) {
            std::cerr
              << "DEBUG: Matc::Callback: Node of invalid type matched.\n";
            return;
          }

          accum = reduce(std::move(accum), *node);
        }

        _callback(const matcher_t &m,
          Accum &a,
          const R &r,
          std::string_view binding_tag)
            : matcher(m)
            , accum(a)
            , reduce(r)
            , binding_tag(binding_tag) {}
      };

      return _callback(bound_matcher, accum, rule.reduce, binding_tag);
    };

  // MatchFinder doesn't own the callbacks, so they need to outlive the finder.
  std::tuple callbacks{callback_from_rule(a, rule)...};

  MatchFinder finder;
  std::apply(
    [&finder](auto &...callback) {
      (finder.addMatcher(callback.matcher, &callback), ...);
    },
    callbacks);
  finder.matchAST(ast.getASTContext());

  return a;
}

} // namespace ast

namespace meta {

// todo: benchmark, then use hash unique type identifier
using type_id = std::string;

struct nm_qual_type {
  /// nullopt for unnamed types
  std::optional<std::string> name;

  /// chain of namespaces. may start with empty string if declared in anonymous
  /// namespace
  std::vector<std::string> namespaces;

  /// non-empty for nested types `struct foo { struct bar{}; };`
  std::vector<std::string> enclosing_records;
};

enum reference_type {
  ref_none,
  ref_lval,
  ref_rval,
};

struct source_location {
  std::filesystem::path source_file;
  std::uint32_t line;
  std::uint32_t column;
};

// refactorme: source_file and location should be separated into
// `struct source_location`
struct type_definition {
  nm_qual_type type_name;
  source_location location;

  enum {
    none = 0x0,

    /// in global scope is not visible as public
    non_public = 0x1 << 0,

    /// definition within a scope
    local = 0x1 << 1,
  } definition_flags;
};

struct struct_data {
  /// protected and private are not supported now (too complicated)
  std::vector<std::string> public_fields;

  enum {
    is_struct,
    is_class,
    is_union,
  } type;
};

struct enum_data {
  bool is_scoped; //< true if 'enum class { }`
  std::vector<std::string> enumerators;
};

struct function_signature_arg {
  // fully namespace-qualified type
  nm_qual_type type_name;
  bool is_const;
  reference_type ref_type;
};

struct func_signature {
  std::vector<function_signature_arg> args;
  function_signature_arg return_type;

  // MISSING:
  // - pointer depth (T*, T**)
  // - qualifiers on pointee (const T*)
  // - arrays / function pointers / etc.
};

struct reflectable {
  type_id id;

  std::variant<struct_data, enum_data> data;
  type_definition definition;
};

struct already_reflected {
  type_id id;
};

// todo: store the type info, since only a limited set of non_relfectable types
// are supported, i.e.: std::tuple<A, B, C>, std::variant<A, B, C>,
// std::vector<A>, ...
struct non_reflectable {
  type_id id;
};

// refactorme: better name, now it differs in only one letter with the function
struct reflected_type_info {
  std::variant<reflectable, non_reflectable, already_reflected> type;
  std::vector<reflectable> dependencies;
};

std::expected<reflected_type_info, std::string> resolve_reflected_type(
  const clang::ClassTemplateSpecializationDecl &decl,
  const clang::ASTUnit &ast,
  const std::set<type_id> &resolved_types);

struct reflected_call_info {
  func_signature f_sig;
  std::set<fs::path> includes;
  std::set<fs::path> std_includes;

  // source_location call_location; //< fixme: with current matching rule, the
  // actual call site can't be determined. CXXMethodDecl is matched, which is
  // the instantiation, not the call expression...
};

std::expected<reflected_call_info, std::string> resolve_reflected_call(
  const clang::CXXMethodDecl &decl,
  const clang::ASTUnit &ast);

} // namespace meta

namespace render {
// refactorme:
// move help funcitoins below, leave only the call signatures for 'main' result
// is valid for reflection rendering only for named types

auto format_location(const meta::source_location &d) {
  return std::format("{}:{}:{}",
    d.source_file.generic_string(),
    d.line,
    d.column);
}

std::string format_nm_qual_type(const meta::nm_qual_type &t) {
  // refactorme: implement concat as view
  const std::vector elems = util::concat(t.namespaces,
    t.enclosing_records,
    std::vector{t.name.value_or("(unnamed)")});

  return std::format("{}",
    util::joined{
      .rng = elems | std::views::all,
      .delim = "::",
    });
}

std::string_view ref_suffix(meta::reference_type r) {
  switch (r) {
  case meta::ref_lval:
    return "&";
  case meta::ref_rval:
    return "&&";
  default:
    return "";
  }
}

std::string format_arg(const meta::function_signature_arg &a) {
  std::string s;

  if (a.is_const)
    s += "const ";

  s += format_nm_qual_type(a.type_name);
  s += ref_suffix(a.ref_type);

  return s;
}

struct log {
  const std::set<meta::type_id> &dependencies;

  static std::string flags(int flags) {
    // todo: return std::format + util::joined
    std::string out;

    if (flags & meta::type_definition::non_public) {
      if (!out.empty())
        out += " | ";
      out += "non_public";
    }

    if (flags & meta::type_definition::local) {
      if (!out.empty())
        out += " | ";
      out += "local";
    }

    return out;
  }

  static std::string format_signature(const meta::func_signature &f) {
    return std::format(
      "({})"
      "\n    -> {}",
      util::joined{
        .rng = f.args | std::views::transform(format_arg),
        .delim = ",\n    ",
      },
      format_arg(f.return_type));
  }

  static auto format_definition(const meta::type_definition &d) {
    return std::format(
      "declared at: {}{}"
      "\n  {}",
      format_location(d.location),
      std::invoke([f = flags(d.definition_flags)] {
        return f.empty() ? std::format("") : std::format("\n  [{}]", f);
      }),

      format_nm_qual_type(d.type_name));
  }

  auto operator()(const meta::struct_data &d) const {
    const char *kind = d.type == meta::struct_data::is_struct //
      ? "struct"
      : d.type == meta::struct_data::is_class //
        ? "class"
        : "union";

    if (d.public_fields.empty())
      return std::format("{} {{}}", kind);

    std::string fields;
    for (std::size_t i = 0; i < d.public_fields.size(); ++i) {
      if (i > 0)
        fields += ", ";
      fields += d.public_fields[i];
    }

    return std::format("{} {{ {} }}", kind, fields);
  }

  auto operator()(const meta::enum_data &d) const {
    const std::string_view kind = d.is_scoped //
      ? "enum class"
      : "enum";

    if (d.enumerators.empty())
      return std::format("{} {{}}", kind);

    std::string enums;
    for (std::size_t i = 0; i < d.enumerators.size(); ++i) {
      if (i > 0)
        enums += ", ";
      enums += d.enumerators[i];
    }

    return std::format("{} {{ {} }}", kind, enums);
  }

  auto operator()(const meta::reflectable &d) const {
    return std::format(
      "reflected type{}:"
      "\n  {}"
      "\n  {}",
      (dependencies.contains(d.id) ? " (as dependency)" : ""),
      format_definition(d.definition),
      std::visit(*this, d.data));
  }

  auto operator()(const meta::reflected_call_info &d) const {
    std::string includes;
    if (d.includes.empty()) {
      includes = "none";
    } else {
      for (auto it = d.includes.begin(); it != d.includes.end(); ++it) {
        if (it != d.includes.begin())
          includes += ", ";
        includes += it->generic_string();
      }
    }

    std::string std_includes;
    if (d.std_includes.empty()) {
      std_includes = "none";
    } else {
      for (auto it = d.std_includes.begin(); it != d.std_includes.end(); ++it) {
        if (it != d.std_includes.begin())
          std_includes += ", ";
        // assume these are header names (no <> here, just raw stem/path)
        std_includes += it->generic_string();
      }
    }

    return std::format(
      "reflected call:"
      // "\n  called at: {}"
      "\n  signature: {}"
      "\n  includes: [{}]"
      "\n  std includes: [{}]",

      // fixme: can't know call_location now
      // format_location(d.call_location),
      format_signature(d.f_sig),
      includes,
      std_includes);
  }
};

struct reflection {
  const std::set<meta::type_id> &dependencies;

  static auto format_enum(const meta::nm_qual_type &t,
    const meta::enum_data &d) noexcept {
    assert(t.name && "format_enum expects named, non-anonymous enum");

    return std::format(
      "template <typename T>"
      "\nstruct _reflected<enum {0}, T> {{"
      "\n  static_assert(std::is_same<enum {0}, T>::value,"
      "\n    \"omnirefl: unexpected types mismatch, try regenerating\");"
      "\n"
      "\n  using type = T;"
      "\n"
      "\n  static constexpr omni::reflected_entity entity() noexcept {{"
      "\n    return omni::reflected_entity::enumeration;"
      "\n  }}"
      "\n"
      "\n  constexpr static auto name() noexcept"
      "\n    -> const char(&)[sizeof(\"{1}\")] {{"
      "\n    return \"{1}\";"
      "\n  }}"
      "\n"
      "\n  constexpr static auto enumerators() noexcept"
      "\n    -> std::array<std::pair<type, const char*>, {2}> {{"
      "\n      return {{{{"
      "\n        {3},"
      "\n      }}}};"
      "\n    }}"
      "\n}};",

      // 0:
      format_nm_qual_type(t),

      // 1:
      *t.name,

      // 2:
      d.enumerators.size(),

      // 3:
      util::joined{
        .rng = d.enumerators | std::views::transform([](std::string_view e) {
          return std::format("{{type::{0}, \"{0}\"}}", e);
        }),
        .delim = ",\n        ",
      });
  }

  static auto format_struct(const meta::nm_qual_type &t,
    const meta::struct_data &d) noexcept -> std::string {
    assert(t.name && "format_struct expects named, non-anonymous type");

    // todo:
    // - `.fields` -> returns all the fields
    // - `.mutable_fields` -> returns only mutable fields
    return std::format(
      "template <typename T>"
      "\nstruct _reflected<{0} {1}, T> {{"
      "\n  static_assert(std::is_same<{0} {1}, T>::value,"
      "\n    \"omnirefl: unexpected types mismatch, try regenerating\");"
      "\n"
      "\n  using type = T;"
      "\n"
      "\n  static constexpr omni::reflected_entity entity() noexcept {{"
      "\n    return omni::reflected_entity::tagged;"
      "\n  }}"
      "\n"
      "\n  static constexpr auto name() noexcept"
      "\n    -> const char(&)[sizeof(\"{2}\")] {{"
      "\n    return \"{2}\";"
      "\n  }}"
      "\n"
      "{3}"
      "\n"
      "\n  using fields_t ="
      "\n    std::tuple<{4}>;"
      "\n"
      "\n  static constexpr fields_t fields() noexcept {{ return {{}}; }}"
      "\n}};",

      // 0:
      d.type == meta::struct_data::is_class //
        ? "class"
        : d.type == meta::struct_data::is_union //
          ? "union"
          : "struct",

      // 1:
      format_nm_qual_type(t),

      // 2:
      *t.name,

      // 3:
      d.public_fields.empty()
        ? std::string("\n  // no reflectable fields detected")
        : std::format("\n{}",
            util::joined{
              .rng = util::indexed(d.public_fields)
                | std::views::transform([](const auto &p) {
                    const auto &[idx, name] = p;
                    return std::format(
                      "  struct {0}_t {{"
                      "\n    static constexpr omni::reflected_entity entity() noexcept {{ "
                      "\n      return omni::reflected_entity::member;"
                      "\n    }}"
                      "\n"
                      "\n    static constexpr std::size_t index() noexcept {{ return {1}; }}"
                      "\n"
                      "\n    static constexpr auto name() noexcept"
                      "\n      -> const char(&)[sizeof(\"{0}\")] {{"
                      "\n      return \"{0}\";"
                      "\n    }}"
                      "\n"
                      "\n    template <typename _T>"
                      "\n    static constexpr auto value(const _T &t) noexcept"
                      "\n      -> const decltype(t.{0})& {{"
                      "\n      return t.{0};"
                      "\n    }}"
                      "\n"
                      "\n    template <typename _T, typename V>"
                      "\n    static constexpr void set_value(_T &t, V &&v) {{"
                      "\n      t.{0} = std::forward<V>(v);"
                      "\n    }}"
                      "\n  }};",
                      name,
                      idx);
                  }),
              .delim = "\n\n",
            }),

      // 4:
      util::joined{
        .rng = d.public_fields | std::views::transform([](std::string_view f) {
          return std::format("{}_t", f);
        }),
        .delim = ",\n      ",
      });
  }

  static std::string format_reflected_call(
    const meta::reflected_call_info &c) noexcept {
    // fixme: doesn't work with util::joined{.rnd = indexed | filter | transform
    std::vector<std::string> args_to_call;
    std::ranges::transform(util::indexed(c.f_sig.args)
        | std::views::filter([](const auto &p) {
            const auto &[idx, _] = p;
            return 0 != idx; //< skip Impl
          }),
      std::back_inserter(args_to_call),
      [](const auto &p) {
        const auto &[idx, arg] = p;
        return !arg.is_const && meta::reference_type::ref_rval == arg.ref_type
          ? std::format("std::move(_{})", idx)
          : std::format("_{}", idx);
      });

    return std::format(
      "template <>"
      "\nauto omni::reflected_call_t::_call_impl("
      "\n  {0})"
      "\n  -> decltype(_impl({1})) {{"
      "\n  return _impl({1});"
      "\n}}",

      // 0: parameters (Impl + call args)
      util::joined{
        .rng = util::indexed(c.f_sig.args)
          | std::views::transform([](const auto &p) {
              const auto &[idx, arg] = p;
              return std::format("{} {}",
                format_arg(arg),
                0 == idx ? "_impl" : std::format("_{}", idx));
            }),
        .delim = ",\n  ",
      },

      // 1:
      util::joined{
        .rng = args_to_call | std::views::all,
        .delim = ", ",
      });
  }

  auto operator()(const meta::reflected_call_info &c) const noexcept {
    return format_reflected_call(c);
  }

  auto operator()(const meta::reflectable &r) const noexcept {
    return std::format(
      "// {0}{1}"
      "\n// declared at: {2}"
      "\n{3}",

      // 0:
      dependencies.contains(r.id) //
        ? "(as dependency) "
        : "",

      // 1: refactorme: visit/matching
      std::holds_alternative<meta::struct_data>(r.data) //
        ? "tag type"
        : "enum",

      // 2:
      format_location(r.definition.location),

      // 3:
      std::visit(
        [&tn = r.definition.type_name]<typename Meta>(const Meta &d) {
          if constexpr (std::same_as<meta::struct_data, Meta>)
            return format_struct(tn, d);
          else {
            static_assert(std::same_as<meta::enum_data, Meta>);
            return format_enum(tn, d);
          }
        },
        r.data));
  }
};

// todo: yaml dump

} // namespace render

} // namespace

int main(int argc, char **argv) {
  const std::expected cli_args = cli::parse(argc, argv);
  if (!cli_args) {
    const auto &[code, error] = cli_args.error();
    std::cerr << error << '\n';
    return code;
  }

  std::cout << std::format(
    "\nargs:"
    "\n{}",
    format_options(*cli_args));

  const llvm::IntrusiveRefCntPtr file_manager = new clang::FileManager({
    .WorkingDir = fs::current_path().parent_path().generic_string(),
  });

  struct context {
    std::vector<meta::reflectable> reflected;
    std::vector<meta::reflected_call_info> calls;

    std::set<meta::type_id> resolved_types;
    std::set<meta::type_id> resolved_as_dependent;
    std::vector<std::string> errors;

    // to generate deps file for cmake, so it can rerun the tool upon changes to
    // those headers
    std::set<fs::path> includes_deps;
  };

  // refactorme: `struct source_file` already binds fs::path and flags. I should
  // just preprocess the list by filtering and logging conflicts.
  //
  // note: as of now, for 'source mode' contexts are merged to emit a single
  // .cpp, but for 'header mode' a different header file
  std::vector<
    // todo: think about a case when one source is compiled
    // several times with different flags (enabling
    // different things with preprocessor). I should allow
    // _explicitly_ only things I can account for.
    std::pair<fs::path, context>>
    ctx_by_source_file;

  std::cout << '\n';
  // todo: I should map the `sources` into a vector of `context or error`
  // loading the ast
  for (const auto &[n_processing, source_file] :
    util::indexed(cli_args->sources, 1)) {
    // refactorme: cleanup the ast creation

    // note: diag to AST is 1:1, has to be recreated each time...
    // must outlive the ast
    const llvm::IntrusiveRefCntPtr diag = std::invoke([] {
      llvm::IntrusiveRefCntPtr diag_opts = new clang::DiagnosticOptions();
      llvm::IntrusiveRefCntPtr diag =
        new clang::DiagnosticsEngine(new clang::DiagnosticIDs(),
          diag_opts,
          new clang::TextDiagnosticPrinter(llvm::errs(), diag_opts.get()));
      return diag;
    });

    std::cout << std::format("[{}/{}] running {} mode for file: {}\t\r",
      n_processing,
      cli_args->sources.size(),
      cli::to_string(cli_args->mode),
      source_file.path.string());

    std::expected compiler_invocation =
      compiler_invocation_from_args(source_file,
        {
          .resource_dir = cli_args->resource_dir,
          .diag = *diag,
          .file_manager = file_manager,
        });

    if (!compiler_invocation) {
      std::cerr << compiler_invocation.error() << "\n";
      continue;
    }

    // -- todo: this is mode-related, make a map or something
    {
      clang::LangOptions &lo = (*compiler_invocation)->getLangOpts();
      // todo: investigate how to properly parse comments
      lo.CommentOpts.ParseAllComments = true;
    }

    // -- todo: this is mode-related, make a map or something
    {
      clang::PreprocessorOptions &po =
        (*compiler_invocation)->getPreprocessorOpts();

      // fixme: only for header mode
      // constexpr std::string_view k_omni_macro = "OMNI_HEADER_REFLECTION";

      // if (const auto omni_defined = std::ranges::find_if(po.Macros,
      //       [k_omni_macro](const auto &macro_def) {
      //         const auto &[macro, is_undef] = macro_def;
      //         return macro == k_omni_macro;
      //       });
      //   po.Macros.cend() == omni_defined) {
      //   po.Macros.emplace_back(k_omni_macro, /*isUndef*/ false);
      // } else {
      //   auto &[_, is_undef] = *omni_defined;
      //   // todo: warning if `true == is_undef`?
      //   is_undef = false;
      // }

      // fixme:
      // removing force-included reflection header that is being generated
      // p.Includes = util::filtered(
      //   [&output_dir = mode.output_dir](const std::string &s) -> bool {
      //     // todo: should I remove the exact path?
      //     return util::is_subpath(s, output_dir);
      //   },
      //   std::move(p.Includes));

      std::cout << (po.Macros.empty()
          ? std::string()
          : std::format("\n{}",
              util::joined{
                .rng = po.Macros | std::views::transform([](const auto &pair) {
                  const auto &[macro, is_undef] = pair;
                  return std::format("[debug] preprocessor #{} {}",
                    is_undef ? "undef" : "define",
                    macro);
                }),
                .delim = "\n",
              }));

      std::cout << (po.Includes.empty()
          ? std::string()
          : std::format("\n{}",
              util::joined{
                .rng = po.Includes | std::views::transform([](const auto &inc) {
                  return std::format("[debug] preprocessor #include \"{}\"",
                    inc);
                }),
                .delim = "\n",
              }));
    }

    // todo: how about creating the ast context directly (without clang::ASTUnit
    // helper)
    const std::unique_ptr ast =
      clang::ASTUnit::LoadFromCompilerInvocation(*compiler_invocation,
        std::make_shared<clang::PCHContainerOperations>(),
        diag,
        file_manager.get());

    if (!ast || ast->getDiagnostics().hasUncompilableErrorOccurred()) {
      std::cout << std::format("\n[error] Failed to build AST Unit for: {}.\n",
        source_file.path.generic_string());
      continue;
    }

    using namespace clang::ast_matchers;

    std::back_inserter(ctx_by_source_file) = //
      std::pair{
        source_file.path,
        // refactorme: how hard would it be to have an std::views-compatible
        // operations here??
        ast::reduce_matches(*ast,
          context{},

          ast::rule{
            .pattern = ast::pattern(classTemplateSpecializationDecl,
              unless(isInStdNamespace()),
              hasAncestor(namespaceDecl(hasName("omni"))),
              hasAncestor(namespaceDecl(hasName("detail"))),
              hasName("_reflected_type"),
              isTemplateInstantiation(),
              isDefinition()),
            .reduce =
              [&ast = *ast](context a,
                const clang::ClassTemplateSpecializationDecl &decl) {
                std::expected resolved =
                  meta::resolve_reflected_type(decl, ast, a.resolved_types);
                if (!resolved) {
                  a.errors.emplace_back(std::move(resolved).error());
                  return a;
                }

                std::visit(
                  [&a]<typename T>(T t) {
                    if constexpr (std::same_as<meta::reflectable, T>) {
                      meta::reflectable &r = t;
                      a.resolved_types.emplace(r.id);
                      a.reflected.emplace_back(std::move(r));
                    } else if constexpr (std::same_as<meta::non_reflectable,
                                           T>) {
                      // todo:
                      // Only a limited set of T in _relfected_type<T> is
                      // supported, and it should be reported as error
                      // diagnostics
                    } else {
                      static_assert(std::same_as<meta::already_reflected, T>);
                      // todo: log for info?
                    }
                  },
                  std::move(resolved->type));

                std::ranges::transform(resolved->dependencies,
                  std::inserter(a.resolved_as_dependent,
                    a.resolved_as_dependent.end()),
                  [](const meta::reflectable &r) { return r.id; });

                a.reflected = util::concat(std::move(a.reflected),
                  std::move(resolved->dependencies));

                return a;
              },
          },

          // note: `_call_impl` is not needed for header mode, so it is disabled
          // by preprocessor
          ast::rule{
            .pattern = ast::pattern(cxxMethodDecl,
              unless(isInStdNamespace()),
              hasAncestor(namespaceDecl(hasName("omni"))),
              hasAncestor(cxxRecordDecl(hasName("reflected_call_t"))),
              isTemplateInstantiation(),
              hasName("_call_impl")),
            .reduce =
              [&ast = *ast](context a, const clang::CXXMethodDecl &call_decl) {
                std::expected resolved =
                  meta::resolve_reflected_call(call_decl, ast);
                if (!resolved) {
                  a.errors.emplace_back(std::move(resolved).error());
                  return a;
                }
                a.calls.emplace_back(std::move(*resolved));
                return a;
              },
          }),
      };

    // todo: render to &ostream (file/console/whatever OS wants)
    // refactorme: block with conditional logging:
    // - cl1 flags
    // - reflected types
    const context &ctx = ctx_by_source_file.back().second;
    const render::log print_log{.dependencies = ctx.resolved_as_dependent};
    std::cout << std::format(
      "\n\n[info] -- types --------\n{}"
      "\n\n[info] -- calls --------\n{}",
      util::joined{
        .rng = ctx.reflected | std::views::transform(print_log),
        .delim = "\n\n",
      },
      util::joined{
        .rng = ctx.calls | std::views::transform(print_log),
        .delim = "\n\n",
      });
  }

  std::cout << "\n[info] -- generating files --------";

  if (cli::options::source == cli_args->mode) {
    struct render_context {
      std::vector<meta::reflectable> reflected;
      std::vector<meta::reflected_call_info> calls;

      std::set<meta::type_id> dependencies;
    };

    // refactorme: const ctx = ctx_by_source_file | fold_left | sort (need pipe
    // adaptors)
    render_context ctx = std::ranges::fold_left(std::move(ctx_by_source_file),
      render_context{},
      [](render_context a, auto &&key_val) -> render_context {
        auto &&[path, ctx] = key_val;

        // todo: skip errors here, or filter/partition before folding? errors
        // should be conditionally ignorable (user may provide
        // --ignore-errors=N)
        if (!ctx.errors.empty())
          return a;

        a.reflected =
          util::concat(std::move(a.reflected), std::move(ctx.reflected));

        a.calls = util::concat(std::move(a.calls), std::move(ctx.calls));

        // todo: implement later. The logic is somewhat complex. If a type is
        // reflected as dependency in file 1, but not as dependency in file 2,
        // it should not be considered 'a dependency' in the overall shared
        // context. But for information it might be useful to output which .cpp
        // file it was detected and if as a dependency... a.dependencies = {};

        return a;
      });

    // -- Collapse duplicate types --------
    static constexpr auto tie_source_location = [](const meta::reflectable &r) {
      return std::tie(r.definition.location.source_file,
        r.definition.location.line,
        r.definition.location.column);
    };

    std::ranges::sort(ctx.reflected,
      [](const meta::reflectable &lhs, const meta::reflectable &rhs) -> bool {
        // refactorme: if std::variant .data contains more then two types, this
        // must be modified (cleaner)
        static_assert(
          std::same_as<std::variant<meta::struct_data, meta::enum_data>,
            decltype(lhs.data)>);

        if (lhs.data.index() != rhs.data.index()) {
          return std::holds_alternative<meta::enum_data>(lhs.data)
            && !std::holds_alternative<meta::enum_data>(rhs.data);
        }

        return tie_source_location(lhs) < tie_source_location(rhs);
      });

    ctx.reflected = std::ranges::fold_left(std::move(ctx.reflected)
        | std::views::chunk_by( //
          [](const meta::reflectable &lhs,
            const meta::reflectable &rhs) -> bool {
            return tie_source_location(lhs) == tie_source_location(rhs);
          })
        | std::views::transform(
          [](auto group) -> std::expected<meta::reflectable, std::string> {
            // note:
            // this can happen, for example, if 2 different .cpp were
            // including the same header file but with different preprocessor,
            // selecting different type definition for different .cpp

            // todo: check odr based on cli-flag condition
            return *std::ranges::begin(group);
          }),
      std::vector<meta::reflectable>{},
      [](auto a, std::expected<meta::reflectable, std::string> v) {
        if (v) {
          a.emplace_back(std::move(*v));
          return a;
        }
        std::cerr << std::format("[error] ODR violation detected: {}\n",
          v.error());
        return a;
      });

    // -- collapse duplacate calls --------
    static constexpr auto tie_func_arg =
      [](const meta::function_signature_arg &a) {
        return std::tie(a.type_name.namespaces,
          a.type_name.enclosing_records,
          a.type_name.name,
          a.is_const,
          a.ref_type);
      };

    std::ranges::sort(ctx.calls,
      [](const meta::reflected_call_info &lhs,
        const meta::reflected_call_info &rhs) -> bool {
        return std::ranges::lexicographical_compare(lhs.f_sig.args,
          rhs.f_sig.args,
          [](const auto &l, const auto &r) {
            return tie_func_arg(l) < tie_func_arg(r);
          });
      });

    ctx.calls = std::ranges::fold_left(std::move(ctx.calls)
        | std::views::chunk_by( //
          [](const meta::reflected_call_info &lhs,
            const meta::reflected_call_info &rhs) -> bool {
            return std::ranges::equal(lhs.f_sig.args,
              rhs.f_sig.args,
              [](const auto &l, const auto &r) {
                return tie_func_arg(l) == tie_func_arg(r);
              });
          })
        | std::views::transform([](auto group) -> meta::reflected_call_info {
            return *std::ranges::begin(group);
          }),
      std::vector<meta::reflected_call_info>{},
      [](auto a, meta::reflected_call_info v) {
        a.emplace_back(std::move(v));
        return a;
      });

    // !! compare source loacations first!
    // fixme: collapse identical types. Use chunk_by, for each group do a
    // transform: if all the elements are the same, collapse it to one, collect
    // locations. If they are not the same, transform to unexpected with error
    // string referencing locations and what is different: for simplicity is
    // equal if the same namespace + same type + same field names and order.
    // Comparing type of each field is not practical performance-wise

    const std::set includes = std::ranges::fold_left(ctx.calls,
      std::ranges::fold_left(ctx.reflected,
        std::set<fs::path>{},
        [](auto acc, const meta::reflectable &r) {
          acc.insert(r.definition.location.source_file);
          return acc;
        }),
      [](auto acc, const meta::reflected_call_info &c) {
        acc.insert(c.includes.begin(), c.includes.end());
        return acc;
      });

    const std::set std_includes = std::ranges::fold_left(ctx.calls,
      std::set<fs::path>{},
      [](auto acc, const meta::reflected_call_info &c) {
        acc.insert(c.std_includes.begin(), c.std_includes.end());
        return acc;
      });

    const fs::path out = //
      std::invoke([&o = cli_args->out] {
        // ad hoc to disable fs exception throwing
        [[maybe_unused]] std::error_code ec;
        return fs::exists(o, ec) //
          ? fs::is_directory(o, ec) //
            ? fs::path(o) / "source_mode_reflected.cpp"
            : o
          // non-existent path without extension is considered a file
          : o.has_extension() //
            ? o
            : fs::path(o) / "source_mode_reflected.cpp";
      });

    std::error_code ec;
    fs::create_directories(out.parent_path(), ec);
    if (ec) {
      std::cout << std::format(
        "\n[error] {}."
        "\n  invalid output directory: {}",
        ec.message(),
        out.parent_path().generic_string());

      return -1;
    }

    std::cout << std::format("\n[info] creating source mode reflection: {}",
      out.generic_string());

    std::ofstream out_file{out, std::ios::binary};
    if (!out_file) {
      std::cout << std::format("\n[error] failed to open file {} for writing",
        out.generic_string());
      return -1;
    }

    const render::reflection render_reflection{
      .dependencies = ctx.dependencies,
    };

    // todo:
    // for `includes` I may want to remove base path, make it relative to the
    // project dir.
    //
    // todo: do I need to print a list of sources and their flags?
    // todo: write meta
    // todo: write calls

    std::format_to(std::ostreambuf_iterator<char>(out_file),
      "// This file was generated by the omnirefl tool on {0:%F %T}."
      "\n// Do not modify the contents of this file."
      "\n"
      "\n// -- headers --------"
      // refactorme: should be configurable via preprocessor
      "\n#include <omnirefl/reflected_scope.hpp>"
      "\n#include <omnirefl/reflected_call.hpp>" //< todo: do I need it here?
      "\n"
      "\n{1}"
      "\n"
      "\n#include <array>" //< refactorme: ad hoc for enumerators(). No need to
                           // include if no enums
      "\n{2}"
      "\n"
      "\nnamespace omni {{"
      "\nnamespace detail {{"
      "\nnamespace {{"
      "\n"
      "\n// -- reflected types --------"
      "\n{3}"
      "\n"
      "\n}} // namespace"
      "\n}} // namespace detail"
      "\n}} // namespace omni"
      "\n"
      "\ntemplate <typename T, typename>"
      "\nstruct omni::is_reflected : std::false_type {{}};"
      "\n"
      "\ntemplate <typename T>"
      "\nstruct omni::is_reflected<"
      "\n    T,"
      "\n    omni::detail::void_t<typename omni::detail::_reflected<typename std::decay<T>::type>::type>"
      "\n> : std::true_type {{}};"
      "\n"
      "\n// -- reflected calls --------"
      "\n{4}",

      // 0:
      std::invoke([] {
        using namespace std::chrono;
        return floor<seconds>(system_clock::now());
      }),

      // 1:
      util::joined{
        .rng = includes | std::views::transform([](const fs::path &p) {
          return std::format("#include \"{}\"", p.generic_string());
        }),
        .delim = "\n",
      },

      // 2:
      util::joined{
        .rng = std_includes | std::views::transform([](const fs::path &p) {
          return std::format("#include <{}>", p.generic_string());
        }),
        .delim = "\n",
      },

      // 3:
      util::joined{
        .rng = ctx.reflected | std::views::transform(render_reflection),
        .delim = "\n\n",
      },

      // 4:
      util::joined{
        .rng = ctx.calls | std::views::transform(render_reflection),
        .delim = "\n\n",
      });
  } else {
    // todo: header mode
    std::cout << "\n[error] header mode is not implemented\n";
    return -1;
  }

  std::cout << "\n[info] done.\n";

  return 0;
}

namespace {

std::expected<cli::options, std::pair<int, std::string>> cli::parse(int argc,
  const char *const *argv) noexcept {
  // refactorme: app description and cli parameters should be at the start of
  // the file for clear undestanding, before 'main'
  CLI::App app{
    "\nC++ reflection code generator that operates in three modes:"
    "\n"
    "\nSource Mode (default):"
    "\n  Generates a single .cpp file containing reflected call implementations for a"
    "\n  list of .cpp sources. Compiled object file needs to be linked to the resulting binary."
    "\n"
    "\nHeader Mode:"
    "\n  Generates .hpp header files containing reflected call implementations for"
    "\n  given .cpp files. Headers must be implicitly included at the start of each"
    "\n  translation unit."
    "\n  WARNING: Uses compile time counters via friend injection, which is not guaranteed"
    "\n           by the C++ Standard to be consistent between compiler implementations."
    "\n"
    "\nDump yaml:"
    "\n  todo: explain",
  };

  fs::path cli_resource_dir;
  // todo: resolve from installation. Should have cmake generating based on
  // enable_packaging: packaged must be configured with default installation
  // path. debug development should be configured with found llvm path upon
  // configuration
  app
    .add_option("--resource-dir",
      cli_resource_dir,
      "Path to bundled system headers.")
    ->check(CLI::ExistingDirectory);
  // ->default_val(fs::path()); //< todo: resolve from installation

  options::mode_t cli_mode;
  // todo: how to print normal error?
  app.add_option("--mode", cli_mode, "reflection mode")
    ->default_val(cli::options::source)
    ->transform(CLI::CheckedTransformer(cli::str_by<cli::options::mode_t>));

  std::vector<cli_source_file> cli_sources;
  app
    .add_option("-s,--sources",
      cli_sources,
      "List of .cpp file paths to run the tool on.")
    ->type_name("FILE")
    ->required();

  fs::path cli_out;
  app
    .add_option("-o,--out", cli_out, "output directory (may contain filename)")
    ->type_name("PATH")
    ->default_val(fs::current_path().generic_string());

  std::optional<fs::path> cli_comp_db = std::nullopt;
  app.add_option("--comp-db", cli_comp_db, "Path to compile_commands.json")
    ->type_name("FILE")
    ->check([](const std::string &file) -> std::string {
      if (file.empty())
        return "";
      return CLI::ExistingFile(file);
    });

  app.allow_extras();

  std::vector<source_file> cli_sources_with_flags;
  for (auto &[_, flags] : cli_sources_with_flags) {
    auto v = flags | std::views::drop(1);
    flags = std::vector(v.begin(), v.end());
  }

  // refactorme: use ranges
  // Validation here
  app.callback([&] {
    // -- check sources
    {
      std::ranges::for_each(cli_sources, [](cli_source_file &s) {
        s.path = fs::absolute(std::move(s.path)).lexically_normal();

        // refactorme: 'unquote' string function
        if (s.output_substr.starts_with("\'")
          || s.output_substr.starts_with("\""))
          s.output_substr = std::move(s.output_substr).substr(1);

        if (s.output_substr.ends_with("\'") || s.output_substr.ends_with("\""))
          s.output_substr =
            std::move(s.output_substr).substr(0, s.output_substr.size() - 1);
      });

      std::vector<fs::path> invalid_sources;
      std::ranges::transform(cli_sources
          | std::views::filter([](const cli_source_file &s) -> bool {
              return !fs::exists(s.path);
            }),
        std::back_inserter(invalid_sources),
        [](const cli_source_file &s) { return s.path; });

      if (!invalid_sources.empty()) {
        throw CLI::ValidationError("Non-existent sources:",
          std::format("[{}]",
            util::joined{
              .rng = invalid_sources
                | std::views::transform(
                  [](const fs::path &p) { return p.generic_string(); }),
              .delim = ", ",
            }));
      }
    }

    const std::vector flags = app.remaining();
    if (!cli_comp_db) {
      std::ranges::transform(std::move(cli_sources),
        std::back_inserter(cli_sources_with_flags),
        [&flags](cli_source_file f) -> source_file {
          return {
            .path = std::move(f.path),
            .flags = flags,
          };
        });

      return;
    }

    if (!app.remaining().empty())
      throw CLI::ValidationError(
        "Compilation flags are not allowed if compile_commands.json is used.");

    cli_comp_db = fs::absolute(*std::move(cli_comp_db)).lexically_normal();

    const std::expected loaded = std::invoke(
      [](const fs::path &db_path)
        -> std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>,
          std::string> {
        std::string err;
        std::unique_ptr loaded =
          // fixme:
          // this is an inconsistent cli interface. one can pass any
          // 'existing' file which is actually not a `compile_commands.json`
          // but still load `compile_commands.json`
          clang::tooling::CompilationDatabase::loadFromDirectory(
            db_path.parent_path().generic_string(),
            err);

        if (!loaded)
          return std::unexpected(std::move(err));

        return loaded;
      },
      *cli_comp_db);

    if (!loaded)
      throw CLI::ValidationError("Invalid compilation db file:",
        cli_comp_db->generic_string());

    struct resolved_t {
      std::vector<source_file> files;
      std::vector<std::pair<fs::path, std::string>> invalid;
    };

    // refactorme: partition would be nice
    auto [files, invalid] = std::ranges::fold_left(cli_sources,
      resolved_t{},
      [&db = **loaded,
        commands = std::vector<clang::tooling::CompileCommand>{}](auto accum,
        const cli_source_file &s) mutable {
        commands = db.getCompileCommands(s.path.generic_string());
        if (commands.empty()) {
          accum.invalid.emplace_back(s.path, "not found in compilation db");
          return accum;
        }

        std::ranges::view auto resolved_view = commands
          | std::views::filter(
            [&s](const clang::tooling::CompileCommand &c) -> bool {
              return c.Output.contains(s.output_substr);
            });
        std::vector resolved(resolved_view.begin(), resolved_view.end());

        if (1 == resolved.size()) {
          accum.files.push_back({
            .path = s.path,
            .flags = std::move(commands.front().CommandLine),
          });
          return accum;
        }

        accum.invalid.emplace_back(s.path,
          std::format("failed to resolve from {} commands by substring `{}`",
            commands.size(),
            s.output_substr));
        return accum;
      });

    if (!invalid.empty()) {
      throw CLI::ValidationError(
        "Failed to resolve sources from compilation db",
        std::format("[{}]",
          util::joined{
            .rng = invalid | std::views::transform([](const auto &pair) {
              const auto &[path, reason] = pair;
              return std::format("{}: {}", path.generic_string(), reason);
            }),
            .delim = ",\n",
          }));
    }

    cli_sources_with_flags = std::move(files);
  });

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    std::stringstream ss;
    const auto code = app.exit(e, ss, ss);
    return std::unexpected(std::pair{code, std::move(ss).str()});
  }

  return options{
    .sources = std::move(cli_sources_with_flags),
    .out = std::move(cli_out),
    .resource_dir = std::move(cli_resource_dir),
    .mode = cli_mode,
  };
}

std::expected<llvm::opt::InputArgList, std::string> parse_driver_args(
  const std::vector<std::string> &flags) {
  std::vector<const char *> cli_ref;
  cli_ref.reserve(flags.size());
  std::ranges::transform(flags,
    std::back_inserter(cli_ref),
    [](std::string_view s) { return s.data(); });

  unsigned missing_arg, missing_arg_c;

  // ad hoc: this allows to translate to cc1 options (i.e. msvc -> cc1)
  llvm::opt::InputArgList argList =
    clang::driver::getDriverOptTable().ParseArgs(
      llvm::ArrayRef(cli_ref.data(), cli_ref.data() + cli_ref.size()),
      missing_arg,
      missing_arg_c);

  if (missing_arg != missing_arg_c) {
    return std::unexpected(
      std::format("Error: Missing argument for option at index {}\n",
        missing_arg));
  }
  return argList;
}

/// leave only the args needed for ast parsing
std::vector<std::string> filter_ast_related_args(
  const llvm::opt::InputArgList &args) {
  namespace options = clang::driver::options;

  // -- only flags related to ast creation
  // single-value / boolean flags (last-wins or toggles)
  constexpr std::array k_single_value_ids = {
    // -- toggles
    // options::OPT_nostdinc, //< ignored, used by the tool
    // options::OPT_nostdincxx, //< ignored, used by the tool
    // options::OPT_nostdlibinc, //< ignored, used by the tool
    options::OPT_fms_extensions,
    options::OPT_fno_ms_extensions,
    options::OPT_fmodules,
    options::OPT_fno_modules,
    options::OPT_fborland_extensions,
    options::OPT_fno_borland_extensions,
    options::OPT_fgnu_keywords,
    options::OPT_fno_gnu_keywords,
    options::OPT_fms_compatibility,
    options::OPT_fno_ms_compatibility,

    // -- single-value
    options::OPT_std_EQ, // -std=<...>
    options::OPT_x, // -x <lang>
    options::OPT_target, // --target=<triple>
    options::OPT_isysroot, // -isysroot <dir>
    options::OPT_resource_dir, // -resource-dir <dir>
    options::OPT_fmodules_cache_path, // -fmodules-cache-path <dir>
  };

  // repeatable, multi-value flags (accumulate all occurrences)
  constexpr std::array k_multi_value_ids = {
    // -- preprocessor
    options::OPT_D, // -DNAME[=VAL]
    options::OPT__SLASH_D, // /DNAME[=VAL]
    options::OPT_U, // -UNAME
    options::OPT__SLASH_U, // /UNAME
    options::OPT_include, // -include <file>
    options::OPT__SLASH_FI, // /FI <file>

    // -- header search
    options::OPT_I, // -I <dir>
    options::OPT__SLASH_I, // /I <dir>
    options::OPT_isystem, // -isystem <dir>
    options::OPT_isystem_after, // -isystem-after <dir>
    options::OPT_idirafter, // -idirafter <dir>
    options::OPT_iquote, // -iquote <dir>
    options::OPT_iprefix, // -iprefix <prefix>
    options::OPT_iwithprefix, // -iwithprefix <dir>
    options::OPT_iwithprefixbefore, // -iwithprefixbefore <dir>
    options::OPT_F, // -F <dir>
    options::OPT_iframework, // -iframework <dir>
    options::OPT_iframeworkwithsysroot, // -iframeworkwithsysroot <dir>

    // -- modules
    options::OPT_fmodule_map_file, // -fmodule-map-file=<path>
    options::OPT_fmodule_file, // -fmodule-file=<path>
  };

  std::vector<std::string> result;
  // handle single-value options
  std::ranges::for_each(
    k_single_value_ids | std::views::transform([&args](options::ID o) {
      return args.getLastArgNoClaim(o);
    }) | std::views::filter([](llvm::opt::Arg *a) { return nullptr != a; }),
    [&result](const llvm::opt::Arg *a) {
      const llvm::opt::Option &opt = a->getOption().getUnaliasedOption();
      result.emplace_back(opt.getPrefixedName().str()) += a->getValue();
    });

  // Handle multi-value options
  std::ranges::for_each(args.getArgs()
      | std::views::filter([&k_multi_value_ids](const llvm::opt::Arg *a) {
          return std::ranges::contains(k_multi_value_ids,
            a->getOption().getID());
        }),
    [&result](const llvm::opt::Arg *a) {
      const llvm::opt::Option &opt = a->getOption().getUnaliasedOption();
      result.emplace_back(opt.getPrefixedName().str());
      for (const auto &v : a->getValues()) {
        result.emplace_back(v);
      }
    });

  return result;
}

// refactorme: this is an ugly shite
std::expected<std::shared_ptr<clang::CompilerInvocation>, std::string>
  compiler_invocation_from_args_t::operator()(const source_file &sf,
    args a) const noexcept {
  namespace options = clang::driver::options;

  const auto to_vector_of_raw_pointers =
    [](const std::vector<std::string> &v) -> std::vector<const char *> {
    std::vector<const char *> result;
    result.reserve(v.size());
    std::ranges::transform(v,
      std::back_inserter(result),
      [](const std::string &s) { return s.c_str(); });
    return result;
  };

  const auto &[source, flags] = sf;

  std::cout << std::format(
    "\n[info] input flags:"
    "\n  [{}]\n",
    util::joined{
      .rng = flags | std::views::all,
      .delim = "\n  ",
    });

  const std::expected arg_list = parse_driver_args(flags);
  if (!arg_list)
    return std::unexpected(arg_list.error());

  enum class toolchain_kind { msvc, generic };

  const auto toolchain = std::invoke([&]() {
    const bool has_driver_mode_cl = std::invoke([&]() {
      if (auto *dm = arg_list->getLastArgNoClaim(options::OPT_driver_mode))
        return std::string_view(dm->getValue()) == "cl";
      return false;
    });

    const bool prog_looks_cl = std::invoke([&]() {
      if (flags.empty())
        return false;
      std::string_view prog = flags.front();
      return prog.ends_with("cl.exe")
        || prog.find("clang-cl") != std::string_view::npos;
    });

    const bool has_msvc_style_opts =
      arg_list->hasArgNoClaim(options::OPT__SLASH_Fo)
      || arg_list->hasArgNoClaim(options::OPT__SLASH_EH);

    if (has_driver_mode_cl || prog_looks_cl || has_msvc_style_opts)
      return toolchain_kind::msvc;
    return toolchain_kind::generic;
  });

  const std::string target_triple = std::invoke([&]() {
    if (const auto *opt = arg_list->getLastArgNoClaim(options::OPT_target))
      return llvm::Triple::normalize(opt->getValue());
    return llvm::sys::getProcessTriple();
  });

  const std::string driver_triple = std::invoke([&]() {
    llvm::Triple triple(target_triple);
    if (toolchain_kind::msvc == toolchain) {
      triple.setOS(llvm::Triple::Win32);
      triple.setEnvironment(llvm::Triple::MSVC);
    }
    return triple.str();
  });

  const std::vector<std::string> external_includes = std::invoke([&]() {
    std::vector<std::string> dirs;
    dirs.reserve(flags.size());
    for (const std::string &f : flags) {
      std::string_view sv(f);
      constexpr std::string_view p1 = "-external:I";
      constexpr std::string_view p2 = "/external:I";
      if (sv.starts_with(p1))
        dirs.emplace_back(sv.substr(p1.size()));
      else if (sv.starts_with(p2))
        dirs.emplace_back(sv.substr(p2.size()));
    }
    return dirs;
  });

  const std::vector<std::string> cc1_args = std::invoke([&]() {
    std::vector<std::string> base{
      "omnirefl",
      source.generic_string(),
      "-fsyntax-only", //< AST only
      std::format("-resource-dir={}", a.resource_dir.generic_string()),
    };

    if (toolchain != toolchain_kind::msvc)
      base.emplace_back("-nostdinc++"); //< prevents from picking up on another
                                        // compiler's C++ < std libs

    base = util::concat(std::move(base), filter_ast_related_args(*arg_list));

    for (const std::string &dir : external_includes) {
      base.emplace_back("-I");
      base.emplace_back(dir);
    }

    return base;
  });

  // ad hoc: this is a heavy-weight solution just to get proper argument
  // translations (for other compilers' commands) and to resolve system
  // include paths...
  clang::driver::Driver driver(toolchain_kind::msvc == toolchain //
      ? "clang-cl"
      : "clang",
    driver_triple,
    a.diag,
    "omnirefl reflection tool");
  driver.setCheckInputsExist(false);

  const std::vector cc1_args_ref = to_vector_of_raw_pointers(cc1_args);
  const std::unique_ptr<clang::driver::Compilation> compilation(
    driver.BuildCompilation(llvm::ArrayRef(cc1_args_ref.data(),
      cc1_args_ref.data() + cc1_args_ref.size())));

  if (!compilation || compilation->getJobs().empty()) {
    return std::unexpected(
      std::format("Failed to build compilatioin for: {}.\n",
        source.generic_string()));
  }

  const auto &compilation_args = compilation->getJobs().begin()->getArguments();
  // ad hoc: driver.BuildCompilation will populate the list with a lot of
  // unrelate parameters.
  // fixme: cc1_args is empty after filtering
  // cc1_args = filter_ast_related_args(
  //   {compilation_args.begin(), compilation_args.end()});
  // }

  // fixme: these args also need to be filtered!
  std::cout << std::format(
    "\n[info] using cc1 args:"
    "\n  [{}]\n",
    util::joined{
      .rng = compilation_args | std::views::all,
      .delim = "\n  ",
    });

  std::shared_ptr compiler_invocation =
    std::make_shared<clang::CompilerInvocation>();

  {
    const std::vector cc1_args_ref2 = to_vector_of_raw_pointers(cc1_args);
    (void)cc1_args_ref2;
    if (!clang::CompilerInvocation::CreateFromArgs(*compiler_invocation,
          // fixme: cc1_args is empty after filtering
          // {cc1_args_ref2.data(), cc1_args_ref2.data() +
          // cc1_args_ref2.size()},
          compilation_args,
          a.diag)) {
      return std::unexpected(
        std::format("Failed to create CompilerInvocation for: {}.",
          source.generic_string()));
    }
  }

  {
    clang::HeaderSearchOptions &o = compiler_invocation->getHeaderSearchOpts();

    // own libc++ includes are bundled
    if (toolchain != toolchain_kind::msvc)
      o.UseStandardCXXIncludes = false;
    o.ResourceDir = a.resource_dir.generic_string();

    o.AddPath(
      // todo: this should be configured at compile time
      (a.resource_dir / "include/x86_64-unknown-linux-gnu/c++/v1")
        .generic_string(),
      clang::frontend::IncludeDirGroup::CXXSystem,
      // todo: I have no idea what are these parameters. comment
      /*IsFramework=*/false,
      /*IgnoreSysRoot=*/false);

    // todo: this should be configured at compile time
    o.AddPath((a.resource_dir / "include/c++/v1").generic_string(),
      clang::frontend::IncludeDirGroup::CXXSystem,
      // todo: I have no idea what are these parameters. comment
      /*IsFramework=*/false,
      /*IgnoreSysRoot=*/false);

    // ad hoc: C++ headers must be included before C's
    std::rotate(o.UserEntries.rbegin(),
      o.UserEntries.rbegin() + 2,
      o.UserEntries.rend());
  }

  {
    clang::PreprocessorOptions &p = compiler_invocation->getPreprocessorOpts();

    // Disable any pch generation/usage operations. Since serialized
    // preamble format is unstable, using an incompatible one might result
    // in unexpected behaviours, including crashes.
    p.ImplicitPCHInclude.clear();
    p.PrecompiledPreambleBytes = {0, false};
    p.PCHThroughHeader.clear();
    p.PCHWithHdrStop = false;
    p.PCHWithHdrStopCreate = false;
  }

  return compiler_invocation;
}

// todo: use expected
std::string get_declaration_source_file(const clang::Decl &d,
  const clang::SourceManager &sm) noexcept {
  const auto loc = d.getLocation();
  if (!loc.isValid())
    return "";

  const auto pm = sm.getPresumedLoc(loc);
  if (!pm.isValid())
    return "";

  std::string filename = pm.getFilename();
  return filename;
}

std::expected<clang::Type const *, std::string> get_template_type_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) noexcept {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() < n) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(
      std::format("invalid template signature for internal omnirefl type {}",
        detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Type != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(std::format("non-type template argument `{}` of {}",
      n,
      detail_struct_name));
  }

  return arg.getAsType().getTypePtr();
}

// todo: remove maybe_unused when header mode is implemented (indexed calls)
[[maybe_unused]] std::expected<int, std::string> get_template_value_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) noexcept {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() < n) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(
      std::format("invalid template signature for internal omnirefl type {}",
        detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Integral != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(
      std::format("non-integral template argument `{}` of {}",
        n,
        detail_struct_name));
  }

  return arg.getAsIntegral().getExtValue();
}

meta::nm_qual_type resolve_nm_qual_type(const clang::TagDecl &td) noexcept {
  auto [namespaces, enclosing_records] =
    std::invoke([&decl_ctx = *td.getDeclContext()] {
      std::array<std::vector<std::string>, 2> result{};
      auto &&[namespaces, enclosing_records] = result;

      const clang::DeclContext *dc = &decl_ctx;
      while (!llvm::isa<clang::TranslationUnitDecl>(dc)) {
        if (const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(dc)) {
          if (ns->isAnonymousNamespace()) {
            namespaces.emplace_back("");
          } else if (ns->isStdNamespace() && ns->getName().starts_with("__")) {
            // fixme:
            // ad hoc: skip implementation-detail namespaces inside std (e.g.
            // std::__1) do nothing
          } else {
            namespaces.emplace_back(ns->getName().str());
          }
        } else if (const auto *rec = llvm::dyn_cast<clang::RecordDecl>(dc)) {
          const clang::IdentifierInfo *id = rec->getIdentifier();

          // fixme:
          // std::optional for enclosing records to specifically signal unnamed
          // records. I need to get back this later, because rendering this is
          // probably more difficult...
          enclosing_records.emplace_back(id ? id->getName().str() : "");
        }

        dc = dc->getParent();
      }

      std::ranges::reverse(namespaces);
      std::ranges::reverse(enclosing_records);
      return result;
    });

  return {
    .name = std::invoke([&td] -> std::optional<std::string> {
      const clang::IdentifierInfo *id = td.getIdentifier();
      if (!id)
        return std::nullopt;

      // "no qualifiers" (not const, not volatile, not restrict).
      const unsigned qual_flags = 0;
      std::string name =
        // fixme:
        // ad hoc: for template specializations use the printing
        // policy to render <template args> properly. For _call_impl in source
        // mode this is sufficient.
        !llvm::isa<clang::ClassTemplateSpecializationDecl>(td)
        ? id->getName().str()
        : clang::QualType{td.getTypeForDecl(), qual_flags}.getAsString(
            std::invoke([] {
              clang::PrintingPolicy p{{}};

              p.SuppressTagKeyword = true; //< no 'struct', 'class' or 'enum'
              p.SuppressScope =
                false; //< namespaces or enclosing records for <template args>
              p.PrintCanonicalTypes = true;

              return p;
            }));

      // ad hoc: (see above) for template specializations strip all scopes to
      // the left of the last '::' before '<', keeping template args intact.
      if (const auto lt = name.find('<'); std::string::npos != lt) {
        if (const auto scope = name.rfind("::", lt);
          scope != std::string::npos) {
          name.erase(0, scope + 2);
        }
      }

      return name;
    }),
    .namespaces = std::move(namespaces),
    .enclosing_records = std::move(enclosing_records),
  };
}

meta::type_definition resolve_definition(const clang::TagDecl &td,
  const clang::SourceManager &sm) noexcept {
  const clang::DeclContext &decl_ctx = *td.getDeclContext();

  // refactorme: ugleee
  using td_flags = decltype(meta::type_definition::definition_flags);
  const td_flags td_local = decl_ctx.isFunctionOrMethod() //
    ? meta::type_definition::local
    : meta::type_definition::none;

  const td_flags td_non_public = td_flags::none; //< todo:

  const clang::SourceLocation loc = td.getLocation();
  return {
    .type_name = resolve_nm_qual_type(td),
    .location =
      {
        .source_file = get_declaration_source_file(td, sm),
        .line = sm.getSpellingLineNumber(loc),
        .column = sm.getSpellingColumnNumber(loc),
      },
    // refactorme: enable bitwise operations
    .definition_flags = td_flags(td_local | td_non_public),
  };
}

std::vector<std::string> public_field_names(
  clang::RecordDecl::field_range fields) noexcept {
  std::vector<std::string> r;
  std::ranges::transform(fields
      | std::views::filter([](const clang::FieldDecl *fd) {
          // todo:
          //   consider logging for skipped fields, since we are not reporting
          //   them as errors
          return clang::AccessSpecifier::AS_public == fd->getAccess();
        }),
    std::back_inserter(r),
    [](const clang::FieldDecl *fd) { return fd->getNameAsString(); });

  return r;
}

// refactorme: reflect in code instead of writing the comment below.
//
// types of interest for recursive reflection:
//   - value_type: std containers, std wrappers, std::optional
//   - key_type: std containers
//   - type: std::reference_wrapper
struct member_typedef_decl {
  std::string_view name;
  clang::QualType qual_type;
};

std::vector<member_typedef_decl> member_typedefs(
  const clang::RecordDecl &rd) noexcept {
  // todo: use `filter`
  std::vector<member_typedef_decl> r;
  r.reserve(std::accumulate(rd.decls_begin(),
    rd.decls_end(),
    size_t(0),
    [](size_t s, const clang::Decl *d) -> size_t {
      const clang::Decl::Kind k = d->getKind();
      return s + (clang::Decl::TypeAlias == k || clang::Decl::Typedef == k);
    }));

  for (const clang::Decl *_d : rd.decls()) {
    const clang::Decl::Kind k = _d->getKind();
    if (clang::Decl::TypeAlias != k && clang::Decl::Typedef != k)
      continue;

    const auto &d = *llvm::cast<clang::TypedefNameDecl>(_d);
    r.push_back({
      .name = d.getName(),
      .qual_type = d.getUnderlyingType(),
    });
  }
  return r;
};

std::vector<const clang::CXXRecordDecl *> recursively_collect_dependency_types(
  const clang::CXXRecordDecl &root,
  const std::set<meta::type_id> &resolved_types) noexcept {
  std::set<const clang::CXXRecordDecl *> visited;
  std::stack<const clang::CXXRecordDecl *> to_visit;
  to_visit.push(&root);

  while (!to_visit.empty()) {
    const clang::CXXRecordDecl *cur = to_visit.top();
    to_visit.pop();

    // todo: remove
    const std::string _debug_nm_qual_type = cur->getQualifiedNameAsString();
    if (visited.contains(cur)
      // todo: continious benchmark before optimization (lookup by string)
      || resolved_types.contains(cur->getQualifiedNameAsString())) {
      continue;
    }

    // not generating reflection info for std types, at least for now, at least
    // here...
    //
    // refactorme: ambiguous control flow/logic
    if (!cur->isInStdNamespace())
      visited.emplace(cur);

    // allow recursive reflection via certain member typedefs
    std::ranges::for_each(member_typedefs(*cur)
        | std::views::filter([](const member_typedef_decl &m) {
            static const std::set<std::string_view> aliases{
              "key_type",
              "value_type",
              "value",
            };
            return aliases.contains(m.name)
              && m.qual_type->isStructureOrClassType();
          })
        | std::views::transform([](const member_typedef_decl &m) {
            return m.qual_type->getAsCXXRecordDecl();
          }),
      [&to_visit](const clang::CXXRecordDecl *rd) { to_visit.push(rd); });

    // fixme: what about unions, built-in arrays?

    // refactorme: ad hoc solution:
    //   in c++ code template trait types can be used, but I don't know if it is
    //   possible to get from parsed ast.
    //   I could, however, capture such types in `omni::detail`...
    if (clang::Decl::ClassTemplateSpecialization == cur->getKind() &&
        [](std::string_view name) -> bool { //
          return "tuple" == name || "variant" == name;
        }(cur->getName())) {
      const auto arg_list =
        clang::cast<clang::ClassTemplateSpecializationDecl>(cur)
          ->getTemplateInstantiationArgs()
          .asArray();
      if (1 != arg_list.size()
        || clang::TemplateArgument::Pack != arg_list.front().getKind()) {
        // todo: log? this should not happen
      } else {
        for (const clang::TemplateArgument &t_arg :
          arg_list.front().getPackAsArray()) {
          // not a type pack
          // todo: is this possible though? should log?
          if (clang::TemplateArgument::Type != t_arg.getKind())
            break;

          const clang::QualType qt = t_arg.getAsType();
          if (qt->isStructureOrClassType())
            to_visit.push(qt->getAsCXXRecordDecl());
        }
      }
    }

    // fixme: just collect the types: determine reflectable later?
    for (const clang::FieldDecl *fd : cur->fields()) {
      if (clang::AccessSpecifier::AS_public != fd->getAccess()
        // todo: other checks that would prevent the field from being
        // reflected (uniouns, bitfields, what else?)
        || fd->isUnnamedBitField())
        continue;

      const clang::QualType qt = fd->getType();
      // fixme: what about unions, built-in arrays?
      if (!qt->isStructureOrClassType())
        continue;

      // todo: support only non-static fields
      to_visit.push(qt->getAsCXXRecordDecl());
    }
  }

  visited.erase(&root);
  return {visited.begin(), visited.end()};
}

// fixme:
// `mpark::variant` is picked up as a type, not as a template specialization
//
// fixme:
// nested `wrestler::info` in `example_types` namespace is rendered as
// `example_types::info`
auto meta::resolve_reflected_type(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  const clang::ASTUnit &ast,
  const std::set<type_id> &resolved_types)
  -> std::expected<reflected_type_info, std::string> {
  // todo: assert template signature
  // omni::detail::_reflected_type<T>
  std::expected _reflected_type = get_template_type_arg(template_decl, 0);
  if (!_reflected_type)
    return std::unexpected(std::move(_reflected_type).error());

  const clang::Type &reflected_type = **_reflected_type;

  if (reflected_type.isFundamentalType()) {
    return reflected_type_info{
      .type =
        non_reflectable{
          .id = reflected_type
            // fixme: I don't think it does what I
            // need (namespace-qualified typename)
            .getTypeClassName(),
        },
      .dependencies = {},
    };
  }

  if (clang::isa<clang::EnumType>(reflected_type)) {
    const clang::EnumDecl &ed =
      *clang::cast<clang::EnumType>(reflected_type).getDecl();

    return reflected_type_info{
      .type =
        reflectable{
          .id = reflected_type
            // fixme: I don't think it does what I
            // need (namespace-qualified typename)
            .getTypeClassName(),
          .data =
            enum_data{
              .is_scoped = ed.isScoped(),
              .enumerators = std::invoke([&ed] {
                std::vector<std::string> result;
                std::ranges::transform(ed.enumerators(),
                  std::back_inserter(result),
                  [](const clang::EnumConstantDecl *e) {
                    return e->getName().str();
                  });

                return result;
              }),
            },
          .definition = resolve_definition(ed, ast.getSourceManager()),
        },
      .dependencies = {},
    };
  }

  // fixme: handle unions, C-arrays, (maybe) pointers
  if (!reflected_type.isStructureOrClassType()) {
    return std::unexpected(std::format("unsupported type {} in {}",
      // fixme:
      //   I don't think it does what I need (namespace-qualified typename)
      reflected_type.getTypeClassName(),
      util::to_string_view(template_decl.getName())));
  }

  const clang::CXXRecordDecl &reflected_type_decl =
    *reflected_type.getAsCXXRecordDecl();
  const std::string nm_qual_type =
    reflected_type_decl.getQualifiedNameAsString();

  // todo: do not use nm_qual_type to uniquely identify the type
  if (resolved_types.contains(nm_qual_type)) {
    return reflected_type_info{
      .type =
        already_reflected{
          .id = nm_qual_type,
        },
      .dependencies = {},
    };
  }

  // todo: forward declarations may be supported in source mode
  if (!reflected_type_decl.hasDefinition()) {
    return std::unexpected(
      std::format("forward declarations are not allowed: {}", nm_qual_type));
  }

  std::vector<meta::reflectable> dependencies;
  std::ranges::transform(
    recursively_collect_dependency_types(reflected_type_decl, resolved_types),
    std::back_inserter(dependencies),
    [&sm = ast.getSourceManager()](
      const clang::CXXRecordDecl *rd) -> meta::reflectable {
      return {
        .id = rd->getQualifiedNameAsString(),
        .data =
          meta::struct_data{
            .public_fields = public_field_names(rd->fields()),

            // fixme: other enums
            .type = rd->isStruct() //
              ? meta::struct_data::is_struct
              : meta::struct_data::is_class,
          },
        .definition = resolve_definition(*rd, sm),
      };
    });

  if (reflected_type_decl.isInStdNamespace()) {
    return reflected_type_info{
      .type = non_reflectable{.id = nm_qual_type},
      .dependencies = std::move(dependencies),
    };
  }

  return reflected_type_info{
    .type =
      reflectable{
        // todo: do not use nm_qual_type to uniquely identify the type
        .id = nm_qual_type,
        .data =
          struct_data{
            .public_fields = public_field_names(reflected_type_decl.fields()),
            // fixme: other enums
            .type = reflected_type_decl.isStruct() //
              ? meta::struct_data::is_struct
              : meta::struct_data::is_class,
          },
        .definition =
          resolve_definition(reflected_type_decl, ast.getSourceManager()),
      },
    .dependencies = std::move(dependencies),
  };
}

auto meta::resolve_reflected_call(const clang::CXXMethodDecl &cd,
  const clang::ASTUnit &ast)
  -> std::expected<reflected_call_info, std::string> {
  static constexpr auto to_func_arg =
    [](clang::QualType q) -> meta::function_signature_arg {
    const clang::QualType base_q =
      q->isLValueReferenceType() || q->isRValueReferenceType()
      ? q->getPointeeType()
      : q;

    // refactorme: I want a .value_or monadic function for pointers to avoid
    // lvalues
    const clang::TagDecl *tag = base_q->getAsTagDecl();
    return {
      .type_name = tag
        ? resolve_nm_qual_type(*tag)
        // no tag -> fundamental / alias. No namespaces needed.
        : meta::nm_qual_type{
             // keeps template args for specs, OK for fundamentals
            .name = base_q.getAsString(),
            .namespaces = {},
            .enclosing_records = {},
          },

      .is_const = base_q.isConstQualified(),
      .ref_type = q->isLValueReferenceType() //
        ? reference_type::ref_lval
        : q->isRValueReferenceType() //
          ? reference_type::ref_rval
          : reference_type::ref_none,
    };
  };

  constexpr auto needs_include = [](clang::QualType q) -> bool {
    if (q.isNull())
      return false;

    // Strip lvalue/rvalue reference
    clang::QualType base_q =
      q->isLValueReferenceType() || q->isRValueReferenceType()
      ? q->getPointeeType()
      : q;

    if (base_q.isNull())
      return false;

    const clang::Type *ty = base_q.getTypePtrOrNull();
    if (!ty)
      return false;

    // Only structure/class types participate in include deduction
    if (!ty->isStructureOrClassType())
      return false;

    const auto *rd = ty->getAsCXXRecordDecl();
    if (!rd)
      return false;

    // We only care if there is a usable definition
    return rd->getDefinition() != nullptr;
  };

  struct include_info {
    fs::path include;
    bool is_std;
  };

  // refactorme: remove intermediate allocations.
  // ```
  // concat(params, return_val)
  //  | filter(needs_include)
  //  | transform(to_include)
  //  | partition
  // ```
  std::vector<include_info> all_includes;
  std::ranges::transform(
    std::views::iota(std::size_t{0}, cd.parameters().size() + 1)
      | std::views::transform([&cd](std::size_t i) -> clang::QualType {
          return 0 == i //
            ? cd.getReturnType()
            : cd.parameters()[i - 1]->getType();
        })
      | std::views::filter(needs_include),
    std::back_inserter(all_includes),
    [&sm = ast.getSourceManager()](clang::QualType q) -> include_info {
      // todo: I'm not sure this is correct or even needed
      const clang::QualType base_q =
        q->isLValueReferenceType() || q->isRValueReferenceType()
        ? q->getPointeeType()
        : q;

      const clang::Type *ty = base_q.getTypePtrOrNull();
      const clang::CXXRecordDecl &rd = *ty->getAsCXXRecordDecl();
      const clang::CXXRecordDecl &def = *rd.getDefinition();

      // refactorme: I only need the source_file, not the whole definition
      const auto def_info = resolve_definition(def, sm);
      const bool is_std = rd.isInStdNamespace();

      return {
        .include = is_std //
          ? def_info.location.source_file.stem()
          : def_info.location.source_file,
        .is_std = is_std,
      };
    });

  std::set<fs::path> includes;
  std::set<fs::path> std_includes;

  for (auto &&info : all_includes)
    (info.is_std ? std_includes : includes).insert(std::move(info.include));

  return reflected_call_info{
    .f_sig =
      {
        .args = std::invoke([&cd] {
          std::vector<function_signature_arg> args;
          args.reserve(cd.parameters().size());
          std::ranges::transform(cd.parameters()
              | std::views::transform(
                [](const clang::ParmVarDecl *pd) { return pd->getType(); }),
            std::back_inserter(args),
            to_func_arg);
          return args;
        }),
        .return_type = to_func_arg(cd.getReturnType()),
      },
    .includes = std::move(includes),
    .std_includes = std::move(std_includes),
  };
}

} // namespace
