
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
#include <clang/AST/DeclBase.h>
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
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Frontend/Utils.h>
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
#include <array>
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

namespace fs = std::filesystem;
using namespace std::string_view_literals;

namespace fn {

template <typename T>
struct as_t {
  template <typename U>
    requires std::constructible_from<T, U>
  constexpr T operator()(U &&value) const
    noexcept(noexcept(T(std::forward<U>(value)))) {
    return T(std::forward<U>(value));
  }
};

template <typename T>
constexpr as_t<T> as{};

} // namespace fn

namespace util {

std::expected<std::ofstream, std::string> create_file_for_writing(
  const fs::path &file) {
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);

  if (ec) {
    return std::unexpected(std::format("invalid parent path: {}",
      file.parent_path().generic_string()));
  }

  std::ofstream out{file, std::ios::binary};
  if (!out) {
    return std::unexpected(
      std::format("failed to open file {} for writing", file.generic_string()));
  }

  return out;
}

template <typename R, typename T>
concept viewable_range_of = std::ranges::viewable_range<R>
  && requires(std::ranges::range_reference_t<R> v) { T{v}; };

template <std::ranges::range R, std::integral I = std::size_t>
  requires(std::ranges::viewable_range<R>
    || std::move_constructible<std::remove_cvref_t<R>>)
constexpr auto indexed(R &&r, I start = 0) {
  auto idx = std::views::iota(start);

  if constexpr (std::ranges::viewable_range<R>) {
    return std::views::zip(std::views::all(std::forward<R>(r)), idx);
  } else {
    // R is an rvalue non-viewable (typically non-borrowed);
    // own it to avoid dangling.
    using D = std::remove_cvref_t<R>;
    return std::views::zip(std::ranges::owning_view<D>{std::forward<R>(r)},
      idx);
  }
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

bool is_subpath(const std::filesystem::path &path,
  const std::filesystem::path &base) {
  const auto mismatch_pair =
    std::mismatch(path.begin(), path.end(), base.begin(), base.end());
  return mismatch_pair.second == base.end();
}

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

template <typename V, typename CharT = char>
struct fmt_fold_t {
  V rng;
};

struct fmt_fold_adaptor {
  template <std::ranges::viewable_range R>
    requires std::ranges::input_range<std::views::all_t<R>>
  constexpr auto operator()(R &&r) const {
    using V = std::views::all_t<R>;
    return fmt_fold_t<V, char>{std::views::all(std::forward<R>(r))};
  }

  template <std::ranges::viewable_range R>
    requires std::ranges::input_range<std::views::all_t<R>>
  friend constexpr auto operator|(R &&r, fmt_fold_adaptor self) {
    return self(std::forward<R>(r));
  }
};

constexpr fmt_fold_adaptor fmt_fold{};

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

template <std::ranges::input_range V, typename CharT>
struct std::formatter<util::fmt_fold_t<V, CharT>, CharT> {
  template <typename ParseContext>
  static constexpr auto parse(ParseContext &pc) {
    return std::formatter<std::basic_string_view<CharT>, CharT>{}.parse(pc);
  }

  template <typename FormatContext>
  static constexpr auto format(util::fmt_fold_t<V, CharT> x,
    FormatContext &ctx) {
    return std::ranges::fold_left(x.rng, ctx.out(), [&](auto out, auto &&e) {
      return std::format_to(out, "{}", e);
    });
  }
};

namespace {

struct source_file {
  fs::path path;
  std::vector<std::string> flags;

  // for transforms
  static std::string path_as_string(const source_file &sf) {
    return sf.path.generic_string();
  }
};

namespace cli {

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
      | std::views::transform(fn::as<std::string_view>);

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
    | std::views::transform(fn::as<std::string_view>)
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
  // .cpp source
  source_file source;

  // output dir or generated header path
  fs::path out;

  fs::path resource_dir;

  // todo: implement parsing
  bool generate_dep_file = true;
};

std::string format_options(const options &o) {
  // todo: consider printing flags
  return std::format(
    "source:{}"
    "\nout:{}"
    "\nresource_dir:{}",
    // 0:
    source_file::path_as_string(o.source),

    // 1:
    o.out.generic_string(),

    // 2:
    o.resource_dir.generic_string());
}

std::expected<options, std::pair<int, std::string>> parse(int argc,
  const char *const *argv) noexcept;

} // namespace cli

/// configure ast-related compiler invocation parameters
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
    [binding_tag]<typename Match, typename Node, typename Reduce>(Accum &accum,
      const ast::rule<Match, Node, Reduce> &rule) {
      const auto bound_matcher =
        traverse(rule.traversal_kind, rule.pattern.match.bind(binding_tag));
      using matcher_t = decltype(bound_matcher);

      struct _callback: MatchFinder::MatchCallback {
        matcher_t matcher;
        Accum &accum;
        Reduce reduce;

        std::string_view binding_tag;

        void run(const MatchFinder::MatchResult &result) override {
          const std::map nodeById = result.Nodes.getMap();
          const auto found = nodeById.find(binding_tag);
          if (nodeById.end() == found) {
            llvm::errs() << "DEBUG: Match::Callback false-fired.\n";
            return;
          }

          const auto *node = found->second.get<Node>();
          if (!node) {
            llvm::errs()
              << "DEBUG: Matc::Callback: Node of invalid type matched.\n";
            return;
          }

          accum = reduce(std::move(accum), *node);
        }

        _callback(const matcher_t &m,
          Accum &a,
          const Reduce &r,
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

constexpr std::array k_supported_member_aliases = //
  std::to_array<std::string_view>({
    "error_type",
    "key_type",
    "type",
    "value",
    "value_type",
  });

constexpr std::array k_supported_template_names = //
  std::to_array<std::string_view>({
    "tuple",
    "variant",
  });

template <typename>
struct map_decl_to_type_t;

template <typename Decl>
using map_decl_to_type = typename map_decl_to_type_t<Decl>::type;

const auto map_decl_to_canonical_type = //
  []<typename Decl>(const clang::ASTContext &ast,
    const Decl *d) -> const map_decl_to_type<std::remove_cvref_t<Decl>> * {
  return ast.getCanonicalType(ast.getTypeDeclType(d))
    ->template getAs<map_decl_to_type<std::remove_cvref_t<Decl>>>();
};

// mappings
template <>
struct map_decl_to_type_t<clang::EnumDecl>:
    std::type_identity<clang::EnumType> {};

template <>
struct map_decl_to_type_t<clang::TagDecl>:
    std::type_identity<clang::TagType> {};

template <>
struct map_decl_to_type_t<clang::RecordDecl>:
    std::type_identity<clang::RecordType> {};

template <>
struct map_decl_to_type_t<clang::CXXRecordDecl>:
    std::type_identity<clang::RecordType> {};

template <>
struct map_decl_to_type_t<clang::TypeDecl>: std::type_identity<clang::Type> {};

// todo: benchmark, then use hash unique type identifier
using type_id = std::string;

struct nm_qual_type {
  /// nullopt for unnamed types
  std::optional<std::string> name;

  /// chain of namespaces. may start with empty string if declared in anonymous
  /// namespace
  std::vector<std::string> namespaces;

  // todo: store type_id instead
  /// non-empty for nested types `struct foo { struct bar{}; };`
  std::vector<std::string> enclosing_records;

  // todo: should it be just clang::Decl?
  static meta::nm_qual_type from_decl(const clang::TagDecl *td) noexcept;
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

// as of now - public only
struct field_data {
  std::string name;
  bool is_bitfield;

  enum {
    none = 0,
    as_const,
    as_mutable,
  } qualified;

  static field_data from_decl(const clang::FieldDecl *d) {
    assert(d);
    return {
      .name = std::string(fn::as<std::string_view>(d->getName())),
      .is_bitfield = d->isBitField(),
      .qualified = d->isMutable()
        ? as_mutable
        : (d->getType().isConstQualified() ? as_const : none),
    };
  }
};

// todo: add info for template types
struct record_data {
  // only public-nonvirtual bases
  std::vector<type_id> public_bases;

  /// protected and private are not supported now (too complicated)
  std::vector<field_data> public_fields;

  enum {
    is_struct,
    is_class,
    is_union,
  } type;

  static record_data from_type(const clang::ASTContext &ast,
    const clang::RecordType *t);
};

struct enum_data {
  bool is_scoped; //< true if 'enum class { }`
  bool is_fixed;
  std::string underlying_type;
  std::vector<std::string> enumerators;

  static meta::enum_data from_type(const clang::EnumType *t);
};

template <typename Data>
concept reflectable_data =
  std::same_as<record_data, Data> || std::same_as<enum_data, Data>;

struct reflectable {
  type_id id;

  // refactorme: Consider having template arg `reflectable_data` instead
  std::variant<record_data, enum_data> data;
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
  const clang::ASTContext &ast,
  const std::set<type_id> &resolved_types);

std::expected<std::pair<std::size_t, reflected_type_info>, std::string>
  resolve_reflected_indexed_type(
    const clang::ClassTemplateSpecializationDecl &decl,
    const clang::ASTContext &ast,
    const std::set<type_id> &resolved_types);

struct source_file_context {
  source_file sf;

  std::vector<meta::reflectable> reflected{};

  std::set<meta::type_id> resolved_types{}; //< all resolved
  std::set<meta::type_id> resolved_as_dependency{};
  std::set<meta::type_id> non_reflectable_types{}; //< for debug or info

  std::vector<std::pair<meta::type_id, std::size_t>> index_by_type{};

  std::vector<std::string> errors{};

  // Used to generate deps file for cmake, so it can rerun the tool upon changes
  // to those headers
  std::set<fs::path> file_dependencies;
};

} // namespace meta

namespace render {

struct reflection_context {
  // can generate forward declaration reflected type
  struct forward_declarable {
    meta::reflectable type;
    std::optional<std::size_t> index;
    bool as_dependent;
  };

  // depends on forward declaration of enclosing type
  struct nested_type {
    meta::reflectable type;
    std::optional<std::size_t> index;
    bool as_dependent;
  };

  // can only be reflected using a generated index
  struct indexed_type {
    meta::reflectable type;
    std::size_t index;
  };

  source_file instrumented_source_file;

  std::vector<forward_declarable> fwd_declarables;
  std::vector<nested_type> nested;
  std::vector<indexed_type> indexed;

  std::set<fs::path> file_dependencies;
};

std::expected<reflection_context, std::string> prepare_reflection_context(
  meta::source_file_context ctx);

std::expected<reflection_context, std::string>
  generate_reflection(reflection_context ctx, std::ofstream file);

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
    elems //
      | std::views::join_with("::"sv) //
      | util::fmt_fold);
}

std::string forward_declaration(const meta::reflectable &t) {
  using namespace std::string_view_literals;

  assert(t.definition.type_name.name);

  std::vector<std::string> lines = t.definition.type_name.namespaces
    | std::views::transform(
      [](const std::string &n) { return std::format("namespace {} {{", n); })
    | std::ranges::to<std::vector>();

  const std::string name = std::format("{}{}{}",
    t.definition.type_name.enclosing_records //
      | std::views::transform(fn::as<std::string_view>)
      | std::views::join_with("::"sv) //
      | util::fmt_fold,
    t.definition.type_name.enclosing_records.empty() ? ""sv : "::"sv,
    *t.definition.type_name.name);

  const auto format_data_sum =
    [&name]<typename... U>(const std::variant<U...> &data) -> std::string {
    return std::visit(
      [&]<typename T>(const T &data) -> std::string {
        if constexpr (std::same_as<meta::enum_data, T>) {
          return std::format("enum {}{}{};",
            data.is_scoped ? "class " : "",
            name,
            data.is_fixed ? std::format(" : {}", data.underlying_type) : "");
        } else {
          static_assert(std::same_as<meta::record_data, T>);
          const meta::record_data &struct_data = data;

          // TODO(High): Generated-header reflection for templates, template
          // specializations, partial specializations, and template-template
          // types.
          return std::format("{} {};",
            std::invoke([type = struct_data.type] {
              switch (type) {
              case meta::record_data::is_struct:
                return "struct"sv;
              case meta::record_data::is_class:
                return "class"sv;
              case meta::record_data::is_union:
                return "union"sv;
              }
              assert(false && "unreachable");
              return "_unhandled_type"sv;
            }),
            name);
        }
      },
      data);
  };

  std::ranges::copy(std::views::single(t) //
      | std::views::transform([&](const meta::reflectable &r) -> std::string {
          return std::format(
            "// declared at: {}"
            "\n{}",
            format_location(r.definition.location),
            format_data_sum(r.data));
        }),
    std::back_inserter(lines));

  std::ranges::copy(t.definition.type_name.namespaces
      | std::views::transform([](const std::string &n) {
          return std::format("}} // namespace {}", n);
        }),
    std::back_inserter(lines));

  return std::format("{}",
    lines //
      | std::views::join_with("\n"sv) //
      | util::fmt_fold);
}

// refactorme: ugleee shite
struct log {
  const std::set<meta::type_id> &dependencies;
  std::map<meta::type_id, std::size_t> index_by_type;

  static std::string flags(int flags) {
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

  auto operator()(const meta::record_data &d) const {
    const char *kind = d.type == meta::record_data::is_struct //
      ? "struct"
      : d.type == meta::record_data::is_class //
        ? "class"
        : "union";

    if (d.public_fields.empty())
      return std::format("{} {{}}", kind);

    std::string fields;
    for (std::size_t i = 0; i < d.public_fields.size(); ++i) {
      if (i > 0)
        fields += ", ";
      fields += d.public_fields[i].name;
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
    const std::string indexed = std::invoke([&]() -> std::string {
      const auto index = index_by_type.find(d.id);
      return index_by_type.cend() != index
        ? std::format(" (indexed: {})", index->second)
        : std::string{};
    });

    return std::format(
      "reflected type{}{}:"
      "\n  {}"
      "\n  {}",
      (dependencies.contains(d.id) ? " (as dependency)" : ""),
      indexed,
      format_definition(d.definition),
      std::visit(*this, d.data));
  }

};

std::expected<fs::path, std::string> write_dependencies_file(
  const fs::path &generated_file,
  const util::viewable_range_of<fs::path> auto &includes) {
  fs::path depfile = generated_file;
  depfile += ".d";

  std::error_code ec;
  if (auto parent = depfile.parent_path(); !parent.empty()) {
    fs::create_directories(parent, ec);
    if (ec) {
      return std::unexpected(std::format("create_directories('{}'): {}",
        depfile.string(),
        ec.message()));
    }
  }

  fs::path tmp = depfile;
  tmp += ".tmp";

  std::ofstream os(tmp, std::ios::out | std::ios::trunc);
  if (!os)
    return std::unexpected(std::format("open('{}')", depfile.string()));

  const auto esc = [](std::string_view s) -> std::string {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
      if (c == ' ' || c == '\\' || c == '#')
        out.push_back('\\');
      out.push_back(c);
    }
    return out;
  };

  os << std::format("{}:{}\n",
    esc(generated_file.string()),
    std::ranges::empty(includes)
      ? std::string{}
      : std::format(" \\\n  {}\n",
          includes //
            | std::views::transform(
              [&](const fs::path &p) { return esc(p.string()); }) //
            | std::views::join_with(" \\\n  "sv) //
            | util::fmt_fold));

  os.close();
  if (!os)
    return std::unexpected(std::format("write/close('{}')", depfile.string()));

  fs::rename(tmp, depfile, ec);
  if (ec) {
    fs::remove(tmp, ec);
    return std::unexpected(
      std::format("rename('{}'): {}", depfile.string(), ec.message()));
  }

  return depfile;
}

// todo: yaml dump

} // namespace render

namespace pipeline {

struct diag_engine_binding {
  std::shared_ptr<clang::DiagnosticOptions> options;
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> engine;
};

struct with_compiler_invocation {
  source_file sf;

  std::shared_ptr<clang::CompilerInvocation> ci;
  diag_engine_binding diag;
};

std::expected<with_compiler_invocation, std::string>
  compiler_invocation_for_source_file(const fs::path &resource_dir,
    const source_file &sf) noexcept;

// todo: do I even need this as a separate funcion?
// todo: rename/refactor. This is (should be) entirely mode related.
with_compiler_invocation configure_compiler_invocation(
  const cli::options &cli_args,
  with_compiler_invocation wci) noexcept {
  {
    clang::LangOptions &lo = wci.ci->getLangOpts();
    // todo: investigate how to properly parse comments
    lo.CommentOpts.ParseAllComments = true;
  }

  {
    clang::PreprocessorOptions &po = wci.ci->getPreprocessorOpts();
    constexpr std::string_view k_omni_macro = "OMNI_GENERATED_REFLECTION";

    if (const auto omni_defined = std::ranges::find_if(po.Macros,
          [k_omni_macro](const auto &macro_def) {
            const auto &[macro, is_undef] = macro_def;
            return macro == k_omni_macro;
          });
      po.Macros.cend() == omni_defined) {
      // enable for the tool
      po.Macros.emplace_back(k_omni_macro, /*isUndef*/ false);
    } else {
      auto &[_, is_undef] = *omni_defined;
      // todo: warning if `true == is_undef`?
      is_undef = false;
    }

    // removing force-included reflection header that is being generated
    std::erase_if(po.Includes,
      [&output_dir = cli_args.out](const std::string &s) -> bool {
        // todo: should I remove the exact path?
        return util::is_subpath(s, output_dir);
      });

    // refactorme: use single `<<` call
    llvm::errs() << (po.Macros.empty()
        ? std::string()
        : std::format("\n{}\n",
            po.Macros //
              | std::views::transform([](const auto &pair) {
                  const auto &[macro, is_undef] = pair;
                  return std::format("[debug] preprocessor #{} {}",
                    is_undef ? "undef" : "define",
                    macro);
                }) //
              | std::views::join_with("\n"sv) //
              | util::fmt_fold));

    llvm::errs() << (po.Includes.empty()
        ? std::string()
        : std::format("{}\n",
            po.Includes //
              | std::views::transform([](const auto &inc) {
                  return std::format("[debug] preprocessor #include \"{}\"",
                    inc);
                }) //
              | std::views::join_with("\n"sv) //
              | util::fmt_fold));
  }

  return wci;
}

struct with_ast {
  source_file sf;
  std::unique_ptr<clang::ASTUnit> ast;
  diag_engine_binding diag;

  // will be populated during ASTUnit creation
  std::set<fs::path> includes_deps;
};

std::expected<with_ast, std::string> ast_from_compiler_invocation(
  with_compiler_invocation wci) {
  struct deps_collector_t final: clang::DependencyCollector {
    bool needSystemDependencies() override {
      return false;
    }
  } deps_collector;

  struct action_t final: clang::SyntaxOnlyAction {
    clang::DependencyCollector &dc;
    explicit action_t(clang::DependencyCollector &dc): dc(dc) {}

    bool BeginSourceFileAction(clang::CompilerInstance &CI) override {
      dc.attachToPreprocessor(CI.getPreprocessor());
      return clang::SyntaxOnlyAction::BeginSourceFileAction(CI);
    }
  } action{deps_collector};

  std::unique_ptr<clang::ASTUnit> ast{
    clang::ASTUnit::LoadFromCompilerInvocationAction(wci.ci,
      std::make_shared<clang::PCHContainerOperations>(),
      wci.diag.options,
      wci.diag.engine,
      &action)};

  if (!ast || ast->getDiagnostics().hasUncompilableErrorOccurred())
    return std::unexpected("Failed to build AST Unit.");

  return with_ast{
    .sf = std::move(wci.sf),
    .ast = std::move(ast),
    .diag = std::move(wci.diag),

    .includes_deps = deps_collector.getDependencies()
      | std::views::transform(fn::as<fs::path>) | std::ranges::to<std::set>(),
  };
}

} // namespace pipeline

} // namespace

int main(int argc, char **argv) {
  const std::expected cli_args = cli::parse(argc, argv);
  if (!cli_args) {
    const auto &[code, error] = cli_args.error();
    llvm::errs() << error << '\n';
    return code;
  }

  llvm::errs() << std::format(
    "\nargs:"
    "\n{}",
    format_options(*cli_args));

  const cli::options &args = *cli_args;
  const auto compiler_invocation_for_source_file =
    std::bind_front(pipeline::compiler_invocation_for_source_file,
      args.resource_dir);

  const auto configure_compiler_invocation =
    std::bind_front(pipeline::configure_compiler_invocation, args);

  static const auto collect_matches =
    [](pipeline::with_ast wast) -> meta::source_file_context {
    using namespace clang::ast_matchers;

    // refactorme: refine the interface. Support non-verbose compasibility.
    // For example, errors handling could be a part of monadic
    // transformation chain, the result of which could later be partitioned
    // into [valid, errors]. Only valid require accumulation code.
    return ast::reduce_matches(*wast.ast,
      meta::source_file_context{
        .sf = std::move(wast.sf),
        .file_dependencies = std::move(wast.includes_deps),
      },

      ast::rule{
        .pattern = ast::pattern(classTemplateSpecializationDecl,
          unless(isInStdNamespace()),
          hasAncestor(namespaceDecl(hasName("omni"))),
          hasAncestor(namespaceDecl(hasName("detail"))),
          hasName("_reflected_indexed_type"),
          isTemplateInstantiation(),
          isDefinition()),
        .reduce =
          [&ast = wast.ast->getASTContext()](meta::source_file_context a,
            const clang::ClassTemplateSpecializationDecl &decl) {
            std::expected resolved =
              meta::resolve_reflected_indexed_type(decl, ast, a.resolved_types)
                .transform( //
                  [&a](std::pair<std::size_t, meta::reflected_type_info> r)
                    -> void {
                    auto &&[index, resolved] = std::move(r);

                    std::visit(
                      [&a, index]<typename T>(T t) {
                        a.resolved_types.emplace(t.id);
                        a.index_by_type.emplace_back(t.id, index);

                        if constexpr (std::same_as<meta::reflectable, T>) {
                          meta::reflectable &r = t;
                          a.reflected.emplace_back(std::move(r));
                        } else if constexpr (std::same_as<meta::non_reflectable,
                                               T>) {
                          // todo:
                          // Only a limited set of T in _relfected_type<T>
                          // is supported, and it should be reported as
                          // error diagnostics
                        } else {
                          static_assert(
                            std::same_as<meta::already_reflected, T>);
                          // todo: log for info?
                        }
                      },
                      std::move(resolved.type));

                    std::ranges::copy(resolved.dependencies
                        | std::views::transform(&meta::reflectable::id),
                      std::inserter(a.resolved_types,
                        a.resolved_types.begin()));

                    std::ranges::copy(resolved.dependencies
                        | std::views::transform(&meta::reflectable::id),
                      std::inserter(a.resolved_as_dependency,
                        a.resolved_as_dependency.begin()));

                    std::ranges::move(resolved.dependencies,
                      std::back_inserter(a.reflected));
                  });

            if (!resolved) {
              a.errors.emplace_back(std::move(resolved).error());
              return a;
            }

            return a;
          },
      });
  };

  llvm::errs() << std::format("\n[info] running for file: {}\t\r",
    args.source.path.string());

  const std::expected processed_source =
    compiler_invocation_for_source_file(args.source)
      .transform(configure_compiler_invocation)
      .and_then(pipeline::ast_from_compiler_invocation)
      .transform(collect_matches)
      .transform_error([filename = args.source.path.string()](std::string error) {
        return std::format("Failed to process '{}' with error: {}",
          filename,
          std::move(error));
      })
      .transform([](meta::source_file_context ctx) {
        const render::log print_log{
          .dependencies = ctx.resolved_as_dependency,
          .index_by_type = ctx.index_by_type | std::ranges::to<std::map>(),
        };

        llvm::errs() << std::format(
          "\n[info] processed source: {}"
          "\n\n[info] -- types --------\n{}"
          "\n\n[info] -- includes --------\n{}",
          source_file::path_as_string(ctx.sf),

          ctx.reflected //
            | std::views::transform(print_log) //
            | std::views::join_with("\n\n"sv) //
            | util::fmt_fold,

          ctx.file_dependencies //
            | std::views::transform([](const fs::path &p) { return p.string(); })
            | std::views::join_with("\n"sv) //
            | util::fmt_fold);

        return ctx;
      });

  const auto reflection_header_path = //
    [&o = args.out](const source_file &instrumented_source) {
      // ad hoc to disable fs exception throwing
      [[maybe_unused]] std::error_code ec;
      return fs::is_directory(o, ec) //
        ? fs::path(o)
          / std::format("{}_omni_reflection_header.h",
            instrumented_source.path.string())
        : o;
    };

  const std::expected instrumented_source =
    processed_source
      .and_then(render::prepare_reflection_context)
      .and_then([reflection_header_path](render::reflection_context ctx) {
        const fs::path out = reflection_header_path(ctx.instrumented_source_file);

        llvm::errs() << std::format(
          "\n[info] creating reflection header: {}",
          out.generic_string());

        return util::create_file_for_writing(out) //
          .and_then(
            std::bind_front(render::generate_reflection, std::move(ctx)))
          .and_then([out](render::reflection_context ctx) {
            llvm::errs() << std::format("\n[info] writing deps file for: {}",
              out.generic_string());

            return render::write_dependencies_file(out, ctx.file_dependencies)
              .transform([out](fs::path deps) {
                return std::pair(out, std::move(deps));
              });
          });
      });

  if (!instrumented_source) {
    llvm::errs() << std::format("\n[error] generating reflection header: {}.",
      instrumented_source.error());
    return -1;
  }

  const auto &[header, deps] = *instrumented_source;
  llvm::errs() << std::format(
    "\n[info] generated header: {}"
    "\n[info] generated deps file: {}",
    header.generic_string(),
    deps.generic_string());

  llvm::errs() << "\n[info] done.\n";

  return 0;
}

namespace {
std::expected<cli::options, std::pair<int, std::string>> cli::parse(int argc,
  const char *const *argv) noexcept {
  // refactorme: app description and cli parameters should be at the start
  // of the file for clear undestanding, before 'main'
  CLI::App app{
    "\nC++ reflection header generator."
    "\n"
    "\nGenerates a .hpp header containing reflection metadata for a given .cpp file."
    "\nThe header must be implicitly included at the start of the translation unit."
    "\n  WARNING: Uses compile time counters via friend injection, which is not guaranteed"
    "\n           by the C++ Standard to be consistent between compiler implementations.",
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

  cli_source_file cli_source;
  app
    .add_option("-s,--source",
      cli_source,
      ".cpp file path to run the tool on.")
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

  source_file cli_source_with_flags;

  // refactorme: use ranges
  // Validation here
  app.callback([&] {
    // -- check source
    {
      cli_source.path = fs::absolute(std::move(cli_source.path)).lexically_normal();

      // refactorme: 'unquote' string function
      if (cli_source.output_substr.starts_with("\'")
        || cli_source.output_substr.starts_with("\""))
        cli_source.output_substr = std::move(cli_source.output_substr).substr(1);

      if (cli_source.output_substr.ends_with("\'")
        || cli_source.output_substr.ends_with("\""))
        cli_source.output_substr = std::move(cli_source.output_substr)
                                     .substr(0, cli_source.output_substr.size()
                                                   - 1);

      if (!fs::exists(cli_source.path)) {
        throw CLI::ValidationError("Non-existent source:",
          cli_source.path.generic_string());
      }
    }

    const std::vector flags = app.remaining();
    if (!cli_comp_db) {
      cli_source_with_flags = {
        .path = std::move(cli_source.path),
        .flags = flags,
      };

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

    llvm::errs() << std::format(
      "\n[debug] disambiguation substring for {}: {}",
      cli_source.path.string(),
      cli_source.output_substr);

    std::vector commands =
      (*loaded)->getCompileCommands(cli_source.path.generic_string());

    std::vector resolved = commands //
      | std::views::as_rvalue
      | std::views::filter([&cli_source](
                             const clang::tooling::CompileCommand &c) -> bool {
          return c.Output.contains(cli_source.output_substr);
        })
      | std::ranges::to<std::vector>();

    if (1 != resolved.size()) {
      throw CLI::ValidationError(
        "Failed to resolve source from compilation db",
        std::format("{}: failed to resolve from {} commands by substring `{}`",
          cli_source.path.generic_string(),
          commands.size(),
          cli_source.output_substr));
    }

    cli_source_with_flags = {
      .path = cli_source.path,
      .flags = std::move(resolved.front().CommandLine),
    };
  });

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    std::stringstream ss;
    const auto code = app.exit(e, ss, ss);
    return std::unexpected(std::pair{code, std::move(ss).str()});
  }

  return options{
    .source = std::move(cli_source_with_flags),
    .out = std::move(cli_out),
    .resource_dir = std::move(cli_resource_dir),
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
auto pipeline::compiler_invocation_for_source_file(const fs::path &resource_dir,
  const source_file &sf) noexcept
  -> std::expected<with_compiler_invocation, std::string> {
  namespace options = clang::driver::options;

  const auto to_vector_of_raw_pointers =
    [](const std::vector<std::string> &v) -> std::vector<const char *> {
    return v //
      | std::views::transform(&std::string::c_str)
      | std::ranges::to<std::vector>();
  };

  // note: diag to AST is 1:1, has to be recreated each time...
  // must outlive the ast

  diag_engine_binding diag = std::invoke([]() -> diag_engine_binding {
    std::shared_ptr options = std::make_shared<clang::DiagnosticOptions>();
    llvm::IntrusiveRefCntPtr engine =
      new clang::DiagnosticsEngine(new clang::DiagnosticIDs(),
        *options,
        new clang::TextDiagnosticPrinter(llvm::errs(), *options));
    return {
      .options = std::move(options),
      .engine = std::move(engine),
    };
  });

  const auto &[source, flags] = sf;

  llvm::errs() << std::format(
    "\n[info] input flags:"
    "\n  [{}]\n",
    flags | std::views::join_with("\n  "sv) | util::fmt_fold);

  const std::expected normalized_args = parse_driver_args(flags);
  if (!normalized_args)
    return std::unexpected(normalized_args.error());

  const bool driver_mode_cl = std::invoke([&normalized_args] {
    if (const auto *dm =
          normalized_args->getLastArgNoClaim(options::OPT_driver_mode))
      return std::string_view(dm->getValue()) == "cl";
    return false;
  });

  const bool msvc_used =
    std::invoke([&flags, &normalized_args, driver_mode_cl] {
      const bool invoked_as_cl = std::invoke([&flags] {
        if (flags.empty())
          return false;
        const std::string exe = fs::path(flags.front()).filename().string();
        return exe == "cl.exe" || exe == "clang-cl" || exe == "clang-cl.exe";
      });

      const bool has_msvc_style_args =
        normalized_args->hasArg(options::OPT__SLASH_D,
          options::OPT__SLASH_U,
          options::OPT__SLASH_I,
          options::OPT__SLASH_FI);

      return driver_mode_cl || invoked_as_cl || has_msvc_style_args;
    });

  const std::vector<std::string> cc1_driver_args =
    std::invoke([&] -> std::vector<std::string> {
      // MSVC/clang-cl: pass through the original driver args to preserve
      // spellings (/I, /FI, -std:c++20, etc). Only drop argv0 and force AST
      // mode.
      if (msvc_used) {
        std::vector<std::string> out;
        out.reserve(8 + flags.size());

        out.emplace_back("omnirefl");
        if (!driver_mode_cl)
          out.emplace_back("--driver-mode=cl");

        // keep the rest verbatim (except argv0)
        std::ranges::copy(flags //
            | std::views::drop(1) //
            | std::views::filter([](std::string_view s) { return !s.empty(); }),
          std::back_inserter(out));

        // force AST-only and tool resource-dir (last-wins)
        out.emplace_back("-fsyntax-only");
        out.emplace_back(
          std::format("-resource-dir={}", resource_dir.generic_string()));
        return out;
      }

      // GCC-like: keep only AST-relevant options.
      std::vector<std::string> out;
      out.reserve(16 + normalized_args->size());

      out.emplace_back("omnirefl");
      out.emplace_back(source.generic_string());
      out.emplace_back("-fsyntax-only"); //< AST only
      out.emplace_back(
        std::format("-resource-dir={}", resource_dir.generic_string()));

      // prevents from picking up on another compiler's C++ < std libs
      out.emplace_back("-nostdinc++");

      const std::vector<std::string> ast_related_args =
        filter_ast_related_args(*normalized_args);

      // ensure tool-provided resource-dir wins
      std::ranges::copy(ast_related_args
          | std::views::filter([](std::string_view s) {
              return !s.starts_with("-resource-dir"sv);
            }),
        std::back_inserter(out));

      return out;
    });

  const std::string driver_triple = std::invoke([msvc_used, &normalized_args] {
    const std::string target_triple = std::invoke([&normalized_args] {
      if (const auto *opt =
            normalized_args->getLastArgNoClaim(options::OPT_target))
        return llvm::Triple::normalize(opt->getValue());
      return llvm::sys::getProcessTriple();
    });

    llvm::Triple triple(target_triple);
    if (msvc_used) {
      triple.setOS(llvm::Triple::Win32);
      triple.setEnvironment(llvm::Triple::MSVC);
    }
    return triple.str();
  });

  // ad hoc: this is a heavy-weight solution just to get proper argument
  // translations (for other compilers' commands) and to resolve system
  // include paths...
  clang::driver::Driver driver(msvc_used ? "clang-cl" : "clang",
    driver_triple,
    *diag.engine,
    "omnirefl reflection tool");
  driver.setCheckInputsExist(false);

  const std::unique_ptr<clang::driver::Compilation> compilation(
    driver.BuildCompilation(
      llvm::ArrayRef(to_vector_of_raw_pointers(cc1_driver_args))));

  if (!compilation || compilation->getJobs().empty()) {
    return std::unexpected(
      std::format("Failed to build compilatioin for: {}.\n",
        source.generic_string()));
  }

  const auto &compilation_args = compilation->getJobs().begin()->getArguments();

  llvm::errs() << std::format(
    "\n[info] using cc1 args:"
    "\n  [{}]\n",
    compilation_args //
      | std::views::transform(fn::as<std::string_view>) //
      | std::views::join_with("\n  "sv) //
      | util::fmt_fold);

  std::shared_ptr compiler_invocation =
    std::make_shared<clang::CompilerInvocation>();

  if (!clang::CompilerInvocation::CreateFromArgs(*compiler_invocation,
        compilation_args,
        *diag.engine)) {
    return std::unexpected("Failed to create CompilerInvocation.");
  }

  {
    clang::HeaderSearchOptions &o = compiler_invocation->getHeaderSearchOpts();

    // own libc++ includes are bundled
    if (!msvc_used)
      o.UseStandardCXXIncludes = false;
    o.ResourceDir = resource_dir.generic_string();

    o.AddPath(
      // todo: this should be configured at compile time
      (resource_dir / "include/x86_64-unknown-linux-gnu/c++/v1")
        .generic_string(),
      clang::frontend::IncludeDirGroup::CXXSystem,
      // todo: I have no idea what are these parameters. comment
      /*IsFramework=*/false,
      /*IgnoreSysRoot=*/false);

    // todo: this should be configured at compile time
    o.AddPath((resource_dir / "include/c++/v1").generic_string(),
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

    p.Macros.emplace_back("OMNI_TOOL_RUN", /*isUndef*/ false);
  }

  // do not need this noise when parsing the AST
  compiler_invocation->getDiagnosticOpts().IgnoreWarnings = 1;

  return with_compiler_invocation{
    .sf = sf,
    .ci = std::move(compiler_invocation),
    .diag = std::move(diag),
  };
}

// used for unique ids.
meta::type_id render_type_id(const clang::ASTContext &ast,
  const clang::Type *type) {
  clang::PrintingPolicy p(ast.getLangOpts());
  p.FullyQualifiedName = true;
  p.SuppressScope = false;
  p.SuppressUnwrittenScope = false;
  p.SuppressTagKeyword = false;
  p.PrintAsCanonical = true;
  // P.PrintTemplateArguments = true;

  // "no qualifiers" (not const, not volatile, not restrict).
  const unsigned qual_flags = 0;
  return clang::QualType(type, qual_flags).getAsString(p);
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

  if (template_args_list.size() <= n) {
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

// todo: remove maybe_unused when generated-header reflection uses this helper.
[[maybe_unused]] std::expected<int, std::string> get_template_value_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) noexcept {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() <= n) {
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

meta::nm_qual_type meta::nm_qual_type::from_decl(
  const clang::TagDecl *td) noexcept {
  // refactorme: function bodies are also captured as enclosing_records. I think
  // capturing them for rendering in diagnostics is somewhat useful, but I'd
  // need to introduce a sum type like {namespace|struct|function}.
  auto [namespaces, enclosing_records] =
    std::invoke([&decl_ctx = *td->getDeclContext()] {
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
          // std::optional for enclosing records to specifically signal
          // unnamed records. I need to get back this later, because rendering
          // this is probably more difficult...
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
      const clang::IdentifierInfo *id = td->getIdentifier();
      if (!id)
        return std::nullopt;

      // "no qualifiers" (not const, not volatile, not restrict).
      const unsigned qual_flags = 0;
      std::string name =
        // fixme:
        // ad hoc: for template specializations use the printing
        // policy to render <template args> properly.
        !llvm::isa<clang::ClassTemplateSpecializationDecl>(td)
        ? id->getName().str()
        : clang::QualType{td->getTypeForDecl(), qual_flags}.getAsString(
            std::invoke([] {
              clang::PrintingPolicy p{{}};

              p.SuppressTagKeyword = true; //< no 'struct', 'class' or 'enum'
              p.SuppressScope = false; //< namespaces or enclosing records
                                       // for <template args>
              p.PrintAsCanonical = true;

              return p;
            }));

      // ad hoc: (see above) for template specializations strip all scopes
      // to the left of the last '::' before '<', keeping template args
      // intact.
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

meta::type_definition resolve_definition(const clang::SourceManager &sm,
  const clang::TagDecl *td) noexcept {
  const clang::DeclContext &decl_ctx = *td->getDeclContext();

  // refactorme: ugleee
  using td_flags = decltype(meta::type_definition::definition_flags);
  const td_flags td_local = decl_ctx.isFunctionOrMethod() //
    ? meta::type_definition::local
    : meta::type_definition::none;

  const td_flags td_non_public = td_flags::none; //< todo:

  const clang::SourceLocation loc = td->getLocation();
  return {
    .type_name = meta::nm_qual_type::from_decl(td),
    .location =
      {
        .source_file = get_declaration_source_file(*td, sm),
        .line = sm.getSpellingLineNumber(loc),
        .column = sm.getSpellingColumnNumber(loc),
      },
    // refactorme: enable bitwise operations
    .definition_flags = td_flags(td_local | td_non_public),
  };
}

// view of alias declarations considered for reflections, like
// using type = user_struct;
util::viewable_range_of<const clang::Type *> auto member_aliases_view(
  const clang::ASTContext &ast,
  const clang::CXXRecordDecl &rd) {
  const auto to_canonical_type =
    [&ast](const clang::TypedefNameDecl *d) -> const clang::Type * {
    return ast.getCanonicalType(d->getUnderlyingType().getUnqualifiedType())
      .getTypePtr();
  };

  const auto is_supported_alias = //
    [](const clang::TypedefNameDecl *d) {
      return std::ranges::any_of(meta::k_supported_member_aliases,
        std::bind_front(std::equal_to<std::string_view>{},
          fn::as<std::string_view>(d->getName())));
    };

  return rd.decls() //
    | std::views::filter([](const clang::Decl *d) {
        const clang::Decl::Kind k = d->getKind();
        return (clang::Decl::TypeAlias == k || clang::Decl::Typedef == k);
      }) //
    | std::views::transform([](const clang::Decl *d) {
        return llvm::cast<clang::TypedefNameDecl>(d);
      }) //
    | std::views::filter(is_supported_alias)
    | std::views::transform(to_canonical_type);
}

util::viewable_range_of<const clang::CXXRecordDecl *> auto public_bases_view(
  const clang::CXXRecordDecl *rd) {
  const auto is_public = //
    [](const clang::CXXBaseSpecifier &b) {
      return clang::AccessSpecifier::AS_public == b.getAccessSpecifier();
    };

  return rd->bases() //
    | std::views::filter(is_public)
    | std::views::transform(&clang::CXXBaseSpecifier::getType)
    | std::views::transform(&clang::QualType::getTypePtr)
    | std::views::transform(&clang::Type::getAsCXXRecordDecl);
}

util::viewable_range_of<const clang::FieldDecl *> auto public_fields_view(
  const clang::RecordDecl *rd) {
  const auto is_public = //
    [](const clang::FieldDecl *f) {
      return clang::AccessSpecifier::AS_public == f->getAccess();
    };

  return rd->fields() //
    | std::views::filter(is_public) //
    | std::views::filter(std::not_fn(&clang::FieldDecl::isUnnamedBitField));
}

std::vector<const clang::TagType *> recursively_collect_dependency_types(
  const clang::ASTContext &ast,
  const std::set<meta::type_id> &resolved_types,
  const clang::CXXRecordDecl &root) noexcept {
  // todo: monadic return.
  const auto is_supported_template_specialization = //
    [](const clang::CXXRecordDecl *rd) {
      assert(rd);
      return //
        std::ranges::contains(
          std::array{
            clang::Decl::ClassTemplateSpecialization,
            clang::Decl::ClassTemplatePartialSpecialization,
          },
          rd->getKind())
        && std::ranges::contains(meta::k_supported_template_names,
          fn::as<std::string_view>(
            clang::cast<clang::ClassTemplateSpecializationDecl>(rd)
              ->getSpecializedTemplate()
              ->getName()));
    };

  const auto template_specialization_types = //
    [](const util::viewable_range_of<clang::TemplateArgument> auto &args)
    -> util::viewable_range_of<const clang::Type *> auto {
    return args //
      | std::views::transform(&clang::TemplateArgument::getAsType)
      | std::views::transform(&clang::QualType::getTypePtr);
  };

  const auto to_tag_type = //
    [](const clang::Type *t) { return t->getAs<clang::TagType>(); };

  const auto not_in_std = //
    [](const clang::TagType *t) { return !t->getDecl()->isInStdNamespace(); };

  const auto not_resolved = //
    [&ast, &resolved_types](const clang::TagType *t) {
      return !resolved_types.contains(render_type_id(ast, t));
    };

  std::set<const clang::TagType *> collected;

  // only CXXRecordDecl may have dependencies
  std::stack<const clang::CXXRecordDecl *> to_visit;
  to_visit.push(&root);

  while (!to_visit.empty()) {
    const clang::CXXRecordDecl *cur_decl = to_visit.top();
    const clang::TagType *cur_type =
      meta::map_decl_to_canonical_type(ast, cur_decl);
    const meta::type_id cur_id = render_type_id(ast, cur_type);

    to_visit.pop();

    if (collected.contains(cur_type) || resolved_types.contains(cur_id))
      continue;

    std::ranges::move(member_aliases_view(ast, *cur_decl) //
        | std::views::filter(&clang::Type::isEnumeralType) //
        | std::views::transform(to_tag_type) //
        | std::views::filter(not_in_std) //
        | std::views::filter(not_resolved),
      std::inserter(collected, collected.begin()));

    std::ranges::for_each(member_aliases_view(ast, *cur_decl) //
        | std::views::filter(&clang::Type::isRecordType) //
        | std::views::transform(&clang::Type::getAsCXXRecordDecl),
      [&to_visit](const clang::CXXRecordDecl *rd) { to_visit.push(rd); });

    // refactorme: clean up
    // ad hoc: special case for 'tuple<T...>', 'variant<T...>'.
    if (is_supported_template_specialization(cur_decl)) {
      const auto arg_list =
        clang::cast<clang::ClassTemplateSpecializationDecl>(cur_decl)
          ->getTemplateInstantiationArgs()
          .asArray();

      if (1 != arg_list.size()
        || clang::TemplateArgument::Pack != arg_list.front().getKind()
        || std::ranges::any_of(arg_list.front().getPackAsArray()
            | std::views::transform(&clang::TemplateArgument::getKind),
          std::bind_front(std::not_equal_to{},
            clang::TemplateArgument::Type))) {
        llvm::errs() << std::format(
          "\n[info] skipping template specialization '{}': unsupported signature.",
          cur_id);

        continue;
      }

      std::ranges::move(
        template_specialization_types(arg_list.front().getPackAsArray()) //
          | std::views::filter(&clang::Type::isEnumeralType) //
          | std::views::transform(to_tag_type) //
          | std::views::filter(not_in_std) //
          | std::views::filter(not_resolved),
        std::inserter(collected, collected.begin()));

      std::ranges::for_each(
        template_specialization_types(arg_list.front().getPackAsArray()) //
          | std::views::filter(&clang::Type::isRecordType)
          | std::views::transform(&clang::Type::getAsCXXRecordDecl),
        [&to_visit](const clang::CXXRecordDecl *rd) { to_visit.push(rd); });

      // not collecting fields or bases.
      continue;
    }

    // bases
    std::ranges::for_each(public_bases_view(cur_decl),
      [&to_visit](const clang::CXXRecordDecl *rd) { to_visit.push(rd); });

    // public fields
    std::ranges::move(public_fields_view(cur_decl) //
        | std::views::transform(&clang::FieldDecl::getType)
        | std::views::transform(&clang::QualType::getTypePtr)
        | std::views::filter(&clang::Type::isEnumeralType)
        | std::views::transform(to_tag_type) //
        | std::views::filter(not_in_std) //
        | std::views::filter(not_resolved),
      std::inserter(collected, collected.begin()));

    std::ranges::for_each(public_fields_view(cur_decl) //
        | std::views::transform(&clang::FieldDecl::getType)
        | std::views::transform(&clang::QualType::getTypePtr)
        | std::views::filter(&clang::Type::isRecordType)
        | std::views::transform(&clang::Type::getAsCXXRecordDecl),
      [&to_visit](const clang::CXXRecordDecl *rd) { to_visit.push(rd); });

    if (not_in_std(cur_type))
      collected.insert(cur_type);
  }

  collected.erase(meta::map_decl_to_canonical_type(ast, &root));
  return {collected.begin(), collected.end()};
}

auto meta::record_data::from_type(const clang::ASTContext &ast,
  const clang::RecordType *t) -> record_data {
  assert(t);

  const clang::RecordDecl &r_decl = *t->getDecl();
  const clang::CXXRecordDecl *cxx_decl = t->getAsCXXRecordDecl();

  return {
    .public_bases = cxx_decl ? public_bases_view(cxx_decl)
        | std::views::transform(
          std::bind_front(meta::map_decl_to_canonical_type, std::cref(ast)))
        | std::views::transform(std::bind_front(render_type_id, std::cref(ast)))
        | std::ranges::to<std::vector>()
                             : std::vector<std::string>{},

    .public_fields = public_fields_view(&r_decl)
      | std::views::transform(meta::field_data::from_decl)
      | std::ranges::to<std::vector>(),

    .type = r_decl.isStruct() //
      ? meta::record_data::is_struct
      : r_decl.isClass() //
        ? meta::record_data::is_class
        : meta::record_data::is_union,
  };
}

auto meta::enum_data::from_type(const clang::EnumType *t) -> enum_data {
  assert(t);

  const clang::EnumDecl &ed = *t->getDecl();
  util::viewable_range_of<std::string> auto names = //
    ed.enumerators() //
    | std::views::transform(&clang::EnumDecl::getName)
    | std::views::transform(fn::as<std::string_view>);

  return {
    .is_scoped = ed.isScoped(),
    .is_fixed = ed.isFixed(),
    .underlying_type = ed.isFixed() ? ed.getIntegerType().getAsString() : "",
    .enumerators = names | std::ranges::to<std::vector<std::string>>(),
  };
}

// refactorme: this exists only because of std::variant clumsy initialization
meta::reflectable match_reflectable_type(const clang::ASTContext &ast,
  const clang::TagType *t) {
  assert(t);

  using record_or_enum = std::variant<meta::record_data, meta::enum_data>;
  static_assert(std::same_as<record_or_enum, decltype(meta::reflectable::data)>,
    "Inconsistency for reflectable types detected.");

  return {
    .id = render_type_id(ast, t),
    .data = clang::isa<clang::EnumType>(t)
      ? record_or_enum(
          meta::enum_data::from_type(clang::cast<clang::EnumType>(t)))
      : record_or_enum(
          meta::record_data::from_type(ast, clang::cast<clang::RecordType>(t))),
    .definition = resolve_definition(ast.getSourceManager(), t->getDecl()),
  };
}

auto meta::resolve_reflected_type(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  const clang::ASTContext &ast,
  const std::set<type_id> &resolved_types)
  -> std::expected<reflected_type_info, std::string> {
  const auto match_record_type = //
    [&ast](const std::string &id, const clang::RecordType *t)
    -> std::variant<reflectable, non_reflectable, already_reflected> {
    if (t->getDecl()->isInStdNamespace())
      return non_reflectable{.id = id};

    return match_reflectable_type(ast, t);
  };

  // todo: assert template signature
  // omni::detail::_reflected_indexed_type<T, I>
  std::expected template_arg = get_template_type_arg(template_decl, 0);
  if (!template_arg)
    return std::unexpected(std::move(template_arg).error());

  const clang::Type &template_arg_type = **template_arg;
  const meta::type_id id = render_type_id(ast, &template_arg_type);
  // todo: C-arrays, pointers?

  if (resolved_types.contains(id)) {
    return reflected_type_info{
      .type = already_reflected{.id = id},
      .dependencies = {}, //< refactorme: dependencies here make no sense
    };
  }

  // not a struct|class|union|enum
  if (!clang::isa<clang::TagType>(template_arg_type)) {
    return reflected_type_info{
      .type = non_reflectable{.id = id},
      .dependencies = {},
    };
  }

  if (clang::isa<clang::EnumType>(template_arg_type)) {
    const auto *enum_type = clang::cast<clang::EnumType>(&template_arg_type);
    return reflected_type_info{
      .type =
        reflectable{
          .id = id,
          .data = enum_data::from_type(enum_type),
          .definition =
            resolve_definition(ast.getSourceManager(), enum_type->getDecl()),
        },
      .dependencies = {},
    };
  }

  const auto *record_type = clang::cast<clang::RecordType>(&template_arg_type);
  return reflected_type_info{
    .type = match_record_type(id, record_type),
    .dependencies = recursively_collect_dependency_types(ast,
                      resolved_types,
                      *record_type->getAsCXXRecordDecl())
      | std::views::transform(
        std::bind_front(match_reflectable_type, std::cref(ast)))
      | std::ranges::to<std::vector>(),
  };
}

auto meta::resolve_reflected_indexed_type(
  const clang::ClassTemplateSpecializationDecl &decl,
  const clang::ASTContext &ast,
  const std::set<type_id> &resolved_types)
  -> std::expected<std::pair<std::size_t, reflected_type_info>, std::string> {
  return get_template_value_arg(decl, 1) //
    .and_then([&](std::size_t index) {
      return resolve_reflected_type(decl, ast, resolved_types)
        .transform([index](reflected_type_info r) {
          return std::pair(index, std::move(r));
        });
    });
}

namespace render::impl {

// render `_omni_{root}_as_root` for `root::inner_type` as input
std::string enclosing_root_as_dependent(const meta::nm_qual_type &inner_type) {
  assert(!inner_type.enclosing_records.empty());
  assert(!inner_type.enclosing_records.front().empty()
    && "can't access unnamed root");

  std::vector<std::string_view> elems = inner_type.namespaces
    | std::views::transform(fn::as<std::string_view>)
    | std::views::filter([](std::string_view s) { return !s.empty(); })
    | std::ranges::to<std::vector>();
  elems.push_back(inner_type.enclosing_records.front());

  return std::format("_omni_{}_as_root",
    elems | std::views::join_with("_"sv) | util::fmt_fold);
}

// Makes a qualified type name dependent so it can be used in SFINAE even when
// the root type is only forward-declared.
// Example: `typename _omni_get_outer<Inner>::inner` denotes `outer::inner`.
// Intended to defer nested-name lookup until the enclosing type is complete.
std::string declaration_for_enclosing_root_as_dependent(
  const meta::nm_qual_type &inner_type) {
  std::vector<std::string_view> elems = inner_type.namespaces
    | std::views::transform(fn::as<std::string_view>)
    | std::views::filter([](std::string_view s) { return !s.empty(); })
    | std::ranges::to<std::vector>();
  elems.push_back(inner_type.enclosing_records.front());

  return std::format(
    "template <typename Inner>"
    "\nusing {} = typename _wrt<{}, Inner>::type;",
    enclosing_root_as_dependent(inner_type),
    elems | std::views::join_with("::"sv) | util::fmt_fold);
}

// render `typename _omni_{root}_as_root<{param}>::inner`
std::string qualified_inner_type_from_fwd_root(
  const meta::nm_qual_type &inner_type,
  std::string_view param) {
  assert(inner_type.name && "unnamed types are not yet supported");
  const std::string root =
    std::format("{}<{}>", enclosing_root_as_dependent(inner_type), param);

  std::vector<std::string_view> elems = inner_type.enclosing_records
    | std::views::drop(1) | std::views::transform(fn::as<std::string_view>)
    | std::ranges::to<std::vector>();
  elems.insert(elems.begin(), root);
  elems.emplace_back(*inner_type.name);

  return std::format("typename {}",
    elems | std::views::join_with("::"sv) | util::fmt_fold);
}

template <meta::reflectable_data Data>
std::string_view reflectable_tag(const Data &d) {
  if constexpr (std::same_as<meta::record_data, Data>) {
    switch (d.type) {
    case meta::record_data::is_class:
      return "class";
    case meta::record_data::is_union:
      return "union";
    case meta::record_data::is_struct:
    default:
      return "struct";
    }
  } else {
    static_assert(std::same_as<meta::enum_data, Data>);
    return "enum";
    // d.is_scoped ? "enum class" : "enum";
    // ^^^ gives "warning: elaborated-type-specifier for a scoped enum must not
    // use the ‘class’ keyword"
  }
}

template <meta::reflectable_data Data>
std::string_view reflectable_entity(const Data &) {
  if constexpr (std::same_as<meta::record_data, Data>)
    return "record";
  else {
    static_assert(std::same_as<meta::enum_data, Data>);
    return "enumeration";
  }
}

template <meta::reflectable_data Data>
std::string reflectable_head(const meta::nm_qual_type &t, const Data &d) {
  // to support, I need to store the decl name for `decltype(instance)`
  assert(t.name && "unnamed structs not supported");

  return std::format(
    "template <typename T>"
    "\nstruct _reflected<{0} {1}, T> {{"
    "\n  static_assert(std::is_same<{0} {1}, T>::value,"
    "\n    \"omnirefl: unexpected types mismatch, try regenerating\");"
    "\n"
    "\n  using type = T;"
    "\n"
    "\n  static constexpr omni::reflected_entity entity() noexcept {{"
    "\n    return omni::reflected_entity::{2};"
    "\n  }}"
    "\n"
    "\n  static constexpr auto name() noexcept"
    "\n    -> const char(&)[sizeof(\"{3}\")] {{"
    "\n    return \"{3}\";"
    "\n  }}",

    // 0
    reflectable_tag(d),

    // 1
    format_nm_qual_type(t),

    // 2
    reflectable_entity(d),

    // 3
    t.name.value_or("(unnamed)"));
}

// SFINAE specialization for inner types of forward-declared types.
template <meta::reflectable_data Data>
std::string inner_reflectable_head(const meta::nm_qual_type &t, const Data &d) {
  // to support, I need to store the decl field name for
  // `decltype(std::declval<root>().inner)
  assert(t.name && "unnamed inner structs not supported");

  const std::string access_root_for =
    std::format("{}<T>", enclosing_root_as_dependent(t));

  std::vector<std::string_view> elems = t.enclosing_records
    | std::views::drop(1) | std::views::transform(fn::as<std::string_view>)
    | std::ranges::to<std::vector>();
  elems.insert(elems.begin(), access_root_for);
  elems.push_back(*t.name);

  return std::format(
    "template <typename T>"
    "\nstruct _reflected<T, typename std::enable_if<"
    "\n  std::is_same<T, typename {0}>::value, T>::type> {{"
    "\n  "
    "\n  using type = T;"
    "\n"
    "\n  static constexpr omni::reflected_entity entity() noexcept {{"
    "\n    return omni::reflected_entity::{1};"
    "\n  }}"
    "\n"
    "\n  static constexpr auto name() noexcept"
    "\n    -> const char(&)[sizeof(\"{2}\")] {{"
    "\n    return \"{2}\";"
    "\n  }}",

    elems | std::views::join_with("::"sv) | util::fmt_fold,
    reflectable_entity(d),
    t.name.value_or("(unnamed)"));
}

template <meta::reflectable_data Data>
std::string indexed_reflectable_head(const meta::nm_qual_type &t,
  const Data &d,
  std::size_t index) {
  return std::format(
    "template <typename T>"
    "\nstruct _reflected<T, typename std::enable_if<"
    "\n  decltype(reflected_index_match<{0}, T>::exists(0))::value, T>::type> {{"
    "\n  "
    "\n  using type = T;"
    "\n"
    "\n  static constexpr omni::reflected_entity entity() noexcept {{"
    "\n    return omni::reflected_entity::{1};"
    "\n  }}"
    "\n"
    "\n  static constexpr auto name() noexcept"
    "\n    -> const char(&)[sizeof(\"{2}\")] {{"
    "\n    return \"{2}\";"
    "\n  }}",

    index,
    reflectable_entity(d),
    t.name.value_or("(unnamed)"));
}

std::string reflectable_body(const meta::enum_data &d) {
  return std::format(
    "\n  constexpr static auto enumerators() noexcept"
    "\n    -> std::array<std::pair<type, const char*>, {}> {{"
    "\n      return {{{{"
    "\n        {},"
    "\n      }}}};"
    "\n    }}"
    "\n}};",
    d.enumerators.size(),
    d.enumerators //
      | std::views::transform([](std::string_view e) {
          return std::format("{{type::{0}, \"{0}\"}}", e);
        })
      | std::views::join_with(",\n        "sv) //
      | util::fmt_fold);
}

std::string reflectable_body(const meta::record_data &d) {
  constexpr auto format_field = //
    [](const auto &field_index) {
      const auto &[f, index] = field_index;
      return std::format(
        "  struct {0}_t {{"
        "\n    static constexpr std::size_t index() noexcept {{ return {1}; }}"
        "\n"
        "\n    static constexpr auto name() noexcept"
        "\n      -> const char(&)[sizeof(\"{0}\")] {{"
        "\n      return \"{0}\";"
        "\n    }}"
        "\n"
        "\n    static constexpr bool is_const() noexcept {{ return {2}; }}"
        "\n    static constexpr bool is_mutable() noexcept {{ return {3}; }}"
        "\n"
        "\n    template <typename _T>"
        "\n    static constexpr auto value(const _T &t) noexcept"
        "\n      -> {4} {{"
        "\n      return t.{0};"
        "\n    }}"
        "\n"
        "\n    template <typename _T, typename V>"
        "\n    static void set_value(_T &t, V &&v) {{"
        "\n      t.{0} = std::forward<V>(v);"
        "\n    }}"
        "\n  }};",
        // 0
        f.name,

        // 1
        index,

        // 2
        bool(meta::field_data::as_const == f.qualified),

        // 3
        bool(meta::field_data::as_mutable == f.qualified),

        // 4
        f.is_bitfield ? std::format("decltype(t.{})", f.name)
                      : std::format("const decltype(t.{})&", f.name));
    };

  return std::format(
    "{}"
    "\n"
    // todo: consider a comment here for ignored fields
    "\n  using own_public_fields_t ="
    "\n    std::tuple<{}>;"
    "\n}};",

    d.public_fields.empty()
      ? std::string("\n  // no reflectable fields detected")
      : std::format("\n{}",
          util::indexed(d.public_fields) //
            | std::views::transform(format_field)
            | std::views::join_with("\n\n"sv) //
            | util::fmt_fold),

    d.public_fields //
      | std::views::transform(&meta::field_data::name)
      | std::views::transform(
        [](std::string_view f) { return std::format("{}_t", f); }) //
      | std::views::join_with(",\n      "sv) //
      | util::fmt_fold);
}

} // namespace render::impl

auto render::prepare_reflection_context(meta::source_file_context ctx)
  -> std::expected<reflection_context, std::string> {
  using partition_result =
    std::tuple<std::vector<reflection_context::forward_declarable>,
      std::vector<reflection_context::nested_type>,
      std::vector<reflection_context::indexed_type>>;

  const auto partition_types = //
    [index_by_type_id =
        ctx.index_by_type | std::views::as_rvalue | std::ranges::to<std::map>(),
      &resolved_as_dependency = ctx.resolved_as_dependency](
      std::vector<meta::reflectable> types)
    -> std::expected<partition_result, std::string> {
    std::expected<partition_result, std::string> accum{};

    auto &[fwd, nested, indexed] = *accum;
    std::vector<meta::reflectable> errors;

    for (meta::reflectable &&t : types | std::views::as_rvalue) {
      const std::optional index = index_by_type_id.contains(t.id)
        ? std::optional(index_by_type_id.at(t.id))
        : std::nullopt;

      const bool as_dependency = resolved_as_dependency.contains(t.id);

      const bool is_nested = !t.definition.type_name.enclosing_records.empty();
      const bool is_unnamed = !t.definition.type_name.name;
      const bool is_public_non_local =
        meta::type_definition::none == t.definition.definition_flags;

      // fixme: if a generated reflected type can be forward declared, the
      // fallback specialization must not be generated. Otherwise generated-header
      // reflection emits both the named/nested specialization and fallback
      // specialization, which Clang diagnoses as ambiguous _reflected<T>
      // metadata.
      if (std::visit(
            [is_nested, is_unnamed, is_public_non_local]<
              meta::reflectable_data Data>(const Data &data) {
              if (is_nested || is_unnamed || !is_public_non_local)
                return false;

              if constexpr (std::same_as<meta::enum_data, Data>)
                return data.is_scoped || data.is_fixed;
              else {
                static_assert(std::same_as<meta::record_data, Data>);
                return true;
              }
            },
            t.data)) {
        if (index) {
          llvm::errs() << std::format(
            "\n[debug] generated fallback '{}' type '{}' will be rendered as forward-declarable.",
            *index,
            t.id);
        }

        fwd.push_back({
          .type = std::move(t),
          .index = index,
          .as_dependent = as_dependency,
        });

        continue;
      }

      // fixme: case when nested was reflected, but root was not.
      // fixme: check if enclosing root is public && non-local. As of now this
      // info is not collected/resolved: only names are collected.
      if (is_nested
        // ad hoc to distinguish from fallback-specialized types that has non-empty
        // enclosing_records. Unnamed nested types can be supported, but
        // the distinction should be stronger.
        && !is_unnamed
        && !t.definition.type_name.enclosing_records.front().empty()) {
        if (index) {
          llvm::errs() << std::format(
            "\n[debug] generated fallback '{}' type '{}' will be rendered as nested forward-declarable.",
            *index,
            t.id);
          llvm::errs() << std::format("\n[debug] is_nested: {}", is_nested);
          llvm::errs() << std::format("\n[debug] enclosing_records: [{}]",
            t.definition.type_name.enclosing_records
              | std::views::join_with(", "sv) | util::fmt_fold);
        }

        nested.push_back({
          .type = std::move(t),
          .index = index,
          .as_dependent = as_dependency,
        });

        continue;
      }

      // fixme(high): some local/unnamed fallback-specialized routes still fail.
      // The generated guard is non-mutating, so re-triage the remaining
      // failures as wrong/duplicate metadata or missing generated fallback
      // specializations.
      if (index) {
        indexed.push_back({
          .type = std::move(t),
          .index = *index,
        });

        continue;
      }

      // fixme: generated-header reflection does not instrument supported dependency types
      // reachable through reflected_call argument types when those dependency
      // types cannot be forward-declared. Incidental reflected_call
      // registrations must not make those dependencies supported.
      errors.push_back(std::move(t));
    }

    // todo:
    // - fwd: not nested && non-local && named
    // - nested: nested && (enclosing root is non-local && named)
    // - generated fallback: everything else discovered through reflected_call
    // - error if failed to classify. Collect errors, aggregate and report as
    // an error string

    if (errors.empty())
      return accum;

    // todo: list types. "qualified::type declared at: loc"
    return std::unexpected("non-reflectable types: []");
  };

  return partition_types(std::move(ctx.reflected))
    .transform([&sf = ctx.sf, &deps = ctx.file_dependencies]<typename Tuple>(
                 Tuple &&tuple) {
      return std::apply(
        [&]<typename F, typename N, typename I>(F &&fwd,
          N &&nested,
          I &&indexed) {
          return reflection_context{
            .instrumented_source_file = std::move(sf),

            .fwd_declarables = std::forward<F>(fwd),
            .nested = std::forward<N>(nested),
            .indexed = std::forward<I>(indexed),

            .file_dependencies = std::move(deps),
          };
        },
        std::forward<Tuple>(tuple));
    });
}

auto render::generate_reflection(reflection_context ctx, std::ofstream file)
  -> std::expected<reflection_context, std::string> {
  static constexpr auto cmp_roots = //
    [](const meta::nm_qual_type &lhs, const meta::nm_qual_type &rhs) -> bool {
    return std::tie(lhs.namespaces, lhs.name)
      < std::tie(rhs.namespaces, rhs.name);
  };

  const std::set access_roots = ctx.nested
    | std::views::transform([](const reflection_context::nested_type &n) {
        return n.type.definition.type_name;
      })
    | std::ranges::to<std::set>(cmp_roots);

  // refactorme: should just maintain within the source file context.
  // ad hoc for rendering bases
  const std::map type_name_by_id = std::invoke(
    [](const auto &...types) {
      auto to_pairs = [](const auto &range) {
        return range //
          | std::views::transform(
            [](const auto &v) -> const meta::reflectable & { return v.type; })
          | std::views::transform([](const meta::reflectable &t) {
              return std::pair{t.id, std::addressof(t.definition.type_name)};
            })
          | std::ranges::to<std::map>();
      };

      std::map<meta::type_id, const meta::nm_qual_type *> out{};
      (out.merge(to_pairs(types)), ...);
      return out;
    },
    ctx.fwd_declarables,
    ctx.nested,
    ctx.indexed);

  static constexpr auto format_head = //
    []<typename S>(const S &r) {
      using rc = reflection_context;
      const auto &type_name = r.type.definition.type_name;

      return std::visit(
        [&](const meta::reflectable_data auto &d) {
          if constexpr (std::same_as<rc::forward_declarable, S>)
            return impl::reflectable_head(type_name, d);
          else if constexpr (std::same_as<rc::nested_type, S>)
            return impl::inner_reflectable_head(type_name, d);
          else {
            static_assert(std::same_as<rc::indexed_type, S>);
            return impl::indexed_reflectable_head(type_name, d, r.index);
          }
        },
        r.type.data);
    };

  // fixme: handle fallback-specialized bases
  // fixme: reflected public fields do not include transitive public bases in
  // generated-header reflection. A direct public base is reflected, but fields from a public
  // grand-base are missing from public_fields().
  const auto format_public_bases = //
    [&type_name_by_id](const meta::record_data &r) {
      const auto fetch = [&type_name_by_id](const meta::type_id &id)
        -> const meta::nm_qual_type & { return *type_name_by_id.at(id); };

      const auto format = //
        [](const meta::nm_qual_type &t) {
          if (t.enclosing_records.empty())
            return format_nm_qual_type(t);
          return impl::qualified_inner_type_from_fwd_root(t, "T");
        };

      return std::format("\n  using public_bases_t = std::tuple<{}>;",
        r.public_bases //
          | std::views::transform(fetch) //
          | std::views::transform(format) //
          | std::views::join_with(",\n    "sv) //
          | util::fmt_fold);
    };

  const auto format_body = //
    [format_public_bases](const meta::reflectable &r) {
      return std::visit(
        [format_public_bases]<meta::reflectable_data Data>(const Data &d) {
          if constexpr (std::same_as<meta::enum_data, Data>)
            return impl::reflectable_body(d);
          else {
            static_assert(std::same_as<meta::record_data, Data>);
            return std::format(
              "{}"
              "\n{}",
              format_public_bases(d),
              impl::reflectable_body(d));
          }
        },
        r.data);
    };

  const auto format_reflectable = //
    [format_body]<typename T>(const T &d) {
      return std::format(
        "{}"
        "\n{}",
        format_head(d),
        format_body(d.type));
    };

  const std::vector std_includes = //
    std::invoke([&] {
      std::vector<std::string> o;

      if (std::invoke(
            [](const auto &...rs) {
              return (std::ranges::any_of(rs, [](const auto &r) {
                return std::holds_alternative<meta::enum_data>(r.type.data);
              }) || ...);
            },
            ctx.fwd_declarables,
            ctx.nested,
            ctx.indexed)) {
        o.emplace_back("array");
      }

      return o;
    });

  std::format_to(std::ostreambuf_iterator<char>(file),
    "// This file was generated by the omnirefl tool on {0:%F %T}."
    "\n// Do not modify the contents of this file."
    "\n"
    "\n#define OMNI_INCLUDED_GENERATED_REFLECTION_HEADER" //< must come before
                                                          // omni headers
    "\n"
    "\n// _wrt<U, T>::type == U, but depends on T."
    "\n// Needed because some compilers perform early (non-SFINAE) lookup for `U::...`"
    "\n// when `U` is incomplete unless the nested-name-specifier is dependent."
    "\ntemplate <class U, class>"
    "\nstruct _wrt {{ using type = U; }};"
    "\n"
    // todo: can I _not_ include these? at least not reflected_call.hpp
    "\n#include <omnirefl/reflected_scope.hpp>"
    "\n#include <omnirefl/reflected_call.hpp>" //< todo: separate unique_id into
                                               // a separate header
    // refactorme: ad hoc for enumerators(). No need to include if no enums
    "\n{1}"
    "\n"
    "\n// -- forward declarable reflected types --------"
    "\n{2}"
    "\n"
    "\n// -- enclosing root type accessors --------"
    "\n{3}"
    "\n"
    "\nnamespace omni {{"
    "\nnamespace detail {{"
    "\nnamespace {{"
    "\n"
    "\n// -- reflected types --------"
    "\n{4}"
    "\n"
    "\n// -- reflected inner types --------"
    "\n{5}"
    "\n"
    "\n// -- generated fallback reflected types --------"
    "\n{6}"
    "\n" // todo: ^^^
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
    "\n    omni::compat::void_t<typename omni::detail::_reflected<typename std::decay<T>::type>::type>"
    "\n> : std::true_type {{}};"
    "\n"
    "\n",

    // 0:
    std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()),

    // 1:
    std_includes //
      | std::views::transform(
        [](std::string_view s) { return std::format("#include <{}>", s); })
      | std::views::join_with("\n"sv) //
      | util::fmt_fold,

    // 2:
    ctx.fwd_declarables //
      | std::views::transform(&reflection_context::forward_declarable::type)
      | std::views::transform(render::forward_declaration) //
      | std::views::join_with("\n\n"sv) //
      | util::fmt_fold,

    // 3:
    access_roots //
      | std::views::transform(
        render::impl::declaration_for_enclosing_root_as_dependent) //
      | std::views::join_with("\n\n"sv) //
      | util::fmt_fold,

    // 4:
    ctx.fwd_declarables //
      | std::views::transform(format_reflectable)
      | std::views::join_with("\n\n"sv) //
      | util::fmt_fold,

    // 5:
    ctx.nested //
      | std::views::transform(format_reflectable)
      | std::views::join_with("\n\n"sv) //
      | util::fmt_fold,

    // 6:
    ctx.indexed //
      | std::views::transform(format_reflectable)
      | std::views::join_with("\n\n"sv) //
      | util::fmt_fold);

  // todo: check if file has errors

  return ctx;
}

} // namespace
