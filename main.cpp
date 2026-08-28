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
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RawCommentList.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchersInternal.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/LangStandard.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Job.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Frontend/Utils.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Options/Options.h>
#include <clang/Sema/Sema.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Option/Arg.h>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/Option.h>
#include <llvm/Support/Allocator.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileUtilities.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#pragma GCC diagnostic pop

#pragma pop_macro("_FORTIFY_SOURCE")

#include <CLI/CLI.hpp>
#include <CLI/Error.hpp>
#include <CLI/Validators.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <compare>
#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <regex>
#include <span>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "std_compat.h"

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

template <typename F>
constexpr auto with(F &&f) {
  return [f = std::decay_t<F>(std::forward<F>(f))]<typename T>(T &&value)
    requires std::invocable<decltype(f) &, T &>
  {
    std::invoke(f, value);
    return std::forward<T>(value);
  };
}

template <typename T>
constexpr std::optional<std::remove_cvref_t<T>> maybe(T &&value) {
  if (true == static_cast<bool>(value))
    return std::forward<T>(value);

  return std::nullopt;
}

template <typename T, typename E>
constexpr std::expected<std::remove_cvref_t<T>, std::remove_cvref_t<E>>
  expect(T &&value, E &&error) {
  if (true == static_cast<bool>(value))
    return std::forward<T>(value);

  return std::unexpected(std::forward<E>(error));
}

} // namespace fn

namespace util {

std::expected<std::ofstream, std::string> create_file_for_writing(
  const fs::path &file) {
  if (const fs::path parent = file.parent_path(); !parent.empty()) {
    if (std::error_code ec; (fs::create_directories(parent, ec), ec)) {
      return std::unexpected(
        std::format("invalid parent path: {}", parent.generic_string()));
    }
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

bool is_subpath(const std::filesystem::path &path,
  const std::filesystem::path &base) {
  return base.end() == std::ranges::mismatch(path, base).in2;
}

template <typename V, typename CharT = char>
struct format_range_t {
  V rng;
};

struct format_range_adaptor {
  template <std::ranges::viewable_range R>
    requires std::ranges::input_range<std::views::all_t<R>>
  constexpr auto operator()(R &&r) const {
    return format_range_t<std::views::all_t<R>, char>{
      std::views::all(std::forward<R>(r))};
  }

  template <std::ranges::viewable_range R>
    requires std::ranges::input_range<std::views::all_t<R>>
  friend constexpr auto operator|(R &&r, format_range_adaptor self) {
    return self(std::forward<R>(r));
  }
};

constexpr format_range_adaptor format_range{};

} // namespace util

template <std::ranges::input_range V, typename CharT>
struct std::formatter<util::format_range_t<V, CharT>, CharT> {
  template <typename ParseContext>
  static constexpr auto parse(ParseContext &pc) {
    return std::formatter<std::basic_string_view<CharT>, CharT>{}.parse(pc);
  }

  template <typename FormatContext>
  static constexpr auto format(util::format_range_t<V, CharT> &x,
    FormatContext &ctx) {
    return std::ranges::fold_left(x.rng, ctx.out(), [&](auto out, auto &&e) {
      return std::format_to(out, "{}", e);
    });
  }
};

namespace {

enum class log_level {
  silent,
  error,
  warning,
  info,
  debug,
};

constexpr std::array log_levels{
  std::pair{log_level::silent, "silent"sv},
  std::pair{log_level::error, "error"sv},
  std::pair{log_level::warning, "warning"sv},
  std::pair{log_level::info, "info"sv},
  std::pair{log_level::debug, "debug"sv},
};

struct app_error {
  int code;
  std::string message;
};

struct diagnostics {
  log_level level;

  std::shared_ptr<clang::DiagnosticOptions> clang_options;
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> clang_engine;

  void operator()(log_level requested, std::string_view text) const {
    if (log_level::silent == level || level < requested)
      return;

    llvm::errs() << text;
  }

  template <typename F>
    requires std::invocable<const F &>
  void operator()(log_level requested, F &&format) const {
    if (log_level::silent == level || level < requested)
      return;

    llvm::errs() << std::invoke(std::forward<F>(format));
  }

  static diagnostics from(log_level level) {
    std::shared_ptr options = std::make_shared<clang::DiagnosticOptions>();
    llvm::IntrusiveRefCntPtr engine =
      new clang::DiagnosticsEngine(new clang::DiagnosticIDs(),
        *options,
        new clang::TextDiagnosticPrinter(llvm::errs(), *options));

    return {
      .level = level,
      .clang_options = std::move(options),
      .clang_engine = std::move(engine),
    };
  }
};

namespace cli {

// parsed cli args (in correct state)
struct options {
  // .cpp source
  fs::path source;
  std::vector<std::string> flags;

  // output dir or generated header path
  fs::path out;
  fs::path resource_dir;

  // todo: implement parsing
  bool generate_dep_file = true;
  bool annotations = true;
  log_level level = log_level::info;
  bool timings = true;
};

std::expected<options, app_error> parse(int argc,
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

template <typename Match, typename Node, typename Fold>
struct rule {
  pattern_t<Match, Node> pattern;
  Fold fold;
  clang::TraversalKind traversal_kind = clang::TraversalKind::TK_AsIs;
};

// todo: use concept `of_template<rule>`
template <typename Accum, typename... Rule>
Accum fold_matches(clang::ASTUnit &ast, //< for some reason MatchFinder needs
                                        // a mutable ASTContext&. SMH...
  Accum a,
  Rule... rule) {
  using namespace clang::ast_matchers;
  constexpr std::string_view binding_tag = "binding_tag";

  constexpr auto callback_from_rule = //
    [binding_tag]<typename Match, typename Node, typename Fold>(Accum &accum,
      const ast::rule<Match, Node, Fold> &rule) {
      const auto bound_matcher =
        traverse(rule.traversal_kind, rule.pattern.match.bind(binding_tag));

      using matcher_t = decltype(bound_matcher);

      struct _callback: MatchFinder::MatchCallback {
        matcher_t matcher;
        Accum &accum;
        Fold fold;

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

          accum = fold(std::move(accum), *node);
        }

        _callback(const matcher_t &m,
          Accum &a,
          const Fold &f,
          std::string_view binding_tag)
            : matcher(m)
            , accum(a)
            , fold(f)
            , binding_tag(binding_tag) {}
      };

      return _callback(bound_matcher, accum, rule.fold, binding_tag);
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

namespace diag {

struct source_location {
  std::filesystem::path source_file;
  std::uint32_t line;
  std::uint32_t column;

  friend bool operator<(const source_location &lhs,
    const source_location &rhs) {
    return std::tie(lhs.source_file, lhs.line, lhs.column)
      < std::tie(rhs.source_file, rhs.line, rhs.column);
  }

  friend bool operator==(const source_location &,
    const source_location &) = default;
};

struct source_excerpt {
  std::vector<std::string> lines;
  std::uint32_t pointer_line;
  std::uint32_t pointer_column;
  std::uint32_t width;
};

struct source_error {
  source_location location;
  source_excerpt source;
  std::vector<std::string> callstack;
  std::string subject;
  std::string reason;
  std::optional<std::string> suggestion;
};

} // namespace diag

namespace meta {

constexpr std::array k_supported_member_aliases = //
  std::to_array<std::string_view>({
    "error_type",
    "first_type",
    "key_type",
    "mapped_type",
    "second_type",
    "type",
    "value",
    "value_type",
  });

constexpr std::array k_compound_dependency_route_names = //
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
  if constexpr (std::derived_from<std::remove_cvref_t<Decl>, clang::TagDecl>) {
    return ast.getCanonicalTagType(d)
      ->template getAs<map_decl_to_type<std::remove_cvref_t<Decl>>>();
  } else {
    return ast.getCanonicalType(ast.getTypeDeclType(d))
      ->template getAs<map_decl_to_type<std::remove_cvref_t<Decl>>>();
  }
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

// refactorme: type_id is the reflection emission identity. For primary record
// templates it identifies the primary template declaration, not the observed
// concrete specialization. Rename to make that semantic explicit.
// todo: after a large-enough .cpp benchmark exists, replace ad hoc string
// type_id rendering with verified Clang declaration identity where applicable,
// especially clang::index::generateUSRForDecl for declaration-backed ids.
using type_id = std::string;

// Dependent route from an enclosing public root to a non-public nested type.
struct type_access_path {
  struct field {
    std::string name;
  };

  struct member_type {
    std::string name;
  };

  struct template_arg {
    std::size_t index;
  };

  std::vector<std::variant<field, member_type, template_arg>> steps;
};

struct namespace_component {
  std::string name;
  bool is_inline = false;

  auto operator<=>(const namespace_component &) const = default;
};

struct nm_qual_type {
  /// nullopt for unnamed types
  std::optional<std::string> name;

  /// chain of namespaces. may start with empty string if declared in anonymous
  /// namespace
  std::vector<namespace_component> namespaces;

  // refactorme: model these as `enclosing_record` values carrying declaration
  // identity and template-specialization spelling. Name renderers could then
  // evaluate ordinary and specialized records through one component pipeline
  // instead of recovering missing structure from source text.
  /// non-empty for nested types `struct foo { struct bar{}; };`
  std::vector<std::string> enclosing_records;

  static meta::nm_qual_type from_decl(const clang::NamedDecl *decl) noexcept;
};

struct type_definition {
  nm_qual_type type_name;
  diag::source_location location;

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
  std::string type_name;
  std::string qualified_type_name;
  std::string annotation;

  enum class value_access {
    reference,
    copy,
    misaligned_array,
  } access;

  bool is_volatile;
  bool is_deprecated;

  enum {
    none = 0,
    as_const,
    as_mutable,
  } qualified;

  static field_data from_decl(const clang::ASTContext &ast,
    const clang::FieldDecl *d);
};

struct template_type_param {
  std::string name;
  bool is_pack;
};

struct template_value_param {
  std::string type;
  std::string name;
  bool is_pack;
};

struct template_template_param;

using template_param = std::variant<template_type_param,
  template_value_param,
  template_template_param>;

struct template_template_param {
  std::vector<template_param> params;
  std::string name;
  bool is_pack;
};

struct template_data {
  enum support_t {
    supported,
    partial_specialization,
    explicit_specialization,
  } support;

  std::string primary_name;
  std::vector<template_param> params;

  static template_data from_decl(const clang::ASTContext &ast,
    const clang::ClassTemplateSpecializationDecl *spec);
};

struct reflectable_reference {
  type_id id;
  type_id concrete_id;
};

struct record_data {
  // Records with virtual bases are rejected before metadata mapping.
  std::vector<reflectable_reference> public_bases;

  /// protected and private are not supported now (too complicated)
  std::vector<field_data> public_fields;

  enum type_t {
    is_struct,
    is_class,
    is_union,
  } type;

  bool has_template_parent;
  std::optional<template_data> template_info;

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
  // Generated metadata identity. Primary template instantiations intentionally
  // share this id.
  type_id id;

  // Concrete observed type identity used for dependency graph traversal and
  // support classification before generated metadata is deduplicated.
  type_id concrete_id;

  // Public member path that exposes an otherwise non-public nested type.
  type_access_path public_access_path;

  std::string annotation;

  // refactorme: Consider having template arg `reflectable_data` instead
  std::variant<record_data, enum_data> data;

  // refactorme: consider removing type_definition from reflectable and keeping
  // type names in source_file_context::type_name_by_id instead. Bulk
  // acquisition, unique-key storage, and render access have different
  // ownership/access needs.
  type_definition definition;
};

struct already_reflected {
  type_id id;
  type_id concrete_id;
};

// todo: store the type info, since only a limited set of non_relfectable types
// are supported, i.e.: std::tuple<A, B, C>, std::variant<A, B, C>,
// std::vector<A>, ...
struct non_reflectable {
  type_id id;
  type_id concrete_id;
};

// refactorme: better name, now it differs in only one letter with the function
struct reflected_type_info {
  std::variant<reflectable, non_reflectable, already_reflected> type;
  std::vector<reflectable> dependencies;
  std::map<type_id, std::set<type_id>> dependencies_by_type;
  std::map<type_id, type_access_path> public_access_path_by_type;
  std::set<type_id> skipped_virtual_base_dependencies{};
};

struct indexed_arg_candidate {
  type_id type;
  diag::source_error error;
};

std::expected<reflected_type_info, std::string> resolve_reflected_type(
  const diagnostics &log,
  clang::QualType type,
  const clang::ASTContext &ast,
  const std::set<type_id> &resolved_concrete_types);

struct source_file_context {
  fs::path source;

  std::vector<meta::reflectable> reflected{};

  // Fold state used while resolving reflected calls. Tracks ids already
  // seen during direct and dependency resolution.
  std::set<meta::type_id> resolved_types{};
  std::set<meta::type_id> resolved_concrete_types{};
  std::set<meta::type_id> direct_concrete_types{};
  std::set<meta::type_id> resolved_as_dependency{};
  std::set<meta::type_id> non_reflectable_types{}; //< for debug or info

  std::map<meta::type_id, std::set<meta::type_id>> dependencies_by_type{};
  std::set<meta::type_id> skipped_virtual_base_dependencies{};

  // Render lookup for bases and other references by type id. Stored by value to
  // avoid coupling render lifetime to reflected collection layout.
  std::map<meta::type_id, meta::nm_qual_type> type_name_by_id{};

  std::map<meta::type_id, std::size_t> index_by_type_id{};
  std::vector<meta::indexed_arg_candidate> indexed_arg_candidates{};

  struct {
    std::vector<diag::source_error> reflection_queries;
    std::vector<diag::source_error> reflected_call_args;
    std::vector<std::string> internal;
  } errors{};

  // Used to generate deps file for cmake, so it can rerun the tool upon changes
  // to those headers
  std::set<fs::path> file_dependencies;
};

namespace matches {

source_file_context fold_out_of_scope_type_query(clang::ASTContext &ast,
  source_file_context a,
  const clang::TypeLoc &type_loc);

source_file_context fold_out_of_scope_function_query(clang::ASTContext &ast,
  source_file_context a,
  const clang::DeclRefExpr &query);

source_file_context fold_reflected_call(const diagnostics &log,
  clang::Sema &sema,
  clang::ASTContext &ast,
  source_file_context a,
  const clang::CXXOperatorCallExpr &call);

source_file_context fold_index_registration(const clang::ASTContext &ast,
  source_file_context a,
  const clang::ClassTemplateSpecializationDecl &decl);

source_file_context finalize(const diagnostics &log, source_file_context ctx);

} // namespace matches
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

  // (experimental) can only be reflected using a generated index
  struct indexed_type {
    meta::reflectable type;
    std::size_t index;
  };

  fs::path instrumented_source;

  std::vector<forward_declarable> fwd_declarables;
  std::vector<nested_type> nested;
  std::vector<indexed_type> indexed;
  std::vector<meta::nm_qual_type> enclosing_roots;

  std::map<meta::type_id, meta::nm_qual_type> type_name_by_id;

  std::set<fs::path> file_dependencies;
};

std::expected<reflection_context, std::string>
  generate_reflection(reflection_context ctx, std::ofstream file);

} // namespace render

namespace pipeline {

struct compiler_invocation {
  fs::path source;
  std::shared_ptr<clang::CompilerInvocation> ci;
};

std::expected<compiler_invocation, app_error> to_compiler_invocation(
  const diagnostics &log,
  const cli::options &cli_args) noexcept;

struct parsed_ast {
  fs::path source;
  std::unique_ptr<clang::ASTUnit> ast;

  // will be populated during ASTUnit creation
  std::set<fs::path> includes_deps;
};

std::expected<parsed_ast, app_error> to_parsed_ast(const diagnostics &log,
  compiler_invocation ci);

std::expected<meta::source_file_context, app_error>
  fold_matches(const diagnostics &log, parsed_ast ast);

std::expected<render::reflection_context, app_error> resolve_reflection_context(
  const diagnostics &log,
  meta::source_file_context ctx);

struct run_report {
  fs::path instrumented_source;
  fs::path reflection_header;
  fs::path dependencies;
  std::size_t reflected_type_count;
  std::size_t forward_declarable_count;
  std::size_t nested_type_count;
  std::size_t indexed_type_count;
  std::size_t file_dependency_count;

  // todo: add useful report fields: timings and generated bytes.
};

std::expected<run_report, app_error> emit_outputs(const diagnostics &log,
  fs::path out,
  render::reflection_context ctx);

} // namespace pipeline

std::string format_options(const cli::options &args);
std::string format_matches(const meta::source_file_context &ctx);
std::string format_success(const pipeline::run_report &out);
std::string format_timings(
  const std::map<std::string, std::chrono::microseconds> &timing_by_action);

struct action_timer {
  const std::map<std::string, std::chrono::microseconds> &
    timing_by_action() const {
    return _timing_by_action;
  }

  auto operator()(std::string action, auto f) {
    return //
      [this, action = std::move(action), f = std::move(f)](auto &&...args) {
        const std::chrono::time_point start = std::chrono::steady_clock::now();
        auto result = std::invoke(f, std::forward<decltype(args)>(args)...);

        _timing_by_action.emplace(action,
          std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start));

        return result;
      };
  }

  private:
  std::map<std::string, std::chrono::microseconds> _timing_by_action;
};

} // namespace

template <>
struct std::formatter<meta::record_data::type_t, char>:
    std::formatter<std::string_view, char> {
  template <typename FormatContext>
  auto format(meta::record_data::type_t t, FormatContext &ctx) const {
    switch (t) {
    case meta::record_data::is_struct:
      return std::formatter<std::string_view, char>::format("struct"sv, ctx);
    case meta::record_data::is_class:
      return std::formatter<std::string_view, char>::format("class"sv, ctx);
    case meta::record_data::is_union:
      return std::formatter<std::string_view, char>::format("union"sv, ctx);
    }

    assert(false && "unreachable");
    return std::formatter<std::string_view, char>::format("struct"sv, ctx);
  }
};

template <>
struct std::formatter<meta::template_data::support_t, char>:
    std::formatter<std::string_view, char> {
  template <typename FormatContext>
  auto format(meta::template_data::support_t support,
    FormatContext &ctx) const {
    switch (support) {
    case meta::template_data::supported:
      return std::formatter<std::string_view, char>::format("supported"sv, ctx);
    case meta::template_data::partial_specialization:
      return std::formatter<std::string_view, char>::format(
        "partial_specialization"sv,
        ctx);
    case meta::template_data::explicit_specialization:
      return std::formatter<std::string_view, char>::format(
        "explicit_specialization"sv,
        ctx);
    }

    assert(false && "unreachable");
    return std::formatter<std::string_view, char>::format("supported"sv, ctx);
  }
};

template <>
struct std::formatter<log_level, char>: std::formatter<std::string_view, char> {
  template <typename FormatContext>
  auto format(log_level level, FormatContext &ctx) const {
    static const std::map<log_level, std::string_view> string_by_enum =
      log_levels
      | std::ranges::to<std::map<log_level, std::string_view>>();

    assert(string_by_enum.contains(level) && "missing log_level mapping");
    return std::formatter<std::string_view, char>::format(
      string_by_enum.at(level),
      ctx);
  }
};

int main(int argc, char **argv) {
  std::expected args = cli::parse(argc, argv);

  if (!args) {
    if (!args.error().message.empty())
      std::cerr << args.error().message << '\n';
    return args.error().code;
  }

  const diagnostics log = diagnostics::from(args->level);

  log(log_level::debug, [&args = *args] { return format_options(args); });
  log(log_level::info, [&source = args->source] {
    return std::format("\nrunning for file: {}\t\r", source.string());
  });

  action_timer measured;

  const auto to_compiler_invocation = std::bind_front(
    measured("compiler invocation", pipeline::to_compiler_invocation),
    std::cref(log));

  const auto to_parsed_ast =
    std::bind_front(measured("parsed ast", pipeline::to_parsed_ast),
      std::cref(log));

  const auto fold_matches =
    std::bind_front(measured("fold matches", pipeline::fold_matches),
      std::cref(log));

  const auto resolve_reflection_context =
    std::bind_front(measured("resolve reflection context",
                      pipeline::resolve_reflection_context),
      std::cref(log));

  const auto emit_outputs =
    std::bind_front(measured("emit outputs", pipeline::emit_outputs),
      std::cref(log),
      args->out);

  const auto with_print_matches =
    fn::with([&log](const meta::source_file_context &ctx) {
      log(log_level::debug, [&ctx] { return format_matches(ctx); });
    });

  const auto report_success = //
    [&log, timings = args->timings, &measured](
      const pipeline::run_report &report) {
      log(log_level::info, [&report] { return format_success(report); });
      if (timings)
        llvm::errs() << format_timings(measured.timing_by_action());
      return 0;
    };

  const auto report_error = //
    [&log, timings = args->timings, &measured](
      app_error error) -> std::expected<int, app_error> {
    log(log_level::error, error.message);
    if (timings)
      llvm::errs() << format_timings(measured.timing_by_action());
    return error.code;
  };

  return to_compiler_invocation(*args)
    .and_then(to_parsed_ast)
    .and_then(fold_matches)
    .transform(with_print_matches)
    .and_then(resolve_reflection_context)
    .and_then(emit_outputs)
    .transform(report_success)
    .or_else(report_error)
    .value();
}

namespace {

namespace diag {

std::string format_location(const source_location &location) {
  return std::format("{}:{}:{}",
    location.source_file.generic_string(),
    location.line,
    location.column);
}

std::string format_error(const source_error &error) {
  std::string message = std::format("  {}: {}:\n    {}",
    format_location(error.location),
    error.subject,
    error.reason);

  for (const auto &[index, line] :
    error.source.lines | std_c::views::enumerate) {
    message += std::format("\n    {}", line);

    if (error.source.pointer_line != index)
      continue;

    const std::size_t leading =
      0 == error.source.pointer_column ? 0 : error.source.pointer_column - 1;

    message += std::format("\n    {}",
      std::views::iota(std::size_t{}, leading + error.source.width)
        | std::views::transform([&error, leading](std::size_t i) {
            if (i < leading) {
              return '\t' == error.source.lines[error.source.pointer_line][i] //
                ? '\t'
                : ' ';
            }

            return i == leading ? '^' : '~';
          })
        | std::ranges::to<std::string>());
  }

  if (!error.callstack.empty()) {
    message += "\n    call stack:";

    for (const std::string &frame : error.callstack)
      message += std::format("\n      {}", frame);
  }

  message += //
    error.suggestion
      .and_then(
        [](const std::string &suggestion) -> std::optional<std::string> {
          return std::format("\n    Suggestion: {}", suggestion);
        })
      .value_or("");

  return message;
}

} // namespace diag

namespace meta {

bool has_template_record_parent(const clang::DeclContext *dc) {
  for (const clang::DeclContext *cur = dc; cur; cur = cur->getParent()) {
    const auto *rd = llvm::dyn_cast<clang::CXXRecordDecl>(cur);

    if (rd
      && (llvm::isa<clang::ClassTemplateSpecializationDecl>(rd)
        || llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(rd))) {
      return true;
    }
  }

  return false;
}

bool has_virtual_bases(const clang::CXXRecordDecl &record) {
  const clang::CXXRecordDecl *const definition = record.getDefinition();
  // `vbases()` contains both direct and inherited virtual bases.
  return definition && !definition->vbases().empty();
}

std::string annotation_from_decl(const clang::ASTContext &ast,
  const clang::Decl *decl) {
  assert(decl);

  // Raw-comment lookup may be relatively expensive; skip it when annotations
  // were explicitly disabled.
  if (!ast.getLangOpts().CommentOpts.ParseAllComments)
    return "";

  const clang::Decl *canonical = decl->getCanonicalDecl();
  const clang::RawComment *comment = ast.getRawCommentForDeclNoCache(canonical);
  if (!comment)
    comment = ast.getRawCommentForDeclNoCache(decl);
  if (!comment)
    return "";

  return comment->getBriefText(ast);
}

type_id canonical_type_id(const clang::ASTContext &ast,
  const clang::Type *type) {
  clang::PrintingPolicy p(ast.getLangOpts());
  p.FullyQualifiedName = true;
  p.SuppressScope = false;
  p.SuppressUnwrittenScope = false;
  p.SuppressTagKeyword = false;
  p.PrintAsCanonical = true;

  // "no qualifiers" (not const, not volatile, not restrict).
  const unsigned qual_flags = 0;
  return clang::QualType(type, qual_flags).getAsString(p);
}

type_id reflectable_type_id(const clang::ASTContext &ast,
  const clang::TagType *type) {
  assert(type);

  const auto *template_spec =
    llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(type->getDecl());

  if (!template_spec)
    return canonical_type_id(ast, type);

  // ad hoc: type_id is still a rendered string, not a Clang-provided opaque
  // identity. Prefix primary-template declaration ids so they cannot collide
  // with rendered concrete type ids if full specializations are supported
  // later.
  // todo: replace this primary-template collapse with general template
  // specialization reflection metadata. `crtp_base<X>` and `crtp_base<Y>` are
  // treated as the same reflected template shape for now, which is only valid
  // while full and partial specializations are unsupported/rejected.
  return std::format("primary-template:{}",
    template_spec->getSpecializedTemplate()
      ->getCanonicalDecl()
      ->getQualifiedNameAsString());
}

bool is_compound_dependency_route(const clang::CXXRecordDecl *record) {
  assert(record);

  return std::ranges::contains(
           std::array{
             clang::Decl::ClassTemplateSpecialization,
             clang::Decl::ClassTemplatePartialSpecialization,
           },
           record->getKind())
    && std::ranges::contains(k_compound_dependency_route_names,
      fn::as<std::string_view>(
        clang::cast<clang::ClassTemplateSpecializationDecl>(record)
          ->getSpecializedTemplate()
          ->getName()));
}

std::string format_type_name(const nm_qual_type &type) {
  const std::string type_name = type.name.value_or("(unnamed)");
  return std::format("{}",
    std::array{
      std::span{type.enclosing_records},
      std::span{&type_name, std::size_t{1}},
    } //
      | std::views::join //
      | std_c::views::join_with("::"sv) //
      | util::format_range);
}

std::string format(const nm_qual_type &type) {
  const std::vector namespace_names = type.namespaces //
    | std::views::transform(&namespace_component::name)
    | std::ranges::to<std::vector>();

  const std::string type_name = format_type_name(type);

  return std::format("{}",
    std::array{
      std::span{namespace_names},
      std::span{&type_name, std::size_t{1}},
    } //
      | std::views::join //
      | std_c::views::join_with("::"sv) //
      | util::format_range);
}

} // namespace meta

namespace meta::matches::impl {

// Clang can emit caret diagnostics directly, but omnirefl collects matcher
// failures before reporting them together. Keep source and caret geometry
// structured instead of emitting through DiagnosticsEngine here.
std::expected<diag::source_excerpt, std::string> source_excerpt_from(
  const clang::ASTContext &ast,
  clang::SourceRange context_range,
  clang::SourceRange subject_range) {
  if (!context_range.isValid() || !subject_range.isValid())
    return std::unexpected("invalid context or subject source range");

  const clang::SourceManager &sm = ast.getSourceManager();
  const auto end_of_token = [&ast, &sm](clang::SourceLocation loc) {
    return sm.getSpellingLoc(
      clang::Lexer::getLocForEndOfToken(loc, 0, sm, ast.getLangOpts()));
  };

  const clang::SourceLocation context_begin =
    sm.getSpellingLoc(context_range.getBegin());

  const clang::SourceLocation context_end =
    end_of_token(context_range.getEnd());

  const clang::SourceLocation subject_begin =
    sm.getSpellingLoc(subject_range.getBegin());

  const clang::SourceLocation subject_end =
    end_of_token(subject_range.getEnd());

  if (!context_begin.isValid() || !context_end.isValid()
    || !subject_begin.isValid() || !subject_end.isValid()) {
    return std::unexpected(
      "invalid spelling location for context or subject range");
  }

  const std::size_t context_begin_offset = sm.getFileOffset(context_begin);
  const std::size_t context_end_offset = sm.getFileOffset(context_end);
  const std::size_t subject_begin_offset = sm.getFileOffset(subject_begin);
  const std::size_t subject_end_offset = sm.getFileOffset(subject_end);

  const std::expected validated_text = std::invoke( //
    [&sm,
      context_begin,
      context_end,
      subject_begin,
      subject_end,
      context_begin_offset,
      context_end_offset,
      subject_begin_offset,
      subject_end_offset] -> std::expected<std::string_view, std::string> {
      const clang::FileID context_file = sm.getFileID(context_begin);

      if (context_file != sm.getFileID(context_end)
        || context_file != sm.getFileID(subject_begin)
        || context_file != sm.getFileID(subject_end)) {
        return std::unexpected(
          "diagnostic context and subject range span multiple files");
      }

      bool invalid = false;
      const llvm::StringRef buffer = sm.getBufferData(context_file, &invalid);

      if (invalid || buffer.size() < context_begin_offset
        || buffer.size() < context_end_offset
        || buffer.size() < subject_begin_offset
        || buffer.size() < subject_end_offset
        || context_end_offset < context_begin_offset
        || subject_end_offset < subject_begin_offset) {
        return std::unexpected(
          "invalid source buffer offsets for diagnostic range");
      }

      return std::string_view{buffer.data(), buffer.size()};
    });

  if (!validated_text)
    return std::unexpected(validated_text.error());

  const auto line_begin_from = [&validated_text](std::size_t offset) {
    const std::size_t nl = validated_text->rfind('\n', offset);
    return std::string_view::npos == nl ? std::size_t{0} : nl + 1;
  };

  const auto line_end_from = [&validated_text](std::size_t offset) {
    const std::size_t nl = validated_text->find('\n', offset);
    return std::string_view::npos == nl ? validated_text->size() : nl;
  };

  const std::size_t first_line_begin = line_begin_from(context_begin_offset);
  const std::size_t last_line_end = line_end_from(context_end_offset);
  const std::size_t pointer_line_begin = line_begin_from(subject_begin_offset);
  const std::size_t pointer_line_end = line_end_from(subject_begin_offset);

  if (last_line_end < first_line_begin || pointer_line_begin < first_line_begin
    || last_line_end < pointer_line_end
    || pointer_line_end < subject_begin_offset) {
    return std::unexpected(
      "diagnostic subject range does not fit in rendered source excerpt");
  }

  return diag::source_excerpt{
    .lines = std::invoke( //
      [&validated_text, &line_end_from, first_line_begin, last_line_end] {
        std::vector<std::string> lines;
        std::size_t line_start = first_line_begin;

        while (line_start <= last_line_end) {
          const std::size_t line_end = line_end_from(line_start);
          std::string line{
            validated_text->substr(line_start, line_end - line_start)};

          if (!line.empty() && '\r' == line.back())
            line.pop_back();

          lines.push_back(std::move(line));

          if (last_line_end <= line_end)
            break;

          line_start = line_end + 1;
        }

        return lines;
      }),

    .pointer_line = static_cast<std::uint32_t>( //
      std::ranges::count( //
        validated_text->substr( //
          first_line_begin,
          pointer_line_begin - first_line_begin),
        '\n')),

    .pointer_column = sm.getSpellingColumnNumber(subject_begin),
    .width = static_cast<std::uint32_t>(std::min<std::size_t>(
      std::max<std::size_t>(1, subject_end_offset - subject_begin_offset),
      std::max<std::size_t>(1, pointer_line_end - subject_begin_offset))),
  };
}

std::expected<diag::source_location, std::string> source_location_from(
  const clang::ASTContext &ast,
  clang::SourceLocation loc) {
  if (!loc.isValid())
    return std::unexpected("invalid source location");

  const clang::SourceManager &sm = ast.getSourceManager();
  const clang::PresumedLoc presumed = sm.getPresumedLoc(loc);

  if (!presumed.isValid())
    return std::unexpected("invalid presumed source location");

  return diag::source_location{
    .source_file = presumed.getFilename(),
    .line = sm.getSpellingLineNumber(loc),
    .column = sm.getSpellingColumnNumber(loc),
  };
}

bool is_omni_frontend_location(const diag::source_location &loc) {
  return std::string::npos
    != loc.source_file.generic_string().find("/include/omnirefl/");
}

bool is_concrete_template_arg(const clang::TemplateArgument &arg) {
  switch (arg.getKind()) {
  case clang::TemplateArgument::Type: {
    const clang::QualType type = arg.getAsType();
    return !type->isDependentType() && !type->isInstantiationDependentType()
      && !type->containsUnexpandedParameterPack();
  }
  case clang::TemplateArgument::Integral:
  case clang::TemplateArgument::Declaration:
  case clang::TemplateArgument::NullPtr:
  case clang::TemplateArgument::Template:
  case clang::TemplateArgument::TemplateExpansion:
  case clang::TemplateArgument::StructuralValue:
    return true;
  case clang::TemplateArgument::Expression: {
    const clang::Expr *expr = arg.getAsExpr();
    return expr && !expr->isTypeDependent() && !expr->isInstantiationDependent()
      && !expr->containsUnexpandedParameterPack();
  }
  case clang::TemplateArgument::Pack:
  case clang::TemplateArgument::Null:
    return false;
  }

  return false;
}

clang::QualType unreferenced_type(clang::QualType type) {
  while (type->isReferenceType())
    type = type->getPointeeType();

  return type;
}

std::optional<clang::QualType> type_t_arg_type(clang::QualType type) {
  return fn::maybe(unreferenced_type(type)->getAs<clang::RecordType>())
    .and_then([](const clang::RecordType *record) {
      return fn::maybe(llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
        record->getDecl()));
    })
    .and_then([](const clang::ClassTemplateSpecializationDecl *spec)
                -> std::optional<clang::QualType> {
      if ("omni::type_t"
        != spec->getSpecializedTemplate()->getQualifiedNameAsString()) {
        return std::nullopt;
      }

      const clang::TemplateArgumentList &args = spec->getTemplateArgs();
      if (1 != args.size()
        || clang::TemplateArgument::Type != args.get(0).getKind()) {
        return std::nullopt;
      }

      return args.get(0).getAsType();
    });
}

std::expected<clang::QualType, std::string> instantiate_type_t_arg(
  clang::Sema &sema,
  clang::QualType type,
  clang::SourceLocation point_of_instantiation) {
  const clang::Type &canonical =
    *sema.Context.getCanonicalType(type.getUnqualifiedType()).getTypePtr();

  auto *const specialization = //
    fn::maybe(llvm::dyn_cast<clang::RecordType>(&canonical))
      .transform([](const clang::RecordType *record) {
        return llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
          record->getDecl());
      })
      .value_or(nullptr);

  if (!specialization //
    || specialization->getDefinition() //
    || specialization->isInStdNamespace() //
    || clang::TSK_ExplicitSpecialization
      == specialization->getSpecializationKind()) {
    return type;
  }

  // ad hoc: omni::type_t<T> does not otherwise require T to be complete, but
  // metadata collection needs the definition. Instantiate only its visible
  // primary specialization; the visitor remains deferred.
  if (!specialization->getSpecializedTemplate()
        ->getTemplatedDecl()
        ->getDefinition()) {
    return type;
  }

  if (sema.InstantiateClassTemplateSpecialization(point_of_instantiation,
        specialization,
        clang::TSK_ImplicitInstantiation,
        /*Complain=*/true,
        specialization->hasStrictPackMatch())) {
    return std::unexpected(
      "the concrete primary-template argument could not be instantiated");
  }

  return type;
}

std::vector<std::string> callstack_from(clang::ASTContext &ast,
  clang::DynTypedNode node) {
  std::vector<std::string> frames;
  std::set<std::string> emitted;
  const auto frame_from = //
    [&ast](std::string_view prefix,
      std::string_view name,
      clang::SourceLocation loc) -> std::optional<std::string> {
    std::expected location = source_location_from(ast, loc);

    if (!location)
      return std::nullopt;

    return std::format("{} '{}' at {}",
      prefix,
      name,
      diag::format_location(*location));
  };

  for (auto parents = ast.getParents(node); //
    !parents.empty(); //
    node = parents[0], parents = ast.getParents(node)) {
    if (const auto *decl = node.get<clang::FunctionDecl>()) {
      const std::optional frame = frame_from("in function",
        decl->getQualifiedNameAsString(),
        decl->getLocation());

      if (frame && emitted.emplace(*frame).second)
        frames.push_back(*frame);

      const clang::SourceLocation instantiation =
        decl->getPointOfInstantiation();

      if (instantiation.isValid()) {
        const std::optional frame = frame_from("instantiated from",
          decl->getQualifiedNameAsString(),
          instantiation);

        if (frame && emitted.emplace(*frame).second)
          frames.push_back(*frame);
      }

      continue;
    }

    if (const auto *lambda = node.get<clang::LambdaExpr>()) {
      const std::optional frame =
        frame_from("in lambda", "<lambda>"sv, lambda->getBeginLoc());

      if (frame && emitted.emplace(*frame).second)
        frames.push_back(*frame);
    }
  }

  return frames;
}

bool definition_follows_reflected_call(clang::ASTContext &ast,
  const clang::CXXOperatorCallExpr &call,
  const clang::TagDecl &definition) {
  const clang::SourceManager &sm = ast.getSourceManager();
  const clang::SourceLocation definition_location =
    sm.getExpansionLoc(definition.getLocation());

  if (definition_location.isInvalid())
    return false;

  const auto is_before_definition = //
    [&sm, definition_location](clang::SourceLocation use) -> bool {
      return use.isValid()
        && sm.isBeforeInTranslationUnit(sm.getExpansionLoc(use),
          definition_location);
    };

  if (!is_before_definition(call.getExprLoc()))
    return false;

  // A lexically later definition is valid when an enclosing template is
  // instantiated only after that definition becomes visible.
  clang::DynTypedNode node = clang::DynTypedNode::create(call);
  for (auto parents = ast.getParents(node); //
    !parents.empty(); //
    node = parents[0], parents = ast.getParents(node)) {
    const auto *decl = node.get<clang::FunctionDecl>();

    if (decl && decl->getPointOfInstantiation().isValid()
      && !is_before_definition(decl->getPointOfInstantiation())) {
      return false;
    }
  }

  return true;
}

meta::source_file_context append_invalid_reflection_query(
  meta::source_file_context a,
  clang::ASTContext &ast,
  std::string_view query,
  clang::SourceRange context_range,
  clang::SourceRange query_range,
  clang::DynTypedNode query_node,
  std::string reason) {
  std::expected location = source_location_from(ast, query_range.getBegin());
  std::expected source = source_excerpt_from(ast, context_range, query_range);

  if (!location || !source) {
    a.errors.internal.emplace_back(std::format(
      "reflection query matcher could not render source context: {}",
      !location ? location.error() : source.error()));

    return a;
  }

  if (is_omni_frontend_location(*location))
    return a;

  std::string subject = std::format("reflection query '{}' is invalid", query);

  // ad hoc: reflection-query diagnostics have no structured identity yet, so
  // deduplicate the same source location by its rendered subject.
  if (std::ranges::any_of(a.errors.reflection_queries,
        [&location, &subject](const diag::source_error &error) {
          return *location == error.location && subject == error.subject;
        })) {
    return a;
  }

  a.errors.reflection_queries.push_back({
    .location = *std::move(location),
    .source = *std::move(source),
    .callstack = callstack_from(ast, std::move(query_node)),
    .subject = std::move(subject),
    .reason = std::move(reason),
    .suggestion =
      "move the query into a deferred reflected_call visitor; the visitor "
      "must be a generic lambda or have a templated operator(), and generic "
      "lambdas must use an explicit trailing return type",
  });

  return a;
}

using reflected_call_arg_error = std::variant<diag::source_error, std::string>;

struct reflected_call_arg_error_input {
  clang::ASTContext &ast;
  const clang::CXXOperatorCallExpr &call;
  const clang::Expr &arg;
  std::string rendered_type;
  std::string reason;
  std::optional<std::string> suggestion = std::nullopt;
};

reflected_call_arg_error reflected_call_arg_error_from(
  reflected_call_arg_error_input input) {
  std::expected location =
    source_location_from(input.ast, input.arg.getExprLoc());

  if (!location) {
    return std::format(
      "reflected_call matcher could not resolve an argument source location: {}",
      std::move(location).error());
  }

  std::expected source = source_excerpt_from(input.ast,
    input.call.getSourceRange(),
    input.arg.getSourceRange());

  if (!source) {
    return std::format(
      "reflected_call matcher could not render an argument source excerpt: {}",
      std::move(source).error());
  }

  return diag::source_error{
    .location = *location,
    .source = std::move(*source),
    .callstack =
      callstack_from(input.ast, clang::DynTypedNode::create(input.call)),
    .subject = std::format("reflected_call argument '{}' is unsupported",
      input.rendered_type),
    .reason = std::move(input.reason),
    .suggestion = std::move(input.suggestion),
  };
}

std::string reflected_call_arg_name(const clang::ASTContext &ast,
  clang::QualType type) {
  clang::PrintingPolicy p(ast.getLangOpts());
  p.FullyQualifiedName = true;
  p.SuppressScope = false;
  p.SuppressUnwrittenScope = false;
  p.SuppressTagKeyword = false;

  return type.getUnqualifiedType().getAsString(p);
}

struct reflected_call_arg {
  clang::QualType type;
  const clang::Expr *reflected_call_expr;
};

std::expected<reflected_call_arg, reflected_call_arg_error>
  validate_reflected_call_arg(clang::ASTContext &ast,
    const clang::CXXOperatorCallExpr &call,
    const clang::Expr *arg_expr,
    clang::QualType type) {
  assert(arg_expr);

  const clang::QualType unqualified = type.getUnqualifiedType();
  const clang::Type &canonical =
    *ast.getCanonicalType(unqualified).getTypePtr();

  const std::string rendered = reflected_call_arg_name(ast, unqualified);

  const auto invalid_arg_error = //
    [&ast, &call, &arg = *arg_expr, &rendered](std::string reason,
      std::string suggestion) -> reflected_call_arg_error {
    return reflected_call_arg_error_from({
      .ast = ast,
      .call = call,
      .arg = arg,
      .rendered_type = rendered,
      .reason = std::move(reason),
      .suggestion = std::move(suggestion),
    });
  };

  if (canonical.isPointerType()) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "reflected_call argument is a pointer; "
      "pointer types are not reflected directly",
      /*suggestion*/ //
      "dereference the pointer and pass the referenced record/enum, "
      "or wrap pointer data in a reflectable record"));
  }

  if (canonical.isArrayType()) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "reflected_call argument is a raw array; "
      "raw arrays are not reflected directly",
      /*suggestion*/ //
      "wrap the array in a reflectable record, "
      "or prefer std::array/std::span where appropriate"));
  }

  if (!clang::isa<clang::TagType>(canonical)) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "reflected_call argument is not a C++ record or enum type",
      /*suggestion*/ //
      "pass a directly reflectable record/enum, "
      "or wrap scalar/fundamental values in a reflectable record"));
  }

  const auto &tag = clang::cast<clang::TagType>(canonical);

  const clang::TagDecl *const definition = tag.getDecl()->getDefinition();

  if (!definition) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "forward declarations without definitions are not allowed "
      "as reflected_call inputs",
      /*suggestion*/ //
      "include the definition before the reflected_call "
      "or pass a different reflectable type"));
  }

  if (definition_follows_reflected_call(ast, call, *definition)) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "the reflected_call argument is incomplete at this call; "
      "its definition appears later in the translation unit",
      /*suggestion*/ //
      "move the type definition before this reflected_call"));
  }

  if (tag.getDecl()->isInStdNamespace()) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "reflected_call argument is a standard-library type; "
      "std:: types are not reflected directly",
      /*suggestion*/ //
      "pass a user record/enum directly; std:: wrappers may still be used "
      "as reflected fields or supported dependency routes"));
  }

  if (llvm::isa<clang::EnumType>(&tag)) {
    return reflected_call_arg{
      .type = type,
      .reflected_call_expr = arg_expr,
    };
  }

  const auto *record_type = llvm::dyn_cast<clang::RecordType>(&tag);
  const clang::CXXRecordDecl *record_decl = record_type
    ? llvm::dyn_cast<clang::CXXRecordDecl>(definition)
    : nullptr;

  if (!record_decl)
    record_decl = record_type
      ? llvm::dyn_cast<clang::CXXRecordDecl>(record_type->getDecl())
      : nullptr;

  if (!record_decl) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "reflected_call argument is not a C++ record or enum type",
      /*suggestion*/ //
      "pass a directly reflectable C++ record/enum type"));
  }

  if (has_virtual_bases(*record_decl)) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "reflected_call argument has a virtual base; "
      "records with virtual bases are not supported",
      /*suggestion*/ //
      "pass a record without virtual inheritance to reflected_call"));
  }

  if (llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(record_decl)) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "reflected_call argument is a partial template specialization; "
      "partial specializations are not supported",
      /*suggestion*/ //
      "sanitize the type before reflection or use a supported "
      "primary record template instantiation"));
  }

  if (const auto *spec =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record_decl)) {
    const auto specialized = spec->getSpecializedTemplateOrPartial();

    if (specialized
          .dyn_cast<clang::ClassTemplatePartialSpecializationDecl *>()) {
      return std::unexpected(invalid_arg_error(
        /*reason*/ //
        "reflected_call argument is a partial template specialization; "
        "partial specializations are not supported",
        /*suggestion*/ //
        "sanitize the type before reflection or use a supported "
        "primary record template instantiation"));
    }

    if (clang::TSK_ExplicitSpecialization == spec->getSpecializationKind()) {
      return std::unexpected(invalid_arg_error(
        /*reason*/ //
        "reflected_call argument is an explicit record template "
        "specialization; explicit specializations are not supported",
        /*suggestion*/ //
        "pass an instantiation of the primary record template or a "
        "non-template reflectable wrapper"));
    }

    // todo: consider routing constrained primary-template arguments through
    // indexed mode. That requires avoiding the normal primary-template
    // declaration/emission path and is not a local validation change.
    if (spec->getSpecializedTemplate()->hasAssociatedConstraints()) {
      return std::unexpected(invalid_arg_error(
        /*reason*/ //
        "constrained templates are not supported",
        /*suggestion*/ //
        "pass an unconstrained record type to reflected_call"));
    }
  }

  if (has_template_record_parent(record_decl->getDeclContext())) {
    return std::unexpected(invalid_arg_error(
      /*reason*/ //
      "records nested inside template records are not supported as "
      "reflected_call inputs",
      /*suggestion*/ //
      "move the nested record to namespace scope or wrap the instantiated "
      "type in a supported record"));
  }

  return reflected_call_arg{
    .type = type,
    .reflected_call_expr = arg_expr,
  };
}

std::expected<reflected_call_arg, reflected_call_arg_error>
  validate_reflected_call_expression(clang::Sema &sema,
    clang::ASTContext &ast,
    const clang::CXXOperatorCallExpr &call,
    const clang::Expr *arg_expr) {
  assert(arg_expr);

  const auto validate_arg = std::bind_front(validate_reflected_call_arg,
    std::ref(ast),
    std::cref(call),
    arg_expr);

  const std::optional type_arg = type_t_arg_type(arg_expr->getType());

  if (!type_arg)
    return validate_arg(unreferenced_type(arg_expr->getType()));

  return instantiate_type_t_arg(sema, *type_arg, arg_expr->getExprLoc())
    .transform_error( //
      [&ast, &call, arg_expr, type = *type_arg](std::string reason) //
      -> reflected_call_arg_error {
        return reflected_call_arg_error_from({
          .ast = ast,
          .call = call,
          .arg = *arg_expr,
          .rendered_type = reflected_call_arg_name(ast, type),
          .reason = std::move(reason),
        });
      })
    .and_then(validate_arg);
}

bool is_forward_declarable(const meta::reflectable &type) {
  const nm_qual_type &name = type.definition.type_name;
  const auto flags = type.definition.definition_flags;

  if (!name.name //
    || !name.enclosing_records.empty() //
    || type_definition::local & flags //
    || type_definition::non_public & flags) {
    return false;
  }

  return std::visit(
    []<reflectable_data Data>(const Data &data) {
      if constexpr (std::same_as<record_data, Data>) {
        return true;
      } else {
        static_assert(std::same_as<enum_data, Data>);
        return data.is_scoped || data.is_fixed;
      }
    },
    type.data);
}

bool is_nested_declarable(const meta::reflectable &type) {
  const nm_qual_type &name = type.definition.type_name;
  const auto flags = type.definition.definition_flags;

  if (!name.name //
    || name.enclosing_records.empty() //
    || name.enclosing_records.front().empty() //
    || type_definition::local & flags) {
    return false;
  }

  return !(type_definition::non_public & flags)
    || !type.public_access_path.steps.empty();
}

} // namespace meta::matches::impl

meta::source_file_context meta::matches::fold_out_of_scope_type_query(
  clang::ASTContext &ast,
  meta::source_file_context a,
  const clang::TypeLoc &type_loc) {
  // ad hoc: Clang gives a substituted visitor constraint the semantic
  // binding_t/meta_t type. Ignore it because the source names a template
  // parameter rather than spelling a reflection query.
  if (type_loc.getAs<clang::SubstTemplateTypeParmTypeLoc>())
    return a;

  // Generic visitor placeholders are substituted with binding_t during
  // overload resolution; the spelled `auto` is not a reflection query.
  if (std::invoke([&ast, &type_loc] {
        clang::Token token;
        return !clang::Lexer::getRawToken(type_loc.getBeginLoc(),
                 token,
                 ast.getSourceManager(),
                 ast.getLangOpts())
          && (token.is(clang::tok::kw_auto)
            // Raw lexing may preserve a keyword as a raw identifier.
            || (token.is(clang::tok::raw_identifier)
              && "auto" == token.getRawIdentifier()));
      })) {
    return a;
  }

  const auto *const decl = //
    fn::maybe(type_loc.getType()->getAs<clang::RecordType>())
      .transform([](const clang::RecordType *record) {
        return llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
          record->getDecl());
      })
      .value_or(nullptr);

  if (!decl) {
    a.errors.internal.emplace_back(
      "reflection query TypeLoc did not resolve to a class template "
      "specialization");

    return a;
  }

  const clang::TemplateArgumentList &args = decl->getTemplateArgs();

  if (!std::ranges::all_of(args.asArray(), impl::is_concrete_template_arg)) {
    return a;
  }

  const clang::ClassTemplateDecl *tmpl = decl->getSpecializedTemplate();
  const std::string query =
    tmpl ? tmpl->getQualifiedNameAsString() : decl->getQualifiedNameAsString();

  return impl::append_invalid_reflection_query(std::move(a),
    ast,
    query,
    type_loc.getSourceRange(),
    type_loc.getSourceRange(),
    clang::DynTypedNode::create(type_loc),
    "reflection query class template instantiated during the tool run");
}

meta::source_file_context meta::matches::fold_out_of_scope_function_query(
  clang::ASTContext &ast,
  meta::source_file_context a,
  const clang::DeclRefExpr &query) {
  const auto *decl = llvm::dyn_cast<clang::FunctionDecl>(query.getDecl());

  if (!decl) {
    a.errors.internal.emplace_back(
      "reflection query DeclRefExpr did not resolve to a function template "
      "specialization");

    return a;
  }

  return impl::append_invalid_reflection_query(std::move(a),
    ast,
    decl->getQualifiedNameAsString(),
    query.getSourceRange(),
    query.getSourceRange(),
    clang::DynTypedNode::create(query),
    "reflection query function instantiated during the tool run");
}

meta::source_file_context meta::matches::fold_reflected_call(
  const diagnostics &log,
  clang::Sema &sema,
  clang::ASTContext &ast,
  meta::source_file_context a,
  const clang::CXXOperatorCallExpr &call) {
  if (call.getNumArgs() <= 2)
    return a;

  const clang::Type *call_object_type =
    ast.getCanonicalType(call.getArg(0)->getType())
      .getUnqualifiedType()
      .getTypePtr();

  const auto *call_object_decl = call_object_type->getAsCXXRecordDecl();

  if (!call_object_decl
    || "omni::reflected_call_t"
      != call_object_decl->getQualifiedNameAsString()) {
    return a;
  }

  for (std::expected arg : call.arguments()
      // Skip the reflected_call object and visitor arguments.
      | std::views::drop(2)
      | std::views::transform(
        std::bind_front(impl::validate_reflected_call_expression,
          std::ref(sema),
          std::ref(ast),
          std::cref(call)))) {
    if (!arg) {
      std::visit(
        [&a]<typename Error>(Error error) {
          if constexpr (std::same_as<diag::source_error, Error>) {
            a.errors.reflected_call_args.push_back(std::move(error));
          } else {
            static_assert(std::same_as<std::string, Error>);
            a.errors.internal.emplace_back(std::move(error));
          }
        },
        std::move(arg).error());

      continue;
    }

    std::expected reflected = meta::resolve_reflected_type(log,
      arg->type,
      ast,
      a.resolved_concrete_types);

    if (!reflected) {
      a.errors.internal.emplace_back(std::move(reflected).error());

      continue;
    }

    const meta::type_id id =
      std::visit([](const auto &type) { return type.id; }, reflected->type);

    const meta::type_id concrete_id =
      std::visit([](const auto &type) { return type.concrete_id; },
        reflected->type);

    const meta::reflectable *resolved_type = std::visit(
      [&a]<typename Type>(const Type &type) -> const meta::reflectable * {
        if constexpr (std::same_as<meta::reflectable, Type>) {
          return &type;
        } else if constexpr (std::same_as<meta::already_reflected, Type>) {
          const auto found = std::ranges::find(a.reflected,
            type.concrete_id,
            &meta::reflectable::concrete_id);

          return a.reflected.end() == found ? nullptr : &*found;
        } else {
          static_assert(std::same_as<meta::non_reflectable, Type>);
          return nullptr;
        }
      },
      reflected->type);

    if (!resolved_type) {
      a.errors.internal.emplace_back(std::format(
        "validated reflected_call argument '{}' did not resolve to reflected "
        "metadata",
        id));

      continue;
    }

    a.direct_concrete_types.emplace(concrete_id);

    if (!impl::is_forward_declarable(*resolved_type)
      && !impl::is_nested_declarable(*resolved_type)) {
      const auto [reason, suggestion] = std::invoke([resolved_type] {
        const meta::type_definition &definition = resolved_type->definition;

        if (meta::type_definition::local & definition.definition_flags) {
          return std::pair{
            "local types are not supported as reflected_call inputs"sv,
            "move the type to namespace or public class scope"sv,
          };
        }

        if (meta::type_definition::non_public & definition.definition_flags) {
          return std::pair{
            "non-public types cannot be named by generated metadata"sv,
            "make the type public or pass a public reflectable wrapper"sv,
          };
        }

        if (!definition.type_name.name) {
          return std::pair{
            "unnamed types are not supported as reflected_call inputs"sv,
            "give the type a name or pass a named reflectable wrapper"sv,
          };
        }

        if (!definition.type_name.enclosing_records.empty()
          && definition.type_name.enclosing_records.front().empty()) {
          return std::pair{
            "types enclosed by unnamed records cannot be named by generated "
            "metadata"sv,
            "name the enclosing record or pass a named reflectable wrapper"sv,
          };
        }

        return std::pair{
          "this enum cannot be forward-declared by generated metadata"sv,
          "use a scoped enum or specify a fixed underlying type"sv,
        };
      });

      std::visit(
        [&a, id]<typename Error>(Error error) {
          if constexpr (std::same_as<diag::source_error, Error>) {
            a.indexed_arg_candidates.push_back({
              .type = id,
              .error = std::move(error),
            });
          } else {
            static_assert(std::same_as<std::string, Error>);
            a.errors.internal.emplace_back(std::move(error));
          }
        },
        impl::reflected_call_arg_error_from({
          .ast = ast,
          .call = call,
          .arg = *arg->reflected_call_expr,
          .rendered_type = impl::reflected_call_arg_name(ast, arg->type),
          .reason = std::string{reason},
          .suggestion = std::string{suggestion},
        }));
    }

    a.skipped_virtual_base_dependencies.merge(
      reflected->skipped_virtual_base_dependencies);

    std::visit(
      [&a]<typename Type>(Type type) {
        a.resolved_types.emplace(type.id);
        a.resolved_concrete_types.emplace(type.concrete_id);

        if constexpr (std::same_as<meta::reflectable, Type>) {
          a.type_name_by_id.emplace(type.id, type.definition.type_name);
          a.reflected.emplace_back(std::move(type));
        } else if constexpr (std::same_as<meta::non_reflectable, Type>) {
          // Validation should reject non-reflectable direct arguments.
        } else {
          static_assert(std::same_as<meta::already_reflected, Type>);
        }
      },
      std::move(reflected->type));

    std::ranges::copy(reflected->dependencies
        | std::views::transform(&meta::reflectable::id),
      std::inserter(a.resolved_types, a.resolved_types.begin()));

    std::ranges::copy(reflected->dependencies
        | std::views::transform(&meta::reflectable::concrete_id),
      std::inserter(a.resolved_concrete_types,
        a.resolved_concrete_types.begin()));

    std::ranges::copy(reflected->dependencies
        | std::views::transform([](const meta::reflectable &dependency) {
            return std::pair{dependency.id, dependency.definition.type_name};
          }),
      std::inserter(a.type_name_by_id, a.type_name_by_id.end()));

    for (auto &[type, dependencies] : reflected->dependencies_by_type) {
      std::ranges::move(dependencies,
        std::inserter(a.dependencies_by_type[type],
          a.dependencies_by_type[type].end()));
    }

    for (const auto &[type, public_access_path] :
      reflected->public_access_path_by_type) {
      const auto found =
        std::ranges::find(a.reflected, type, &meta::reflectable::concrete_id);

      if (a.reflected.end() != found
        && (meta::type_definition::non_public
          & found->definition.definition_flags)
        && found->public_access_path.steps.empty()) {
        found->public_access_path = public_access_path;
      }
    }

    std::ranges::move(reflected->dependencies, std::back_inserter(a.reflected));
  }

  return a;
}

meta::source_file_context meta::matches::fold_index_registration(
  const clang::ASTContext &ast,
  meta::source_file_context a,
  const clang::ClassTemplateSpecializationDecl &decl) {
  const clang::TemplateArgumentList &args = decl.getTemplateArgs();

  if (args.size() <= 1) {
    a.errors.internal.emplace_back("invalid _reflected_indexed_type signature");

    return a;
  }

  const clang::TemplateArgument &type_arg = args.get(0);

  if (clang::TemplateArgument::Type != type_arg.getKind()) {
    a.errors.internal.emplace_back(
      "invalid _reflected_indexed_type signature: non-type first argument");

    return a;
  }

  const clang::TemplateArgument &index_arg = args.get(1);

  if (clang::TemplateArgument::Integral != index_arg.getKind()) {
    a.errors.internal.emplace_back(
      "invalid _reflected_indexed_type signature: non-integral index argument");

    return a;
  }

  const clang::Type &type = *type_arg.getAsType();
  const meta::type_id id = clang::isa<clang::TagType>(type)
    ? meta::reflectable_type_id(ast, clang::cast<clang::TagType>(&type))
    : meta::canonical_type_id(ast, &type);

  a.index_by_type_id.emplace(id, index_arg.getAsIntegral().getExtValue());

  return a;
}

meta::source_file_context meta::matches::finalize(const diagnostics &log,
  meta::source_file_context ctx) {
  for (meta::indexed_arg_candidate &&candidate :
    ctx.indexed_arg_candidates | std::views::as_rvalue) {
    if (!ctx.index_by_type_id.contains(candidate.type))
      ctx.errors.reflected_call_args.push_back(std::move(candidate.error));
  }

  for (const meta::type_id &type_name : ctx.skipped_virtual_base_dependencies) {
    log(log_level::warning, [&type_name] {
      return std::format(
        "\nreflection dependency '{}' skipped: "
        "records with virtual bases are not supported.",
        type_name);
    });
  }

  const auto unsupported_record_reason =
    [](const meta::reflectable &type) -> std::optional<std::string_view> {
    return std::visit(
      []<meta::reflectable_data Data>(const Data &data) //
      -> std::optional<std::string_view> {
        if constexpr (std::same_as<meta::record_data, Data>) {
          if (data.has_template_parent)
            return "records nested inside template records are not supported"sv;

          if (!data.template_info)
            return std::nullopt;

          switch (data.template_info->support) {
          case meta::template_data::supported:
            return std::nullopt;
          case meta::template_data::partial_specialization:
            return "partial template specializations are not supported"sv;
          case meta::template_data::explicit_specialization:
            return "explicit template specializations are not supported"sv;
          }

          assert(false && "unreachable");
          return std::nullopt;
        } else {
          static_assert(std::same_as<meta::enum_data, Data>);
          return std::nullopt;
        }
      },
      type.data);
  };

  const auto is_public_base = [&ctx](const meta::type_id &concrete_id) {
    return std::ranges::any_of(ctx.reflected,
      [&concrete_id](const meta::reflectable &type) {
        return std::visit(
          [&concrete_id](const meta::reflectable_data auto &data) {
            if constexpr (std::same_as<meta::record_data,
                            std::remove_cvref_t<decltype(data)>>) {
              return std::ranges::contains(data.public_bases,
                concrete_id,
                &meta::reflectable_reference::concrete_id);
            } else {
              return false;
            }
          },
          type.data);
      });
  };

  std::set<meta::type_id> skipped_concrete_types;

  for (const meta::reflectable &type : ctx.reflected) {
    if (ctx.direct_concrete_types.contains(type.concrete_id))
      continue;

    const std::optional record_reason = unsupported_record_reason(type);
    const std::optional reason = //
      record_reason.or_else([&type]() -> std::optional<std::string_view> {
        if (impl::is_forward_declarable(type)
          || impl::is_nested_declarable(type)) {
          return std::nullopt;
        }

        return "the type cannot be named by a supported generated metadata route"sv;
      });

    if (!reason)
      continue;

    if (!record_reason && ctx.index_by_type_id.contains(type.id))
      continue;

    const bool public_base = is_public_base(type.concrete_id);
    log(log_level::warning, [&type, reason, public_base] {
      return public_base
        ? std::format(
            "\nunsupported public base dependency '{}' skipped "
            "(declared at {}): {}. "
            "Inherited fields from this base will not be reflected.",
            meta::format(type.definition.type_name),
            diag::format_location(type.definition.location),
            *reason)
        : std::format(
            "\nunsupported reflection dependency '{}' skipped "
            "(declared at {}): {}.",
            meta::format(type.definition.type_name),
            diag::format_location(type.definition.location),
            *reason);
    });

    skipped_concrete_types.emplace(type.concrete_id);
  }

  std::set reachable_types = ctx.direct_concrete_types;
  std::vector<meta::type_id> to_visit{ctx.direct_concrete_types.begin(),
    ctx.direct_concrete_types.end()};

  while (!to_visit.empty()) {
    const meta::type_id type = std::move(to_visit.back());
    to_visit.pop_back();

    if (!ctx.dependencies_by_type.contains(type))
      continue;

    for (const meta::type_id &dependency : ctx.dependencies_by_type.at(type)) {
      if (skipped_concrete_types.contains(dependency))
        continue;

      if (reachable_types.emplace(dependency).second)
        to_visit.push_back(dependency);
    }
  }

  std::set<meta::type_id> retained_concrete_types;
  std::erase_if(ctx.reflected, [&](const meta::reflectable &type) {
    return skipped_concrete_types.contains(type.concrete_id)
      || !reachable_types.contains(type.concrete_id)
      || !retained_concrete_types.emplace(type.concrete_id).second;
  });

  const std::set retained_types = ctx.reflected
    | std::views::transform(&meta::reflectable::id)
    | std::ranges::to<std::set>();

  // todo(high): add a CLI option to promote omitted unsupported public bases
  // from warnings to errors.
  for (meta::reflectable &type : ctx.reflected) {
    std::visit(
      [&retained_concrete_types](meta::reflectable_data auto &data) {
        if constexpr (std::same_as<meta::record_data,
                        std::remove_cvref_t<decltype(data)>>) {
          std::erase_if(data.public_bases,
            [&retained_concrete_types](const auto &base) {
              return !retained_concrete_types.contains(base.concrete_id);
            });
        }
      },
      type.data);
  }

  ctx.resolved_types = retained_types;
  std::ranges::copy(ctx.non_reflectable_types,
    std::inserter(ctx.resolved_types, ctx.resolved_types.end()));

  ctx.resolved_concrete_types = std::move(retained_concrete_types);
  ctx.resolved_as_dependency = ctx.reflected
    | std::views::filter([&ctx](const meta::reflectable &type) {
        return !ctx.direct_concrete_types.contains(type.concrete_id);
      })
    | std::views::transform(&meta::reflectable::id)
    | std::ranges::to<std::set>();

  ctx.type_name_by_id.clear();
  std::ranges::copy(ctx.reflected
      | std::views::transform([](const meta::reflectable &type) {
          return std::pair{type.id, type.definition.type_name};
        }),
    std::inserter(ctx.type_name_by_id, ctx.type_name_by_id.end()));

  ctx.indexed_arg_candidates.clear();
  return ctx;
}

namespace render::impl {

std::string format_primary_template_type_name(const meta::nm_qual_type &t,
  const std::string &primary_name) {
  return std::format("{}",
    std::array{
      std::span{t.enclosing_records},
      std::span{&primary_name, std::size_t{1}},
    } //
      | std::views::join //
      | std_c::views::join_with("::"sv) //
      | util::format_range);
}

std::string format_primary_template_qualified_type_name(
  const meta::nm_qual_type &t,
  const std::string &primary_name) {
  const std::vector namespace_names = t.namespaces //
    | std::views::transform(&meta::namespace_component::name)
    | std::ranges::to<std::vector>();

  return std::format("{}",
    std::array{
      std::span{namespace_names},
      std::span{t.enclosing_records},
      std::span{&primary_name, std::size_t{1}},
    } //
      | std::views::join //
      | std_c::views::join_with("::"sv) //
      | util::format_range);
}

std::string indentation(std::size_t level) {
  return std::string(level * 2, ' ');
}

std::string format_template_params(
  const std::vector<meta::template_param> &params,
  std::size_t indent_level) {
  const auto format_list = //
    [](this auto self,
      const std::vector<meta::template_param> &params,
      std::size_t level) -> std::string {
    std::string out;

    for (const auto &[index, param] : params | std_c::views::enumerate) {
      if (0 != index)
        out += std::format(",\n{}", indentation(level));

      out += std::visit(
        [self, level]<typename Param>(const Param &p) -> std::string {
          if constexpr (std::same_as<meta::template_type_param, Param>) {
            return std::format("typename{}{}", p.is_pack ? "..." : " ", p.name);
          } else if constexpr (std::same_as<meta::template_value_param,
                                 Param>) {
            return std::format("{}{}{}",
              p.type,
              p.is_pack ? "..." : " ",
              p.name);
          } else {
            static_assert(std::same_as<meta::template_template_param, Param>);

            return std::format("template <\n{0}{1}\n{2}> class{3}{4}",
              indentation(level + 1),
              self(p.params, level + 1),
              indentation(level),
              p.is_pack ? "..." : " ",
              p.name);
          }
        },
        param);
    }

    return out;
  };

  return format_list(params, indent_level);
}

std::string format_template_args(const meta::template_data &t) {
  return std::format("{}",
    t.params //
      | std::views::transform([](const meta::template_param &param) {
          return std::visit(
            [](const auto &p) {
              return std::format("{}{}", p.name, p.is_pack ? "..." : "");
            },
            param);
        }) //
      | std_c::views::join_with(", "sv) //
      | util::format_range);
}

std::string namespace_opening(const meta::namespace_component &ns) {
  return std::format("{}namespace {} {{",
    ns.is_inline ? "inline " : "",
    ns.name);
}

std::string forward_declaration(const meta::reflectable &t) {
  assert(t.definition.type_name.name);

  const auto format_data_sum =
    [name = meta::format_type_name(t.definition.type_name), &t]<typename... U>(
      const std::variant<U...> &data) -> std::string {
    return std::visit(
      [&name, &t]<typename T>(const T &data) -> std::string {
        if constexpr (std::same_as<meta::enum_data, T>) {
          return std::format("enum {}{}{};",
            data.is_scoped ? "class " : "",
            name,
            data.is_fixed ? std::format(" : {}", data.underlying_type) : "");
        } else {
          static_assert(std::same_as<meta::record_data, T>);
          const meta::record_data &struct_data = data;

          if (struct_data.template_info) {
            return std::format("template <\n  {}\n>\n{} {};",
              format_template_params(struct_data.template_info->params, 1),
              struct_data.type,
              format_primary_template_type_name(t.definition.type_name,
                struct_data.template_info->primary_name));
          }

          return std::format("{} {};", struct_data.type, name);
        }
      },
      data);
  };

  return //
    std::ranges::to<std::string>( //
      std::array{
        t.definition.type_name.namespaces //
          | std::views::transform(namespace_opening) //
          | std_c::views::join_with("\n"sv) //
          | std::ranges::to<std::string>(),

        std::format("// declared at: {}\n{}",
          diag::format_location(t.definition.location),
          format_data_sum(t.data)),

        t.definition.type_name.namespaces //
          | std::views::reverse //
          | std::views::transform([](const meta::namespace_component &ns) {
              return std::format("}} // namespace {}", ns.name);
            }) //
          | std_c::views::join_with("\n"sv) //
          | std::ranges::to<std::string>(),
      } //
      | std::views::filter([](std::string_view s) { return !s.empty(); }) //
      | std_c::views::join_with("\n"sv));
}

} // namespace render::impl

namespace render {

std::expected<fs::path, std::string> write_dependencies_file(
  const fs::path &generated_file,
  const util::viewable_range_of<fs::path> auto &includes) {
  fs::path depfile = generated_file;
  depfile += ".d";

  if (auto parent = depfile.parent_path(); !parent.empty()) {
    if (std::error_code ec; (fs::create_directories(parent, ec), ec)) {
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
            | std_c::views::join_with(" \\\n  "sv) //
            | util::format_range));

  os.close();
  if (!os)
    return std::unexpected(std::format("write/close('{}')", depfile.string()));

  if (std::error_code ec; (fs::rename(tmp, depfile, ec), ec)) {
    std::invoke([&tmp] {
      std::error_code ec;
      fs::remove(tmp, ec);
    });

    return std::unexpected(
      std::format("rename('{}'): {}", depfile.string(), ec.message()));
  }

  return depfile;
}

} // namespace render

app_error processing_error(const fs::path &source, std::string error) {
  return {
    .code = -1,
    .message = std::format("\nFailed to process '{}' with error: {}",
      source.string(),
      std::move(error)),
  };
}

std::expected<fs::path, app_error> infer_resource_dir(const char *argv0) {
  const fs::path executable =
    llvm::sys::fs::getMainExecutable(argv0, (void *)(intptr_t)&cli::parse);

  if (executable.empty()) {
    return std::unexpected(app_error{
      .code = -1,
      .message =
        "Failed to infer Clang resource dir: executable path is unknown. Pass --resource-dir explicitly.",
    });
  }

  const fs::path clang_dir =
    executable.parent_path().parent_path() / "share/omnirefl/clang";

  if (std::error_code ec; !fs::is_directory(clang_dir, ec)) {
    return std::unexpected(app_error{
      .code = -1,
      .message = std::format(
        "Failed to infer Clang resource dir: {} is not a directory.{}Pass --resource-dir explicitly.",
        clang_dir.generic_string(),
        ec ? std::format(" Error: {}. ", ec.message()) : " "),
    });
  }

  struct resource_dir_scan {
    std::vector<fs::path> candidates;
    std::vector<std::string> skipped;
  };

  const resource_dir_scan scan = std::ranges::fold_left(
    fs::directory_iterator(clang_dir),
    resource_dir_scan{},
    [](resource_dir_scan acc, const fs::directory_entry &entry) {
      const bool is_dir = std::invoke([&entry, &acc] {
        std::error_code ec;
        const bool result = entry.is_directory(ec);
        if (ec) {
          acc.skipped.push_back(
            std::format("{}: {}", entry.path().generic_string(), ec.message()));
        }
        return !ec && result;
      });

      if (!is_dir)
        return acc;

      const bool has_include = std::invoke([&entry, &acc] {
        std::error_code ec;
        const fs::path include = entry.path() / "include";
        const bool result = fs::is_directory(include, ec);
        if (ec) {
          acc.skipped.push_back(
            std::format("{}: {}", include.generic_string(), ec.message()));
        }
        return !ec && result;
      });

      if (has_include)
        acc.candidates.push_back(entry.path());

      return acc;
    });

  if (1 == scan.candidates.size())
    return scan.candidates.front();

  return std::unexpected(app_error{
    .code = -1,
    .message = std::format(
      "Failed to infer Clang resource dir: {} contains {} candidate(s).{}Pass --resource-dir explicitly.",
      clang_dir.generic_string(),
      scan.candidates.size(),
      scan.skipped.empty()
        ? " "
        : std::format("\nSkipped entries:\n{}\n",
            scan.skipped //
              | std_c::views::join_with("\n"sv) //
              | util::format_range)),
  });
}

std::optional<log_level> parse_log_level(std::string_view name) {
  static const std::map<std::string_view, log_level> log_level_by_name =
    log_levels //
    | std::views::transform(
      [](const auto &level) { return std::pair{level.second, level.first}; }) //
    | std::ranges::to<std::map<std::string_view, log_level>>();

  if (!log_level_by_name.contains(name))
    return std::nullopt;

  return log_level_by_name.at(name);
}

std::expected<cli::options, app_error> cli::parse(int argc,
  const char *const *argv) noexcept {
  // refactorme: app description and cli parameters should be at the start
  // of the file for clear undestanding, before 'main'
  CLI::App app{
    "\nC++ reflection header generator."
    "\n"
    "\nGenerates a .hpp header containing reflection metadata for a given .cpp file."
    "\nThe header must be implicitly included at the start of the translation unit."
    "\n"
    "\nUsage: omnirefl -o <reflection.hpp> -c <source.cpp> -- <compiler command...>"
    "\n  WARNING: Uses compile time counters via friend injection, which is not guaranteed"
    "\n           by the C++ Standard to be consistent between compiler implementations.",
  };

  app.set_version_flag("--version",
    std::format("omnirefl {} (commit {})",
      OMNIREFL_VERSION,
      std::string_view{OMNIREFL_COMMIT}.empty() //
        ? "unknown"
        : OMNIREFL_COMMIT));

  CLI::Option *const resource_dir = //
    app
      .add_option("--resource-dir",
        "Override inferred Clang resource directory.")
      ->check(CLI::ExistingDirectory);

  CLI::Option *const source = //
    app.add_option("-c", ".cpp file path to run the tool on.")
      ->type_name("FILE")
      ->check(CLI::ExistingFile)
      ->required();

  CLI::Option *const out = //
    app.add_option("-o,--out", "output directory (may contain filename)")
      ->type_name("PATH")
      ->default_val(fs::current_path().generic_string());

  CLI::Option *const no_annotations = //
    app
      .add_flag("--no-annotations",
        "Disable reflected documentation comment annotations.")
      ->configurable(false);

  const std::string log_level_help = std::format("Log level: {}.",
    log_levels //
      | std::views::values | std_c::views::join_with(", "sv)
      | util::format_range);

  CLI::Option *const cli_log_level = //
    app.add_option("--log-level", log_level_help)
      ->check(CLI::IsMember(log_levels | std::views::values
        | std::ranges::to<std::vector<std::string>>()))
      ->default_val(std::format("{}", log_level::info))
      ->configurable(false);

  CLI::Option *const timings = //
    app
      .add_flag("--timings",
        "Print action timings. Defaults to enabled unless --log-level silent is "
        "selected.")
      ->configurable(false);

  app.allow_extras();

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    std::stringstream ss;
    const auto code = app.exit(e, ss, ss);
    return std::unexpected(app_error{
      .code = code,
      .message = std::move(ss).str(),
    });
  }

  std::vector flags = app.remaining();
  if (!flags.empty() && "--" == flags.front())
    flags.erase(flags.begin());

  if (flags.empty()) {
    return std::unexpected(app_error{
      .code = -1,
      .message = "Missing compiler command after `--`.",
    });
  }

  const fs::path parsed_source =
    fs::absolute(source->as<fs::path>()).lexically_normal();

  const std::string level = cli_log_level->as<std::string>();
  const std::optional parsed_level = parse_log_level(level);
  if (!parsed_level) {
    return std::unexpected(app_error{
      .code = -1,
      .message = std::format("Unknown log level: {}", level),
    });
  }

  std::expected parsed_resource_dir = 0 == resource_dir->count()
    ? infer_resource_dir(argv[0])
    : std::expected<fs::path, app_error>{resource_dir->as<fs::path>()};

  if (!parsed_resource_dir)
    return std::unexpected(std::move(parsed_resource_dir).error());

  return options{
    .source = parsed_source,
    .flags = std::move(flags),
    .out = out->as<fs::path>(),
    .resource_dir = std::move(*parsed_resource_dir),
    .annotations = !no_annotations->as<bool>(),
    .level = *parsed_level,
    .timings = 0 == timings->count() ? log_level::silent != *parsed_level
                                     : timings->as<bool>(),
  };
}

std::expected<llvm::opt::InputArgList, std::string>
  parse_driver_args(std::span<const std::string> flags, bool cl_style) {
  std::vector<const char *> cli_ref;
  cli_ref.reserve(flags.size());
  std::ranges::transform(flags,
    std::back_inserter(cli_ref),
    [](std::string_view s) { return s.data(); });

  unsigned missing_arg, missing_arg_c;

  // ad hoc: this allows to translate to cc1 options (i.e. msvc -> cc1)
  llvm::opt::InputArgList argList = clang::getDriverOptTable().ParseArgs(
    llvm::ArrayRef(cli_ref.data(), cli_ref.data() + cli_ref.size()),
    missing_arg,
    missing_arg_c,
    llvm::opt::Visibility{static_cast<unsigned>(cl_style
        ? clang::options::ClangOption | clang::options::CLOption
        : clang::options::ClangOption)});

  if (missing_arg != missing_arg_c) {
    return std::unexpected(
      std::format("Error: Missing argument for option at index {}\n",
        missing_arg));
  }
  return argList;
}

std::vector<std::string> filter_tool_irrelevant_args(
  const llvm::opt::InputArgList &args) {
  namespace options = clang::options;

  // ad hoc: preserve frontend options by default, but treat driver action,
  // linker, output and optimization groups as outside reflection fidelity.
  // Optimization-defined macros are consequently not modeled.

  // ad hoc: PCH state is not reusable after omnirefl replaces the source,
  // resource directory and frontend action. Keep the manual option list close
  // to the filter that discards it.
  static constexpr std::array k_pch_option_ids{
    options::OPT_include_pch,
    options::OPT_fpch_codegen,
    options::OPT_fpch_debuginfo,
    options::OPT_fpch_instantiate_templates,
    options::OPT_fpch_preprocess,
    options::OPT_fpch_validate_input_files_content,
    options::OPT_pch_through_hdrstop_create,
    options::OPT_pch_through_hdrstop_use,
    options::OPT_pch_through_header_EQ,
    options::OPT__SLASH_Fp,
    options::OPT__SLASH_Yc,
    options::OPT__SLASH_Yd,
    options::OPT__SLASH_Yl,
    options::OPT__SLASH_Yu,
  };

  llvm::opt::ArgStringList rendered;
  for (const llvm::opt::Arg *arg : args) {
    const llvm::opt::Option option = arg->getOption().getUnaliasedOption();

    if (llvm::opt::Option::InputClass == option.getKind()
      || option.hasFlag(options::LinkOption)
      || option.hasFlag(options::LinkerInput)
      || option.matches(options::OPT_Action_Group)
      || option.matches(options::OPT_M_Group)
      || option.matches(options::OPT_O_Group)
      || options::OPT_o == option.getID()
      || options::OPT__SLASH_Fo == option.getID()
      || options::OPT_resource_dir == option.getID()
      || options::OPT_working_directory == option.getID()
      || std::ranges::contains(k_pch_option_ids, option.getID())) {
      continue;
    }

    // Preserve user-written aliases such as /external:I and /FI when the
    // filtered arguments are passed back to clang-cl.
    if (const llvm::opt::Arg *alias = arg->getAlias())
      alias->render(args, rendered);
    else
      arg->render(args, rendered);
  }

  return rendered //
    | std::views::transform(fn::as<std::string>) //
    | std::ranges::to<std::vector>();
}

// refactorme: this is an ugly shite
auto pipeline::to_compiler_invocation(const diagnostics &log,
  const cli::options &cli_args) noexcept
  -> std::expected<compiler_invocation, app_error> {
  namespace options = clang::options;
  const fs::path &resource_dir = cli_args.resource_dir;
  const fs::path &source = cli_args.source;
  const std::vector<std::string> &raw_flags = cli_args.flags;
  const std::string source_path = source.generic_string();
  const bool windows_drive_mount_used =
    3 <= source_path.size() && '/' == source_path[0]
    && std::isalpha(static_cast<unsigned char>(source_path[1]))
    && '/' == source_path[2];
  const std::string driver_source = windows_drive_mount_used
    ? std::format("{}:{}", source_path[1], source_path.substr(2))
    : source_path;

  // Omnirefl supplies its own source and produces no compiler output.
  static constexpr std::array k_ignored_action_options{
    "-c"sv,
    "/c"sv,
  };

  static constexpr std::array k_ignored_output_options{
    "-o"sv,
    "/Fo"sv,
    "/Fo:"sv,
  };

  static constexpr std::array k_cl_driver_names{
    "cl"sv,
    "cl.exe"sv,
    "clang-cl"sv,
    "clang-cl.exe"sv,
  };

  const std::string compiler_name =
    llvm::StringRef{fs::path(raw_flags.front()).filename().string()}.lower();

  const bool invoked_as_cl =
    std::ranges::contains(k_cl_driver_names, compiler_name);

  const bool cl_style =
    invoked_as_cl || std::ranges::contains(raw_flags, "--driver-mode=cl"sv);

  // std::views::pairwise is unavailable in Cosmopolitan's libc++; this variant
  // preserves boundary flags by making their missing neighbors optional.
  struct flag_context {
    std::optional<std::string_view> previous;
    std::string_view current;
    std::optional<std::string_view> next;
  };

  const auto flag_contexts = [&raw_flags] {
    return std::views::iota(raw_flags.cbegin(), raw_flags.cend()) //
      | std::views::transform(
        [begin = raw_flags.cbegin(), end = raw_flags.cend()](const auto flag) {
          const auto next = std::ranges::next(flag);
          return flag_context{
            begin == flag
              ? std::optional<std::string_view>{}
              : std::optional<std::string_view>{
                  *std::ranges::prev(flag),
                },
            std::string_view{*flag},
            end == next
              ? std::optional<std::string_view>{}
              : std::optional<std::string_view>{*next},
          };
        });
  };

  const auto flags = flag_contexts() //
    | std::views::filter([](const auto &flag) {
        const bool ignored_output_value = flag.previous.has_value()
          && std::ranges::contains(k_ignored_output_options, *flag.previous);

        return !ignored_output_value
          && !std::ranges::contains(k_ignored_action_options,
            flag.current)
          && !std::ranges::contains(k_ignored_output_options,
            flag.current);
      })
    | std::views::transform([windows_drive_mount_used](
                              const flag_context &flag) -> std::string {
        if (!windows_drive_mount_used || 4 > flag.current.size()
          || '@' != flag.current[0]
          || !std::isalpha(static_cast<unsigned char>(flag.current[1]))
          || ':' != flag.current[2]
          || ('/' != flag.current[3] && '\\' != flag.current[3])) {
          return std::string{flag.current};
        }

        // The Windows host passes @C:/... response files, while an APE opens
        // the same drive through its /C/... mount.
        std::string mounted{"@/"};
        mounted += flag.current[1];
        mounted += flag.current.substr(3);
        std::ranges::replace(mounted, '\\', '/');
        return mounted;
      }) //
    | std::ranges::to<std::vector<std::string>>();

  const std::optional<std::string> response_file_directory =
    std::invoke([&flag_contexts] -> std::optional<std::string> {
      const auto flags = flag_contexts() | std::views::reverse;

      const auto found =
        std::ranges::find_if(flags, [](const auto &flag) {
          return flag.current.starts_with("-working-directory="sv)
            || ("-working-directory"sv == flag.current
              && flag.next.has_value());
        });

      if (flags.end() == found)
        return std::nullopt;

      const auto found_flag = *found;
      constexpr std::string_view k_joined = "-working-directory=";
      const std::string_view directory = //
        found_flag.current.starts_with(k_joined) //
        ? found_flag.current.substr(k_joined.size())
        : *found_flag.next;

      return fs::absolute(directory).lexically_normal().generic_string();
    });

  // ad hoc: approximate driver response-file expansion by selecting the
  // tokenizer from the invocation style and compile-command directory.
  const std::expected expanded_flags = //
    std::invoke([&flags,
                  response_file_directory,
                  cl_style,
                  &driver_source,
                  &source_path,
                  windows_drive_mount_used]
      -> std::expected<std::vector<std::string>, std::string> {
      llvm::BumpPtrAllocator allocator;
      llvm::SmallVector<const char *> args;
      std::ranges::transform(flags,
        std::back_inserter(args),
        &std::string::c_str);

      llvm::cl::ExpansionContext expansion{allocator,
        cl_style //
          ? llvm::cl::TokenizeWindowsCommandLine
          : llvm::cl::TokenizeGNUCommandLine};

      if (response_file_directory)
        expansion.setCurrentDir(*response_file_directory);

      if (llvm::Error error = expansion.expandResponseFiles(args))
        return std::unexpected(llvm::toString(std::move(error)));

      if (const auto unexpanded = std::ranges::find_if(args,
            [](std::string_view arg) { return arg.starts_with('@'); });
        args.end() != unexpanded) {
        return std::unexpected(
          std::format("failed to expand compiler response file: {}",
            *unexpanded));
      }

      return args //
        | std::views::transform(
          [&driver_source,
            &source_path,
            windows_drive_mount_used](std::string_view arg) -> std::string {
            // Clang's Windows driver treats /C/... as the /C option. Restore
            // the drive spelling only for the input selected by omnirefl.
            if (windows_drive_mount_used
              && (source_path == arg || driver_source == arg))
              return driver_source;

            if (!windows_drive_mount_used || 3 > arg.size()
              || !std::isalpha(static_cast<unsigned char>(arg[0]))
              || ':' != arg[1] || ('/' != arg[2] && '\\' != arg[2]))
              return std::string{arg};

            // APE translates drive paths passed by the host, but paths read
            // from response files enter Clang after that translation.
            std::string mounted{"/"};
            mounted += arg[0];
            mounted += arg.substr(2);
            std::ranges::replace(mounted, '\\', '/');
            return mounted;
          }) //
        | std::ranges::to<std::vector>();
    });

  if (!expanded_flags)
    return std::unexpected(processing_error(source, expanded_flags.error()));

  const auto to_vector_of_raw_pointers =
    [](const std::vector<std::string> &v) -> std::vector<const char *> {
    return v //
      | std::views::transform(&std::string::c_str)
      | std::ranges::to<std::vector>();
  };

  log(log_level::debug, [&raw_flags] {
    return std::format("\ninput flags:\n{}",
      raw_flags | std_c::views::join_with("\n"sv) | util::format_range);
  });

  const std::expected normalized_args =
    parse_driver_args(std::span{*expanded_flags}.subspan(1), cl_style);

  if (!normalized_args)
    return std::unexpected(processing_error(source, normalized_args.error()));

  const std::optional<std::string> compile_directory =
    std::invoke([&normalized_args] -> std::optional<std::string> {
      const auto *working_directory =
        normalized_args->getLastArgNoClaim(options::OPT_working_directory);

      if (!working_directory)
        return std::nullopt;

      return fs::absolute(working_directory->getValue())
        .lexically_normal()
        .generic_string();
    });

  const std::vector compiler_sources =
    normalized_args->getAllArgValues(options::OPT_INPUT) //
    | std::views::transform(
      [&compile_directory,
        &driver_source,
        &source](std::string_view input) {
        if (driver_source == input)
          return source;

        const auto resolve = [&compile_directory](std::string_view input) {
          return fs::absolute(compile_directory //
              ? fs::path{*compile_directory} / input
              : fs::path{input})
            .lexically_normal();
        };

        const fs::path compiler_source = resolve(input);
        if (source == compiler_source)
          return compiler_source;

        // ad hoc: CMake escapes a literal '$' as '$$' in
        // compile_commands.json. Try the exact spelling first so a real '$$'
        // path remains unchanged.
        std::string cmake_unescaped{input};
        for (std::size_t dollar = cmake_unescaped.find("$$");
          std::string::npos != dollar;
          dollar = cmake_unescaped.find("$$", dollar + 1)) {
          cmake_unescaped.erase(dollar, 1);
        }

        return resolve(cmake_unescaped);
      })
    | std::ranges::to<std::vector>();

  const auto conflicting_source = std::ranges::find_if(compiler_sources,
    [&source](const auto &candidate) { return source != candidate; });

  if (compiler_sources.end() != conflicting_source)
    log(log_level::warning, [&conflicting_source, &source] {
      return std::format(
        "warning: compiler source after `--` differs from omnirefl `-c`; "
        "ignoring '{}', using '{}'\n",
        conflicting_source->generic_string(),
        source.generic_string());
    });

  const std::vector driver_args = filter_tool_irrelevant_args(*normalized_args);

  const bool driver_mode_cl = std::invoke([&normalized_args] {
    if (const auto *dm =
          normalized_args->getLastArgNoClaim(options::OPT_driver_mode))
      return std::string_view(dm->getValue()) == "cl";
    return false;
  });

  const bool msvc_used =
    std::invoke([&normalized_args, driver_mode_cl, invoked_as_cl] {
      const bool has_msvc_style_args =
        normalized_args->hasArg(options::OPT__SLASH_D,
          options::OPT__SLASH_U,
          options::OPT__SLASH_I,
          options::OPT__SLASH_FI);

      return driver_mode_cl || invoked_as_cl || has_msvc_style_args;
    });

  const auto target_triple =
    std::invoke([&] -> std::optional<std::string> {
      if (const auto *target =
            normalized_args->getLastArgNoClaim(options::OPT_target))
        return llvm::Triple::normalize(target->getValue());

      if (compiler_name.starts_with("x86_64-w64-mingw32-"))
        return "x86_64-w64-windows-gnu";
      if (compiler_name.starts_with("i686-w64-mingw32-"))
        return "i686-w64-windows-gnu";
      return std::nullopt;
    });

  const llvm::Triple process_triple{llvm::sys::getProcessTriple()};

  // refactorme(macos_sysroot): replace this inline platform detection.
  // ad hoc: Apple's compiler launcher omits SDK selection from
  // compile_commands.json. A successful xcrun query supplies both the SDK and
  // the APE's runtime operating system.
  const auto macos_sysroot =
    (msvc_used || process_triple.isOSWindows()
      || (target_triple && llvm::Triple{*target_triple}.isOSWindows())) //
    ? std::nullopt
    : std::invoke([] -> std::optional<std::string> {
      const auto xcrun = llvm::sys::findProgramByName("xcrun");
      if (!xcrun)
        return std::nullopt;

      llvm::SmallString<64> output;
      if (llvm::sys::fs::createTemporaryFile("omnirefl-xcrun", "", output))
        return std::nullopt;

      llvm::FileRemover remove_output(output);
      if (llvm::sys::ExecuteAndWait(*xcrun,
            {
              llvm::StringRef{*xcrun},
              llvm::StringRef{"--show-sdk-path"},
            },
            /*Env=*/std::nullopt,
            {
              /*stdin=*/std::optional{llvm::StringRef{""}},
              /*stdout=*/std::optional{llvm::StringRef{output}},
              /*stderr=*/std::optional{llvm::StringRef{""}},
            },
            /*SecondsToWait=*/10)
        != 0)
        return std::nullopt;

      const auto buffer = llvm::MemoryBuffer::getFile(output);
      if (!buffer)
        return std::nullopt;

      const llvm::StringRef path = buffer->get()->getBuffer().trim();
      return path.empty() //
        ? std::nullopt
        : std::optional{path.str()};
    });

  const std::string driver_triple = std::invoke([&] {
    llvm::Triple triple = target_triple //
      ? llvm::Triple{*target_triple}
      : process_triple;

    if (msvc_used) {
      triple.setOS(llvm::Triple::Win32);
      triple.setEnvironment(llvm::Triple::MSVC);
    } else if (!target_triple && windows_drive_mount_used) {
      // refactorme(cosmo_runtime): review runtime-platform detection.
      // ad hoc: a Cosmopolitan process triple identifies its build target, not
      // the operating system currently executing the APE. Drive-mounted source
      // paths identify a native Windows compiler invocation without spawning
      // the compiler once per instrumentation run.
      triple.setVendor(llvm::Triple::PC);
      triple.setOS(llvm::Triple::Win32);
      triple.setEnvironment(llvm::Triple::GNU);
    } else if (!target_triple && macos_sysroot) {
      triple.setVendor(llvm::Triple::Apple);
      triple.setOS(llvm::Triple::Darwin);
      triple.setEnvironment(llvm::Triple::UnknownEnvironment);
    }

    return triple.str();
  });

  const bool mingw_used = llvm::Triple(driver_triple).isWindowsGNUEnvironment();

  const std::vector<fs::path> mingw_include_paths = std::invoke([&] {
    std::vector<fs::path> paths;
    if (!mingw_used || raw_flags.empty())
      return paths;

    const std::string compiler_name =
      fs::path(raw_flags.front()).filename().string();

    std::string gcc_machine = std::invoke([&] {
      if (compiler_name.starts_with("x86_64-w64-mingw32-"))
        return std::string{"x86_64-w64-mingw32"};
      if (compiler_name.starts_with("i686-w64-mingw32-"))
        return std::string{"i686-w64-mingw32"};
      return std::string{};
    });

    const fs::path compiler_path = std::invoke([&] {
      std::string compiler{raw_flags.front()};
      if (windows_drive_mount_used && 3 <= compiler.size()
        && std::isalpha(static_cast<unsigned char>(compiler[0]))
        && ':' == compiler[1]
        && ('/' == compiler[2] || '\\' == compiler[2])) {
        // refactorme(cosmo_windows_path): review runtime path translation.
        // ad hoc: resolve the compiler through the APE's mounted Windows drive.
        compiler = '/' + compiler.substr(0, 1) + compiler.substr(2);
        std::ranges::replace(compiler, '\\', '/');
      }

      std::error_code ec;
      return fs::weakly_canonical(compiler, ec);
    });

    const fs::path compiler_root = compiler_path.parent_path().parent_path();

    if (gcc_machine.empty()) {
      if ("mingw64" == compiler_root.filename())
        gcc_machine = "x86_64-w64-mingw32";
      else if ("mingw32" == compiler_root.filename())
        gcc_machine = "i686-w64-mingw32";
    }

    if (fs::is_directory(compiler_root / "include/c++/v1")) {
      std::array ordered = {
        compiler_root / "include/c++/v1",
        compiler_root / "include",
      };

      std::ranges::copy(ordered | std::views::filter([](const fs::path &path) {
        return fs::is_directory(path);
      }),
        std::back_inserter(paths));

      const fs::path clang_root = compiler_root / "lib/clang";
      if (fs::is_directory(clang_root)) {
        std::vector<fs::path> candidates;
        std::ranges::copy(fs::directory_iterator(clang_root)
            | std::views::filter([](const fs::directory_entry &entry) {
                return entry.is_directory();
              })
            | std::views::transform(&fs::directory_entry::path),
          std::back_inserter(candidates));

        std::ranges::sort(candidates);
        if (!candidates.empty()
          && fs::is_directory(candidates.back() / "include"))
          paths.emplace_back(candidates.back() / "include");
      }

      return paths;
    }

    if (gcc_machine.empty())
      return paths;

    const std::string compiler_variant =
      compiler_path.filename().string().contains("posix") ? "posix" : "win32";

    fs::path gcc_root = compiler_root / "lib/gcc" / gcc_machine;
    if (!fs::is_directory(gcc_root))
      gcc_root = fs::path("/usr/lib/gcc") / gcc_machine;
    if (!fs::is_directory(gcc_root))
      return paths;

    std::vector<fs::path> candidates;
    std::ranges::copy(fs::directory_iterator(gcc_root)
        | std::views::filter(
          [](const fs::directory_entry &entry) { return entry.is_directory(); })
        | std::views::transform(&fs::directory_entry::path),
      std::back_inserter(candidates));

    std::ranges::sort(candidates);

    if (candidates.empty())
      return paths;

    const auto found = std::ranges::find_if(candidates,
      [&compiler_variant](const fs::path &candidate) {
        return candidate.filename().string().contains(compiler_variant);
      });

    const fs::path &gcc_dir =
      candidates.end() == found ? candidates.back() : *found;

    // refactorme(mingw_include_paths): review explicit distribution layouts.
    // ad hoc: Alpine, MSYS2, and Debian store cross-target libstdc++ in
    // different roots; prefer target-qualified layouts before GCC's local
    // layout.
    const std::array cxx_root_candidates = {
      compiler_root / gcc_machine / "include/c++" / gcc_dir.filename(),
      compiler_root / "include/c++" / gcc_dir.filename(),
      gcc_dir / "include/c++",
    };

    const auto cxx_root = std::ranges::find_if(cxx_root_candidates,
      [](const fs::path &path) { return fs::is_directory(path); });

    if (cxx_root_candidates.end() == cxx_root)
      return paths;

    std::array ordered = {
      *cxx_root,
      *cxx_root / gcc_machine,
      *cxx_root / "backward",
      gcc_dir / "include",
      gcc_dir / "include-fixed",
      compiler_root / gcc_machine / "include",
    };

    std::ranges::copy(ordered | std::views::filter([](const fs::path &path) {
      return fs::is_directory(path);
    }),
      std::back_inserter(paths));

    // ad hoc: MSYS2 keeps its CRT here, while the equivalent Linux path is
    // the host /usr/include and would precede project-provided system includes.
    if ((compiler_root.filename() == "mingw64"
          || compiler_root.filename() == "mingw32")
      && fs::is_directory(compiler_root / "include")) {
      paths.emplace_back(compiler_root / "include");
    }

    return paths;
  });

  const std::vector<std::string> cc1_driver_args =
    std::invoke([&] -> std::vector<std::string> {
      // Preserve frontend-affecting driver arguments for every supported
      // compiler spelling; source, output, linker, optimization, dependency,
      // and PCH options were removed above.
      if (msvc_used) {
        std::vector<std::string> out;
        out.reserve(8 + driver_args.size());

        out.emplace_back("omnirefl");
        if (!driver_mode_cl)
          out.emplace_back("--driver-mode=cl");

        std::ranges::copy(driver_args
            | std::views::filter([](std::string_view s) { return !s.empty(); }),
          std::back_inserter(out));

        out.emplace_back(driver_source);

        // force AST-only and tool resource-dir (last-wins)
        out.emplace_back("-fsyntax-only");
        out.emplace_back(
          std::format("-resource-dir={}", resource_dir.generic_string()));

        return out;
      }

      std::vector<std::string> out;
      out.reserve(16 + normalized_args->size());

      out.emplace_back("omnirefl");

      if (!mingw_used) {
        // prevents from picking up on another compiler's C++ < std libs
        out.emplace_back("-nostdinc++");
      } else if (!mingw_include_paths.empty()) {
        // MinGW GCC already provides builtin C headers in its GCC include dir.
        // MSYS2 clang64 provides its own builtin headers too. Mixing Clang's
        // packaged builtin headers with either chain breaks CRT headers such as
        // stddef.h/max_align_t and float.h include_next.
        out.emplace_back("-nobuiltininc");
      }

      std::ranges::for_each(mingw_include_paths, [&out](const fs::path &p) {
        out.emplace_back("-isystem");
        out.emplace_back(p.generic_string());
      });

      std::ranges::copy(driver_args, std::back_inserter(out));

      if (macos_sysroot
        && !normalized_args->hasArg(options::OPT_isysroot,
          options::OPT__sysroot_EQ)) {
        out.emplace_back("-isysroot");
        out.emplace_back(*macos_sysroot);
      }

      out.emplace_back("-fsyntax-only"); //< AST only
      out.emplace_back(
        std::format("-resource-dir={}", resource_dir.generic_string()));

      out.emplace_back(driver_source);

      return out;
    });

  // ad hoc: impersonate the original compiler basename so Clang's driver can
  // infer its toolchain and language defaults; MSVC-style input must instead
  // be normalized through clang-cl.
  clang::driver::Driver driver(
    msvc_used ? "clang-cl" : fs::path(raw_flags.front()).filename().string(),
    driver_triple,
    *log.clang_engine,
    "omnirefl reflection tool");

  const std::expected<std::optional<std::string>, app_error>
    driver_directory_to_restore =
      std::invoke([&driver, &compile_directory, &source]
        -> std::expected<std::optional<std::string>, app_error> {
        if (!compile_directory)
          return std::nullopt;

        const llvm::ErrorOr<std::string> current_directory =
          driver.getVFS().getCurrentWorkingDirectory();

        if (!current_directory) {
          return std::unexpected(processing_error(source,
            std::format("failed to read the current working directory: {}",
              current_directory.getError().message())));
        }

        if (const std::error_code ec =
              driver.getVFS().setCurrentWorkingDirectory(*compile_directory);
          ec) {
          return std::unexpected(processing_error(source,
            std::format("failed to use compiler working directory '{}': {}",
              *compile_directory,
              ec.message())));
        }

        return *current_directory;
      });

  if (!driver_directory_to_restore)
    return std::unexpected(driver_directory_to_restore.error());

  driver.setCheckInputsExist(false);

  const std::unique_ptr<clang::driver::Compilation> compilation(
    driver.BuildCompilation(
      llvm::ArrayRef(to_vector_of_raw_pointers(cc1_driver_args))));

  if (*driver_directory_to_restore) {
    if (const std::error_code ec = driver.getVFS().setCurrentWorkingDirectory(
          **driver_directory_to_restore);
      ec) {
      return std::unexpected(processing_error(source,
        std::format("failed to restore compiler working directory '{}': {}",
          **driver_directory_to_restore,
          ec.message())));
    }
  }

  if (!compilation || compilation->getJobs().empty()) {
    return std::unexpected(processing_error(source,
      std::format("Failed to build compilatioin for: {}.\n",
        source.generic_string())));
  }

  const auto &compilation_args = compilation->getJobs().begin()->getArguments();

  const std::vector<std::string> frontend_args = compilation_args //
    | std::views::transform(
      [&driver_source,
        &source_path,
        windows_drive_mount_used](llvm::StringRef arg) {
        // The Windows driver needs C:/... to recognize an input, while the
        // Cosmopolitan frontend VFS needs /C/... to open the same file.
        return windows_drive_mount_used && driver_source == arg
          ? source_path
          : arg.str();
      }) //
    | std::ranges::to<std::vector>();

  log(log_level::debug, [&frontend_args] {
    return std::format("\nusing cc1 args:\n{}",
      frontend_args | std_c::views::join_with("\n"sv) | util::format_range);
  });

  std::shared_ptr clang_invocation =
    std::make_shared<clang::CompilerInvocation>();

  if (!clang::CompilerInvocation::CreateFromArgs(*clang_invocation,
        to_vector_of_raw_pointers(frontend_args),
        *log.clang_engine)) {
    return std::unexpected(
      processing_error(source, "Failed to create CompilerInvocation."));
  }

  if (compile_directory)
    clang_invocation->getFileSystemOpts().WorkingDir = *compile_directory;

  const auto &frontend_inputs = clang_invocation->getFrontendOpts().Inputs;
  if (1 != frontend_inputs.size()) {
    return std::unexpected(processing_error(source,
      std::format("expected one frontend input, resolved {}",
        frontend_inputs.size())));
  }

  const clang::Language input_language =
    frontend_inputs.front().getKind().getLanguage();

  if (clang::Language::CXX != input_language) {
    return std::unexpected(processing_error(source,
      std::format("omnirefl requires a C++ translation unit, but the compiler "
                  "command selects {}",
        clang::languageToString(input_language).str())));
  }

  {
    clang::HeaderSearchOptions &o = clang_invocation->getHeaderSearchOpts();

    if (msvc_used && windows_drive_mount_used) {
      // The APE exposes %INCLUDE% as a colon-separated /C/... path list. This
      // POSIX-configured Clang expects semicolons for MSVC and otherwise emits
      // the complete list as one search entry. Split that generated entry.
      decltype(o.UserEntries) mounted_entries;
      for (const auto &entry : o.UserEntries) {
        if (3 > entry.Path.size()
          || '/' != entry.Path[0]
          || !std::isalpha(static_cast<unsigned char>(entry.Path[1]))
          || '/' != entry.Path[2]) {
          mounted_entries.emplace_back(entry);
          continue;
        }

        for (std::size_t start = 0; start < entry.Path.size();) {
          std::size_t next = entry.Path.find(':', start + 3);
          while (std::string::npos != next
            && (next + 3 >= entry.Path.size()
              || '/' != entry.Path[next + 1]
              || !std::isalpha(
                static_cast<unsigned char>(entry.Path[next + 2]))
              || '/' != entry.Path[next + 3])) {
            next = entry.Path.find(':', next + 1);
          }

          auto mounted = entry;
          mounted.Path = entry.Path.substr(start, next - start);
          mounted_entries.emplace_back(std::move(mounted));

          if (std::string::npos == next)
            break;
          start = next + 1;
        }
      }
      o.UserEntries = std::move(mounted_entries);
    }

    // Original intent: bundle libc++ so native Linux instrumentation can work
    // as a mostly standalone tool even when no usable host C++ standard library
    // is discovered. This is not a target-independent C++ library setup:
    // libc/CRT and compiler builtin headers still come from the active target
    // toolchain. MinGW/MSVC-style targets must keep their own matched
    // C++/CRT/builtin include chain instead of mixing it with bundled libc++.
    // Note: removing bundled libc++ headers was tried and failed. Host
    // libstdc++ needs compiler-driver discovery for the primary C++ include
    // dir, target-specific bits/c++config.h dir, correct musl/glibc target
    // triple, feature macros, and include ordering. Missing any of those can
    // break standard headers before the tool reaches user code.
    if (!msvc_used && !mingw_used)
      o.UseStandardCXXIncludes = false;
    o.ResourceDir = resource_dir.generic_string();

    if (!msvc_used && !mingw_used) {
      // Packaged headers are absolute and remain outside a target SDK.
      o.AddPath(
        // todo: this should be configured at compile time
        (resource_dir / "include/x86_64-unknown-linux-gnu/c++/v1")
          .generic_string(),
        clang::frontend::IncludeDirGroup::CXXSystem,
        /*IsFramework=*/false,
        /*IgnoreSysRoot=*/true);

      // todo: this should be configured at compile time
      o.AddPath((resource_dir / "include/c++/v1").generic_string(),
        clang::frontend::IncludeDirGroup::CXXSystem,
        /*IsFramework=*/false,
        /*IgnoreSysRoot=*/true);

      // ad hoc: C++ headers must be included before C's
      std::rotate(o.UserEntries.rbegin(),
        o.UserEntries.rbegin() + 2,
        o.UserEntries.rend());
    }
  }

  {
    clang::PreprocessorOptions &p = clang_invocation->getPreprocessorOpts();

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

  {
    clang::LangOptions &lo = clang_invocation->getLangOpts();
    lo.CommentOpts.ParseAllComments = cli_args.annotations;
  }

  {
    clang::PreprocessorOptions &po = clang_invocation->getPreprocessorOpts();
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

    // Remove force-included reflection headers. They belong to the real
    // compilation step, not to the tool run.
    std::erase_if(po.Includes,
      [&out = cli_args.out](const std::string &s) -> bool {
        const std::string filename = fs::path{s}.filename().string();
        return util::is_subpath(s, out) || filename.ends_with(".omnirefl.hpp");
      });

    std::ranges::for_each(po.Macros, [&log](const auto &pair) {
      const auto &[macro, is_undef] = pair;
      log(log_level::debug, [macro, is_undef] {
        return std::format("\npreprocessor #{} {}",
          is_undef ? "undef" : "define",
          macro);
      });
    });

    std::ranges::for_each(po.Includes, [&log](const auto &inc) {
      log(log_level::debug,
        [&inc] { return std::format("\npreprocessor #include \"{}\"", inc); });
    });
  }

  // do not need this noise when parsing the AST
  clang_invocation->getDiagnosticOpts().IgnoreWarnings = 1;

  return compiler_invocation{
    .source = source,
    .ci = std::move(clang_invocation),
  };
}

std::expected<pipeline::parsed_ast, app_error>
  pipeline::to_parsed_ast(const diagnostics &log, compiler_invocation ci) {
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
    clang::ASTUnit::LoadFromCompilerInvocationAction(ci.ci,
      std::make_shared<clang::PCHContainerOperations>(),
      log.clang_options,
      log.clang_engine,
      &action)};

  if (!ast || ast->getDiagnostics().hasUncompilableErrorOccurred())
    return std::unexpected(
      processing_error(ci.source, "Failed to build AST Unit."));

  return parsed_ast{
    .source = std::move(ci.source),
    .ast = std::move(ast),

    .includes_deps = deps_collector.getDependencies()
      | std::views::transform(fn::as<fs::path>) | std::ranges::to<std::set>(),
  };
}

std::expected<meta::source_file_context, app_error>
  pipeline::fold_matches(const diagnostics &log, parsed_ast parsed) {
  using namespace clang::ast_matchers;

  return meta::matches::finalize(log,
    ast::fold_matches(*parsed.ast,
      meta::source_file_context{
        .source = std::move(parsed.source),
        .file_dependencies = std::move(parsed.includes_deps),
      },

      ast::rule{
        .pattern = ast::pattern(typeLoc,
          isExpansionInMainFile(),
          loc(qualType(hasDeclaration(
            classTemplateSpecializationDecl(isTemplateInstantiation(),
              isDefinition(),
              anyOf(hasName("::omni::is_reflected"),
                hasName("::omni::meta_t"),
                hasName("::omni::binding_t"),
                hasName("::omni::field_meta_t"),
                hasName("::omni::field_binding_t")))))),
          // For example:
          //   struct visitor {
          //     template <typename T>
          //     auto operator()(omni::binding_t<T &>) const -> void;
          //   };
          //   omni::reflected_call(visitor{}, value);
          // Clang instantiates the visitor parameter signature while selecting
          // reflected_call, but the explicitly returned body remains deferred.
          unless(hasParent(
            parmVarDecl(hasAncestor(cxxMethodDecl(isTemplateInstantiation(),
              hasOverloadedOperatorName("()")))))),
          unless(hasParent(typeLoc()))),
        .fold = std::bind_front(meta::matches::fold_out_of_scope_type_query,
          std::ref(parsed.ast->getASTContext())),
      },

      ast::rule{
        .pattern = ast::pattern(declRefExpr,
          isExpansionInMainFile(),
          to(functionDecl(isTemplateInstantiation(),
            isDefinition(),
            hasName("::omni::reflected")))),
        .fold = std::bind_front(meta::matches::fold_out_of_scope_function_query,
          std::ref(parsed.ast->getASTContext())),
      },

      ast::rule{
        .pattern =
          ast::pattern(cxxOperatorCallExpr, hasOverloadedOperatorName("()")),
        .fold = std::bind_front(meta::matches::fold_reflected_call,
          std::cref(log),
          std::ref(parsed.ast->getSema()),
          std::ref(parsed.ast->getASTContext())),
      },

      ast::rule{
        .pattern = ast::pattern(classTemplateSpecializationDecl,
          unless(isInStdNamespace()),
          hasAncestor(namespaceDecl(hasName("omni"))),
          hasAncestor(namespaceDecl(hasName("detail"))),
          hasName("_reflected_indexed_type"),
          isTemplateInstantiation(),
          isDefinition()),
        .fold = std::bind_front(meta::matches::fold_index_registration,
          std::cref(parsed.ast->getASTContext())),
      }));
}

std::expected<render::reflection_context, app_error>
  pipeline::resolve_reflection_context(const diagnostics &log,
    meta::source_file_context ctx) {
  using render_context = render::reflection_context;

  const fs::path &source = ctx.source;

  std::vector<std::string> input_errors;

  if (!ctx.errors.reflection_queries.empty()) {
    input_errors.emplace_back(std::format("invalid reflection queries:\n{}",
      ctx.errors.reflection_queries //
        | std::views::transform(diag::format_error) //
        | std_c::views::join_with("\n"sv) //
        | util::format_range));
  }

  if (!ctx.errors.reflected_call_args.empty()) {
    input_errors.emplace_back(std::format(
      "invalid reflected_call arguments:\n{}"
      "\nreflected_call inputs must be directly reflectable records/enums or "
      "omni::type_t<T>. Callers are responsible for sanitizing semantic "
      "compound inputs. Compound types may still be discovered "
      "through supported dependency routes when they appear as reflected "
      "fields, bases, or protocol aliases",
      ctx.errors.reflected_call_args //
        | std::views::transform(diag::format_error) //
        | std_c::views::join_with("\n"sv) //
        | util::format_range));
  }

  if (!ctx.errors.internal.empty()) {
    input_errors.emplace_back(std::format("internal errors:\n{}",
      ctx.errors.internal //
        | std::views::transform([](const std::string &error) {
            return std::format("  {}", error);
          }) //
        | std_c::views::join_with("\n"sv) //
        | util::format_range));
  }

  if (!input_errors.empty()) {
    return std::unexpected(processing_error(source,
      input_errors //
        | std_c::views::join_with("\n\n"sv) //
        | std::ranges::to<std::string>()));
  }

  std::vector<render_context::forward_declarable> fwd;
  std::vector<render_context::nested_type> nested;
  std::vector<render_context::indexed_type> indexed;

  for (meta::reflectable &&t : ctx.reflected | std::views::as_rvalue) {
    const std::optional index = ctx.index_by_type_id.contains(t.id)
      ? std::optional(ctx.index_by_type_id.at(t.id))
      : std::nullopt;

    const bool as_dependency = ctx.resolved_as_dependency.contains(t.id);

    if (meta::matches::impl::is_forward_declarable(t)) {
      if (index) {
        log(log_level::debug, [index, id = t.id] {
          return std::format(
            "\ngenerated fallback '{}' type '{}' will be rendered as "
            "forward-declarable.",
            *index,
            id);
        });
      }

      fwd.push_back({
        .type = std::move(t),
        .index = index,
        .as_dependent = as_dependency,
      });
    } else if (meta::matches::impl::is_nested_declarable(t)) {
      if (index) {
        log(log_level::debug, [index, id = t.id] {
          return std::format(
            "\ngenerated fallback '{}' type '{}' will be rendered as nested "
            "forward-declarable.",
            *index,
            id);
        });
      }

      nested.push_back({
        .type = std::move(t),
        .index = index,
        .as_dependent = as_dependency,
      });
    } else {
      if (!index) {
        return std::unexpected(processing_error(source,
          std::format(
            "internal error: non-emittable reflected type escaped matcher "
            "finalization: '{}'",
            t.id)));
      }

      indexed.push_back({
        .type = std::move(t),
        .index = *index,
      });
    }
  }

  // std::ranges::to<std::vector>() can precompute size for filtered ranges,
  // causing another pass over a stateful dedup predicate. Keep this as explicit
  // single-pass mutation.
  std::set<meta::type_id> emitted_forward_declarables;
  std::erase_if(fwd, [&emitted_forward_declarables](const auto &t) {
    return !emitted_forward_declarables.emplace(t.type.id).second;
  });

  const auto missing_base =
    [&ctx](const meta::reflectable &r) -> std::optional<meta::type_id> {
    return std::visit(
      [&ctx](const meta::reflectable_data auto &data)
        -> std::optional<meta::type_id> {
        if constexpr (std::same_as<meta::record_data,
                        std::remove_cvref_t<decltype(data)>>) {
          for (const meta::reflectable_reference &base : data.public_bases)
            if (!ctx.type_name_by_id.contains(base.id))
              return base.id;
        }

        return std::nullopt;
      },
      r.data);
  };

  for (const render_context::forward_declarable &t : fwd) {
    if (const std::optional base_id = missing_base(t.type))
      return std::unexpected(processing_error(source,
        std::format(
          "internal error: missing reflected public base metadata:"
          "\n  base: '{}'"
          "\n  required by: {} '{}'"
          "\nThis is an internal consistency failure; report it with the "
          "instrumented source file",
          *base_id,
          diag::format_location(t.type.definition.location),
          meta::format(t.type.definition.type_name))));
  }

  for (const render_context::nested_type &t : nested) {
    if (const std::optional base_id = missing_base(t.type))
      return std::unexpected(processing_error(source,
        std::format(
          "internal error: missing reflected public base metadata:"
          "\n  base: '{}'"
          "\n  required by: {} '{}'"
          "\nThis is an internal consistency failure; report it with the "
          "instrumented source file",
          *base_id,
          diag::format_location(t.type.definition.location),
          meta::format(t.type.definition.type_name))));
  }

  for (const render_context::indexed_type &t : indexed) {
    if (const std::optional base_id = missing_base(t.type))
      return std::unexpected(processing_error(source,
        std::format(
          "internal error: missing reflected public base metadata:"
          "\n  base: '{}'"
          "\n  required by: {} '{}'"
          "\nThis is an internal consistency failure; report it with the "
          "instrumented source file",
          *base_id,
          diag::format_location(t.type.definition.location),
          meta::format(t.type.definition.type_name))));
  }

  std::map<std::string, const meta::reflectable *> emitted_by_target;
  const auto validate_target =
    [&source, &emitted_by_target](const std::string &target,
      const meta::reflectable &type) -> std::optional<app_error> {
    if (!emitted_by_target.contains(target)) {
      emitted_by_target.emplace(target, &type);

      return std::nullopt;
    }

    const meta::reflectable &first = *emitted_by_target.at(target);

    return processing_error(source,
      std::format(
        "internal error: reflected metadata specialization target collision:"
        "\n  target: {}"
        "\n  first: {} '{}'"
        "\n  second: {} '{}'",
        target,
        diag::format_location(first.definition.location),
        meta::format(first.definition.type_name),
        diag::format_location(type.definition.location),
        meta::format(type.definition.type_name)));
  };

  const auto template_or_type_target =
    [](const meta::reflectable &t) -> std::string {
    return std::visit(
      [&t]<meta::reflectable_data Data>(const Data &data) {
        if constexpr (std::same_as<meta::record_data, Data>) {
          if (data.template_info)
            return std::format("direct:{}:{}", data.type, t.id);

          return std::format("direct:{}:{}",
            data.type,
            meta::format(t.definition.type_name));
        } else {
          static_assert(std::same_as<meta::enum_data, Data>);
          return std::format("direct:enum:{}",
            meta::format(t.definition.type_name));
        }
      },
      t.data);
  };

  for (const render_context::forward_declarable &t : fwd) {
    if (std::optional error =
          validate_target(template_or_type_target(t.type), t.type)) {
      return std::unexpected(std::move(*error));
    }
  }

  for (const render_context::nested_type &t : nested) {
    if (std::optional error = validate_target(
          std::format("nested:{}", meta::format(t.type.definition.type_name)),
          t.type)) {
      return std::unexpected(std::move(*error));
    }
  }

  for (const render_context::indexed_type &t : indexed) {
    if (std::optional error =
          validate_target(std::format("indexed:{}", t.index), t.type)) {
      return std::unexpected(std::move(*error));
    }
  }

  constexpr auto cmp_roots = //
    [](const meta::nm_qual_type &lhs, const meta::nm_qual_type &rhs) -> bool {
    assert(!lhs.enclosing_records.empty());
    assert(!rhs.enclosing_records.empty());
    return std::tie(lhs.namespaces, lhs.enclosing_records.front())
      < std::tie(rhs.namespaces, rhs.enclosing_records.front());
  };

  std::vector enclosing_roots = nested //
    | std::views::transform([](const render_context::nested_type &n) {
        return n.type.definition.type_name;
      })
    | std::ranges::to<std::set>(cmp_roots) //
    | std::views::as_rvalue //
    | std::ranges::to<std::vector>();

  return render::reflection_context{
    .instrumented_source = std::move(ctx.source),

    .fwd_declarables = std::move(fwd),
    .nested = std::move(nested),
    .indexed = std::move(indexed),
    .enclosing_roots = std::move(enclosing_roots),

    .type_name_by_id = std::move(ctx.type_name_by_id),
    .file_dependencies = std::move(ctx.file_dependencies),
  };
}

std::expected<pipeline::run_report, app_error> pipeline::emit_outputs(
  const diagnostics &log,
  fs::path out,
  render::reflection_context ctx) {
  // Avoid filesystem exceptions when the output path does not exist yet.
  if (std::error_code ec; fs::is_directory(out, ec)) {
    out /= std::format("{}_omni_reflection_header.h",
      ctx.instrumented_source.string());
  }

  log(log_level::info, [&out] {
    return std::format("\ncreating reflection header: {}",
      out.generic_string());
  });

  const fs::path source = ctx.instrumented_source;
  const std::size_t forward_declarable_count = ctx.fwd_declarables.size();
  const std::size_t nested_type_count = ctx.nested.size();
  const std::size_t indexed_type_count = ctx.indexed.size();
  const std::size_t file_dependency_count = ctx.file_dependencies.size();
  const std::size_t reflected_type_count =
    forward_declarable_count + nested_type_count + indexed_type_count;

  return util::create_file_for_writing(out) //
    .and_then(std::bind_front(render::generate_reflection, std::move(ctx)))
    .and_then([=](render::reflection_context ctx) {
      log(log_level::info, [out] {
        return std::format("\nwriting deps file for: {}", out.generic_string());
      });

      return render::write_dependencies_file(out, ctx.file_dependencies)
        .transform([=](fs::path deps) {
          return run_report{
            .instrumented_source = source,
            .reflection_header = out,
            .dependencies = std::move(deps),
            .reflected_type_count = reflected_type_count,
            .forward_declarable_count = forward_declarable_count,
            .nested_type_count = nested_type_count,
            .indexed_type_count = indexed_type_count,
            .file_dependency_count = file_dependency_count,
          };
        });
    })
    .transform_error([&source](std::string error) {
      return processing_error(source, std::move(error));
    });
}

std::string format_definition_flags(int flags) {
  std::string out;

  if (meta::type_definition::non_public & flags) {
    if (!out.empty())
      out += " | ";
    out += "non_public";
  }

  if (meta::type_definition::local & flags) {
    if (!out.empty())
      out += " | ";
    out += "local";
  }

  return out;
}

std::string format_type_definition(const meta::type_definition &d) {
  return std::format(
    "declared at: {}{}"
    "\n  {}",
    diag::format_location(d.location),
    std::invoke([f = format_definition_flags(d.definition_flags)] {
      return f.empty() ? std::string() : std::format("\n  [{}]", f);
    }),

    meta::format(d.type_name));
}

std::string format_record_data(const meta::record_data &d) {
  return std::format("{}{}{}",
    d.type,
    d.template_info
      .transform([](const meta::template_data &t) {
        return meta::template_data::supported == t.support
          ? std::string{}
          : std::format(" [{}]", t.support);
      })
      .value_or(""),
    d.public_fields.empty() //
      ? " {}"
      : std::format(" {{ {} }}",
          d.public_fields //
            | std::views::transform(&meta::field_data::name) //
            | std_c::views::join_with(", "sv) //
            | util::format_range));
}

std::string format_enum_data(const meta::enum_data &d) {
  const std::string_view kind = d.is_scoped //
    ? "enum class"
    : "enum";

  if (d.enumerators.empty())
    return std::format("{} {{}}", kind);

  return std::format("{} {{ {} }}",
    kind,
    d.enumerators //
      | std_c::views::join_with(", "sv) //
      | util::format_range);
}

std::string format_reflectable(const std::set<meta::type_id> &dependencies,
  const std::map<meta::type_id, std::size_t> &index_by_type,
  const meta::reflectable &d) {
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
    format_type_definition(d.definition),
    std::visit(
      []<meta::reflectable_data Data>(const Data &data) {
        if constexpr (std::same_as<meta::record_data, Data>)
          return format_record_data(data);
        else {
          static_assert(std::same_as<meta::enum_data, Data>);
          return format_enum_data(data);
        }
      },
      d.data));
}

std::string format_options(const cli::options &args) {
  // todo: consider printing flags.
  return std::format(
    "\nargs:"
    "\nsource:{}"
    "\nout:{}"
    "\nresource_dir:{}",
    args.source.generic_string(),
    args.out.generic_string(),
    args.resource_dir.generic_string());
}

std::string format_matches(const meta::source_file_context &ctx) {
  return std::format(
    "\nprocessed source: {}"
    "\n\n-- types --------\n{}"
    "\n\n-- includes --------\n{}",
    ctx.source.generic_string(),

    ctx.reflected //
      | std::views::transform(std::bind_front(format_reflectable,
        std::cref(ctx.resolved_as_dependency),
        std::cref(ctx.index_by_type_id))) //
      | std_c::views::join_with("\n\n"sv) //
      | util::format_range,

    ctx.file_dependencies //
      | std::views::transform([](const fs::path &p) { return p.string(); })
      | std_c::views::join_with("\n"sv) //
      | util::format_range);
}

std::string format_success(const pipeline::run_report &out) {
  return //
    std::format(
      "\ninstrumented source: {}"
      "\ngenerated header: {}"
      "\ngenerated deps file: {}"
      "\nreflected types: {}"
      "\n  forward-declarable: {}"
      "\n  nested: {}"
      "\n  indexed: {}"
      "\nfile dependencies: {}",
      out.instrumented_source.generic_string(),
      out.reflection_header.generic_string(),
      out.dependencies.generic_string(),
      out.reflected_type_count,
      out.forward_declarable_count,
      out.nested_type_count,
      out.indexed_type_count,
      out.file_dependency_count)
    + "\ndone.\n";
}

std::string format_scaled_duration(std::chrono::microseconds duration,
  std::chrono::microseconds scale,
  std::string_view unit) {
  const auto whole = duration / scale;
  const auto fractional = duration % scale;
  std::string suffix =
    std::format("{:03}", 1000 * fractional.count() / scale.count());

  while (!suffix.empty() && '0' == suffix.back())
    suffix.pop_back();

  return suffix.empty() ? std::format("{} {}", whole, unit)
                        : std::format("{}.{} {}", whole, suffix, unit);
}

std::string format_duration(std::chrono::microseconds duration) {
  if (std::chrono::milliseconds{1} > duration)
    return std::format("{} microseconds", duration.count());

  if (std::chrono::seconds{1} > duration) {
    return format_scaled_duration(duration,
      std::chrono::milliseconds{1},
      "milliseconds"sv);
  }

  return format_scaled_duration(duration, std::chrono::seconds{1}, "seconds"sv);
}

std::string format_timings(
  const std::map<std::string, std::chrono::microseconds> &timing_by_action) {
  return std::format("\ntimings:\n{}\n",
    timing_by_action //
      | std::views::transform([](const auto &timing) {
          const auto &[action, duration] = timing;
          // Keep the raw microsecond value stable for benchmark parsing; the
          // preceding duration is for human-readable diagnostics.
          return std::format("{}: {} ({} microseconds)",
            action,
            format_duration(duration),
            duration.count());
        })
      | std_c::views::join_with("\n"sv) //
      | util::format_range);
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

meta::template_param map_template_param(const clang::ASTContext &ast,
  const clang::NamedDecl *decl) {
  assert(decl);

  const auto emitted_name = [](const auto &param) -> std::string {
    return std::format("omnirefl_T{}_{}", param.getDepth(), param.getIndex());
  };

  // TemplateParameterList exposes parameters as NamedDecl*. The runtime domain
  // handled here is TemplateTypeParmDecl, NonTypeTemplateParmDecl, or
  // TemplateTemplateParmDecl.
  if (const auto *p = llvm::dyn_cast<clang::TemplateTypeParmDecl>(decl)) {
    return meta::template_type_param{
      .name = emitted_name(*p),
      .is_pack = p->isParameterPack(),
    };
  }

  if (const auto *p = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(decl)) {
    clang::PrintingPolicy pp(ast.getLangOpts());
    pp.FullyQualifiedName = true;
    pp.SuppressScope = false;
    pp.SuppressUnwrittenScope = false;
    pp.SuppressTagKeyword = false;
    pp.PrintAsCanonical = true;

    // ad hoc: Clang's canonical printer renders dependent parameters as
    // `type-parameter-<depth>-<index>`. Map that stable identity to the names
    // used by generated declarations instead of retaining source aliases that
    // are unavailable before the force-included header.
    static const std::regex dependent_parameter{
      R"(\btype-parameter-([0-9]+)-([0-9]+)\b)",
      std::regex_constants::optimize,
    };

    return meta::template_value_param{
      .type = std::regex_replace(p->getType().getAsString(pp),
        dependent_parameter,
        "omnirefl_T$1_$2"),
      .name = emitted_name(*p),
      .is_pack = p->isParameterPack(),
    };
  }

  const auto *p = clang::cast<clang::TemplateTemplateParmDecl>(decl);
  return meta::template_template_param{
    .params = *p->getTemplateParameters() //
      | std::views::transform(
        std::bind_front(map_template_param, std::cref(ast)))
      | std::ranges::to<std::vector>(),
    .name = emitted_name(*p),
    .is_pack = p->isParameterPack(),
  };
}

auto meta::template_data::from_decl(const clang::ASTContext &ast,
  const clang::ClassTemplateSpecializationDecl *spec) -> template_data {
  assert(spec);

  const clang::ClassTemplateDecl *primary = spec->getSpecializedTemplate();
  return meta::template_data{
    .support = std::invoke([spec] {
      if (llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(spec) //
        || spec->getSpecializedTemplateOrPartial()
          .dyn_cast<clang::ClassTemplatePartialSpecializationDecl *>())
        return partial_specialization;

      if (clang::TSK_ExplicitSpecialization == spec->getSpecializationKind())
        return explicit_specialization;

      return supported;
    }),
    .primary_name = primary->getNameAsString(),
    .params = *primary->getTemplateParameters() //
      | std::views::transform(
        std::bind_front(map_template_param, std::cref(ast)))
      | std::ranges::to<std::vector>(),
  };
}

meta::nm_qual_type meta::nm_qual_type::from_decl(
  const clang::NamedDecl *decl) noexcept {
  // refactorme: function bodies are also captured as enclosing_records. I think
  // capturing them for rendering in diagnostics is somewhat useful, but I'd
  // need to introduce a sum type like {namespace|struct|function}.
  auto [namespaces, enclosing_records] =
    std::invoke([&decl_ctx = *decl->getDeclContext()] {
      std::pair result{
        std::vector<meta::namespace_component>{},
        std::vector<std::string>{},
      };

      auto &&[namespaces, enclosing_records] = result;

      const clang::DeclContext *dc = &decl_ctx;
      while (!llvm::isa<clang::TranslationUnitDecl>(dc)) {
        if (const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(dc)) {
          if (ns->isAnonymousNamespace()) {
            namespaces.push_back({.name = ""});
          } else if (ns->isStdNamespace() && ns->getName().starts_with("__")) {
            // fixme:
            // ad hoc: skip implementation-detail namespaces inside std (e.g.
            // std::__1) do nothing
          } else {
            namespaces.push_back({
              .name = ns->getName().str(),
              .is_inline = ns->isInline(),
            });
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
    .name = std::invoke([decl] -> std::optional<std::string> {
      const clang::IdentifierInfo *id = decl->getIdentifier();
      if (!id)
        return std::nullopt;

      std::string name =
        // fixme:
        // ad hoc: for template specializations use the printing
        // policy to render <template args> properly.
        !llvm::isa<clang::ClassTemplateSpecializationDecl>(decl)
        ? id->getName().str()
        : std::invoke([tag = clang::cast<clang::TagDecl>(decl)] {
            return clang::QualType(
              tag->getASTContext().getCanonicalTagType(tag))
              .getAsString(std::invoke([] {
                clang::PrintingPolicy p{{}};

                p.SuppressTagKeyword = true; //< no 'struct', 'class' or 'enum'
                p.SuppressScope = false; //< namespaces or enclosing records
                                         // for <template args>
                p.PrintAsCanonical = true;

                return p;
              }));
          });

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

namespace meta::impl {

struct named_type_source {
  const clang::NamedDecl *declaration;
  clang::NestedNameSpecifierLoc qualifier;
  clang::SourceLocation name;
};

std::optional<named_type_source> named_type_source_from(clang::TypeLoc type) {
  if (const clang::QualifiedTypeLoc qualified =
        type.getAs<clang::QualifiedTypeLoc>()) {
    return named_type_source_from(qualified.getUnqualifiedLoc());
  }

  if (const clang::PointerTypeLoc pointer =
        type.getAs<clang::PointerTypeLoc>()) {
    return named_type_source_from(pointer.getPointeeLoc());
  }

  if (const clang::ReferenceTypeLoc reference =
        type.getAs<clang::ReferenceTypeLoc>()) {
    return named_type_source_from(reference.getPointeeLoc());
  }

  if (const clang::ArrayTypeLoc array = type.getAs<clang::ArrayTypeLoc>())
    return named_type_source_from(array.getElementLoc());

  if (const clang::TemplateSpecializationTypeLoc specialization =
        type.getAs<clang::TemplateSpecializationTypeLoc>()) {
    const clang::TemplateDecl *const template_decl =
      specialization.getTypePtr()->getTemplateName().getAsTemplateDecl();

    if (!template_decl || !template_decl->getTemplatedDecl())
      return std::nullopt;

    return named_type_source{
      .declaration = template_decl->getTemplatedDecl(),
      .qualifier = specialization.getQualifierLoc(),
      .name = specialization.getTemplateNameLoc(),
    };
  }

  if (const clang::TagTypeLoc tag = type.getAsAdjusted<clang::TagTypeLoc>()) {
    return named_type_source{
      .declaration = tag.getDecl(),
      .qualifier = tag.getQualifierLoc(),
      .name = tag.getNameLoc(),
    };
  }

  if (const clang::TypedefTypeLoc alias =
        type.getAsAdjusted<clang::TypedefTypeLoc>()) {
    return named_type_source{
      .declaration = alias.getDecl(),
      .qualifier = alias.getQualifierLoc(),
      .name = alias.getNameLoc(),
    };
  }

  if (const clang::UsingTypeLoc alias =
        type.getAsAdjusted<clang::UsingTypeLoc>()) {
    return named_type_source{
      .declaration = alias.getDecl(),
      .qualifier = alias.getQualifierLoc(),
      .name = alias.getNameLoc(),
    };
  }

  return std::nullopt;
}

clang::SourceLocation namespace_free_begin(const named_type_source &source) {
  if (!source.qualifier
    || clang::NestedNameSpecifier::Kind::Type
      != source.qualifier.getNestedNameSpecifier().getKind()) {
    return source.name;
  }

  return named_type_source_from(source.qualifier.getAsTypeLoc())
    .transform(namespace_free_begin)
    .value_or(source.name);
}

std::string source_text_from(const clang::ASTContext &ast,
  clang::SourceRange range) {
  return clang::Lexer::getSourceText( //
    clang::CharSourceRange::getTokenRange(range),
    ast.getSourceManager(),
    ast.getLangOpts())
    .str();
}

std::string type_name_from_decl(const clang::NamedDecl *decl) {
  assert(decl);

  return meta::format_type_name(meta::nm_qual_type::from_decl(decl));
}

// Namespaces and enclosing records use the same `::` spelling. Clang's
// qualifier kind distinguishes `ns::outer::value`, which becomes
// `outer::value`, from the enclosing-record portion that must be preserved.
std::string field_type_name_from(const clang::ASTContext &ast,
  clang::TypeLoc type) {
  if (const clang::QualifiedTypeLoc qualified =
        type.getAs<clang::QualifiedTypeLoc>()) {
    const std::string qualifiers =
      type.getType().getLocalQualifiers().getAsString(ast.getPrintingPolicy());

    const clang::UnqualTypeLoc unqualified = qualified.getUnqualifiedLoc();
    const std::string type_name = field_type_name_from(ast, unqualified);

    if (qualifiers.empty())
      return type_name;

    if (unqualified.getAs<clang::PointerTypeLoc>())
      return std::format("{} {}", type_name, qualifiers);

    return std::format("{} {}", qualifiers, type_name);
  }

  if (const clang::PointerTypeLoc pointer =
        type.getAs<clang::PointerTypeLoc>()) {
    const std::string pointee =
      field_type_name_from(ast, pointer.getPointeeLoc());

    return std::format("{}{}", pointee, pointee.ends_with('*') ? "*" : " *");
  }

  if (const clang::LValueReferenceTypeLoc reference =
        type.getAs<clang::LValueReferenceTypeLoc>()) {
    return std::format("{} &",
      field_type_name_from(ast, reference.getPointeeLoc()));
  }

  if (const clang::RValueReferenceTypeLoc reference =
        type.getAs<clang::RValueReferenceTypeLoc>()) {
    return std::format("{} &&",
      field_type_name_from(ast, reference.getPointeeLoc()));
  }

  if (const clang::ArrayTypeLoc array = type.getAs<clang::ArrayTypeLoc>()) {
    std::string extents;
    clang::TypeLoc element = array;
    while (const clang::ArrayTypeLoc dimension =
             element.getAs<clang::ArrayTypeLoc>()) {
      extents += source_text_from(ast, dimension.getBracketsRange());
      element = dimension.getElementLoc();
    }

    return field_type_name_from(ast, element) + extents;
  }

  const std::optional source = named_type_source_from(type);
  if (!source)
    return source_text_from(ast, type.getSourceRange());

  if (source->qualifier
    && clang::NestedNameSpecifier::Kind::Type
      == source->qualifier.getNestedNameSpecifier().getKind()) {
    return source_text_from(ast,
      {namespace_free_begin(*source), type.getEndLoc()});
  }

  const std::string type_name = type_name_from_decl(source->declaration);
  if (const clang::TemplateSpecializationTypeLoc specialization =
        type.getAs<clang::TemplateSpecializationTypeLoc>()) {
    return type_name
      + source_text_from(ast,
        {specialization.getLAngleLoc(), specialization.getRAngleLoc()});
  }

  return type_name;
}

std::string field_type_name(const clang::ASTContext &ast,
  const clang::FieldDecl *field) {
  assert(field);

  const clang::TypeSourceInfo *type_source_info = field->getTypeSourceInfo();
  if (!type_source_info)
    return field->getType().getAsString(ast.getPrintingPolicy());

  return field_type_name_from(ast,
    type_source_info->getTypeLoc().getUnqualifiedLoc());
}

} // namespace meta::impl

meta::field_data meta::field_data::from_decl(const clang::ASTContext &ast,
  const clang::FieldDecl *d) {
  assert(d);

  return {
    .name = std::string(fn::as<std::string_view>(d->getName())),
    .type_name = meta::impl::field_type_name(ast, d),
    .qualified_type_name = std::invoke([&ast, d] -> std::string {
      clang::PrintingPolicy policy = ast.getPrintingPolicy();
      policy.FullyQualifiedName = true;

      const clang::TypeSourceInfo *source = d->getTypeSourceInfo();
      const clang::QualType type = source ? source->getType() : d->getType();

      return type.getUnqualifiedType().getAsString(policy);
    }),

    .annotation = annotation_from_decl(ast, d),
    // ad hoc: packed fields cannot always bind to references. Infer safe
    // access from the record layout and copy fields whose address may be
    // misaligned.
    .access = std::invoke([&ast, d] -> value_access {
      if (d->isBitField())
        return value_access::copy;

      if (d->getType()->isReferenceType())
        return value_access::reference;

      const clang::CharUnits field_alignment =
        ast.getTypeAlignInChars(d->getType());

      if (field_alignment.isOne()
        || (field_alignment
            <= ast.getTypeAlignInChars(ast.getCanonicalTagType(d->getParent()))
          && ast.toCharUnitsFromBits(ast.getFieldOffset(d))
            .isMultipleOf(field_alignment))) {
        return value_access::reference;
      }

      return d->getType()->isArrayType() //
        ? value_access::misaligned_array
        : value_access::copy;
    }),

    .is_volatile = d->getType().isVolatileQualified(),
    .is_deprecated = d->hasAttr<clang::DeprecatedAttr>(),
    .qualified = d->isMutable()
      ? as_mutable
      : (d->getType().isConstQualified() ? as_const : none),
  };
}

meta::type_definition resolve_definition(const clang::ASTContext &ast,
  const clang::TagDecl *td) noexcept {
  const clang::SourceManager &sm = ast.getSourceManager();
  const clang::DeclContext &decl_ctx = *td->getDeclContext();

  // refactorme: ugleee
  using td_flags = decltype(meta::type_definition::definition_flags);
  const td_flags td_local = decl_ctx.isFunctionOrMethod() //
    ? meta::type_definition::local
    : meta::type_definition::none;

  const td_flags td_non_public =
    clang::AccessSpecifier::AS_private == td->getAccess()
      || clang::AccessSpecifier::AS_protected == td->getAccess()
    ? meta::type_definition::non_public
    : meta::type_definition::none;

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
util::viewable_range_of<const clang::TypedefNameDecl *> auto
  member_aliases_view(const clang::CXXRecordDecl &rd) {
  const auto is_supported_public_alias = //
    [](const clang::TypedefNameDecl *d) {
      return clang::AccessSpecifier::AS_public == d->getAccess()
        && std::ranges::any_of(meta::k_supported_member_aliases,
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
    | std::views::filter(is_supported_public_alias);
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
    | std::views::transform(&clang::Type::getAsCXXRecordDecl)
    | std::views::filter([](const auto *base) { return nullptr != base; });
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

struct collected_dependencies {
  std::vector<const clang::TagType *> types;
  std::map<meta::type_id, std::set<meta::type_id>> dependencies_by_type;
  std::map<meta::type_id, meta::type_access_path> public_access_path_by_type;
  std::set<meta::type_id> skipped_virtual_base_dependencies;
};

collected_dependencies recursively_collect_dependency_types(
  const diagnostics &log,
  const clang::ASTContext &ast,
  const std::set<meta::type_id> &resolved_concrete_types,
  const clang::CXXRecordDecl &root) {
  // todo: monadic return.
  const auto template_specialization_types = //
    [](const util::viewable_range_of<clang::TemplateArgument> auto &args)
    -> util::viewable_range_of<const clang::Type *> auto {
    return args //
      | std::views::transform(&clang::TemplateArgument::getAsType)
      | std::views::transform(&clang::QualType::getTypePtr);
  };

  struct pending_record {
    const clang::TagType *parent;
    const clang::CXXRecordDecl *record;
    std::optional<meta::type_access_path> public_access_path;
    bool is_root;
  };

  const clang::TagType *root_type =
    meta::map_decl_to_canonical_type(ast, &root);

  std::set<const clang::TagType *> collected;
  std::map<meta::type_id, std::set<meta::type_id>> dependencies_by_type;
  std::map<meta::type_id, meta::type_access_path> public_access_path_by_type;
  std::set<meta::type_id> skipped_virtual_base_dependencies;
  std::set<std::pair<const clang::TagType *, const clang::CXXRecordDecl *>>
    visited;

  // Only CXXRecordDecl may have dependencies.
  std::stack<pending_record> to_visit;
  to_visit.push({
    .parent = root_type,
    .record = &root,
    .public_access_path = meta::type_access_path{},
    .is_root = true,
  });

  const auto concrete_id = [&ast](const clang::TagType *type) {
    return meta::canonical_type_id(ast, type);
  };

  const auto connect = //
    [&concrete_id, &dependencies_by_type](const clang::TagType *parent,
      const clang::TagType *dependency) {
      dependencies_by_type[concrete_id(parent)].emplace(
        concrete_id(dependency));
    };

  // refactorme: consider extracting reusable type-support classification. Most
  // checks that reject reflected_call arguments also apply while resolving
  // dependencies, where unsupported types are skipped instead of diagnosed as
  // usage errors.
  const auto enqueue = //
    [&ast,
      &collected,
      &connect,
      &public_access_path_by_type,
      &resolved_concrete_types,
      &skipped_virtual_base_dependencies,
      &to_visit](const clang::TagType *parent,
      const clang::Type *type,
      std::optional<meta::type_access_path> public_access_path) {
      const clang::TagType *dependency = type->getAs<clang::TagType>();

      if (!dependency)
        return;

      const auto *record = dependency->getAsCXXRecordDecl();

      if (dependency->getDecl()->isInStdNamespace()
        || (record && meta::is_compound_dependency_route(record))) {
        if (record) {
          to_visit.push({
            .parent = parent,
            .record = record,
            .public_access_path = std::move(public_access_path),
            .is_root = false,
          });
        }

        return;
      }

      const meta::type_id id = meta::canonical_type_id(ast, dependency);

      if (record && meta::has_virtual_bases(*record)) {
        skipped_virtual_base_dependencies.emplace(id);

        return;
      }

      connect(parent, dependency);

      if (public_access_path && !public_access_path->steps.empty()) {
        public_access_path_by_type.try_emplace(id, *public_access_path);
      }

      if (resolved_concrete_types.contains(id))
        return;

      collected.emplace(dependency);

      if (record) {
        to_visit.push({
          .parent = parent,
          .record = record,
          .public_access_path = std::move(public_access_path),
          .is_root = false,
        });
      }
    };

  while (!to_visit.empty()) {
    const pending_record pending = to_visit.top();
    const clang::CXXRecordDecl *cur_decl = pending.record;
    const clang::CXXRecordDecl *cur_definition = cur_decl->getDefinition();

    to_visit.pop();

    if (!cur_definition) {
      log(log_level::info, [cur_decl] {
        return std::format(
          "\nincomplete dependency type skipped: '{}'. "
          "Include the definition if this dependency should be reflected.",
          cur_decl->getQualifiedNameAsString());
      });

      continue;
    }

    cur_decl = cur_definition;
    const clang::TagType *cur_type =
      meta::map_decl_to_canonical_type(ast, cur_decl);

    const bool in_std = cur_decl->isInStdNamespace();
    const bool is_compound_dependency =
      meta::is_compound_dependency_route(cur_decl);

    const clang::TagType *dependency_parent =
      in_std || is_compound_dependency ? pending.parent : cur_type;

    if (!pending.is_root && !in_std && !is_compound_dependency) {
      connect(pending.parent, cur_type);

      if (resolved_concrete_types.contains(concrete_id(cur_type)))
        continue;

      collected.emplace(cur_type);
    }

    if (!visited.emplace(dependency_parent, cur_decl).second)
      continue;

    for (const clang::TypedefNameDecl *alias : member_aliases_view(*cur_decl)) {
      std::optional public_access_path = pending.public_access_path;

      if (public_access_path) {
        public_access_path->steps.emplace_back(
          meta::type_access_path::member_type{
            .name = alias->getNameAsString(),
          });
      }

      enqueue(dependency_parent,
        ast.getCanonicalType(alias->getUnderlyingType().getUnqualifiedType())
          .getTypePtr(),
        std::move(public_access_path));
    }

    // refactorme: clean up
    // Dependency route only:
    //   struct r { std::tuple<a, b> v; };
    //   struct r { variant<a, b> v; };
    // This does not make compound inputs reflected-call arguments:
    //   reflected_call(f, std::tuple<a, b>{}); //< unsupported
    // A visitor receives meta_t<T> or binding_t<T>; the argument type itself
    // must be reflected, which is not promised for compound dependency types.
    if (is_compound_dependency) {
      const auto raw_args =
        clang::cast<clang::ClassTemplateSpecializationDecl>(cur_decl)
          ->getTemplateArgs()
          .asArray();

      const llvm::ArrayRef<clang::TemplateArgument> arg_list =
        std::invoke([raw_args] -> llvm::ArrayRef<clang::TemplateArgument> {
          return 1 == raw_args.size()
              && clang::TemplateArgument::Pack == raw_args.front().getKind()
            ? raw_args.front().getPackAsArray()
            : raw_args;
        });

      if (std::ranges::any_of(arg_list
              | std::views::transform(&clang::TemplateArgument::getKind),
            std::bind_front(std::not_equal_to{},
              clang::TemplateArgument::Type))) {
        continue;
      }

      for (const auto &[index, type] :
        template_specialization_types(arg_list) | std_c::views::enumerate) {
        std::optional public_access_path = pending.public_access_path;

        if (public_access_path) {
          public_access_path->steps.emplace_back(
            meta::type_access_path::template_arg{
              .index = static_cast<std::size_t>(index),
            });
        }

        enqueue(dependency_parent, type, std::move(public_access_path));
      }

      // not collecting fields or bases.
      continue;
    }

    // Standard-library wrappers may expose dependency protocol aliases above,
    // but unsupported wrappers are not themselves reflectable records. Do not
    // traverse their bases/fields: some declarations visible through bundled
    // headers have no definition data, and Clang's `bases()` requires it.
    if (in_std)
      continue;

    // bases
    for (const clang::CXXRecordDecl *base : public_bases_view(cur_decl)) {
      if (base->isInStdNamespace()) {
        log(log_level::warning, [base, cur_decl] {
          return std::format(
            "\nstandard-library public base '{}' of '{}' ignored. "
            "Inherited fields from this base will not be reflected.",
            base->getQualifiedNameAsString(),
            cur_decl->getQualifiedNameAsString());
        });

        continue;
      }

      enqueue(dependency_parent,
        ast.getCanonicalTagType(base).getTypePtr(),
        std::nullopt);
    }

    // public fields
    for (const clang::FieldDecl *field : public_fields_view(cur_decl)) {
      std::optional public_access_path = pending.public_access_path;

      if (public_access_path) {
        public_access_path->steps.emplace_back(meta::type_access_path::field{
          .name = field->getNameAsString(),
        });
      }

      enqueue(dependency_parent,
        field->getType().getTypePtr(),
        std::move(public_access_path));
    }
  }

  collected.erase(root_type);
  return {
    .types = {collected.begin(), collected.end()},
    .dependencies_by_type = std::move(dependencies_by_type),
    .public_access_path_by_type = std::move(public_access_path_by_type),
    .skipped_virtual_base_dependencies =
      std::move(skipped_virtual_base_dependencies),
  };
}

auto meta::record_data::from_type(const clang::ASTContext &ast,
  const clang::RecordType *t) -> record_data {
  assert(t);

  const clang::RecordDecl &r_decl = *t->getDecl();
  const clang::CXXRecordDecl *cxx_decl = t->getAsCXXRecordDecl();

  return {
    .public_bases = cxx_decl ? public_bases_view(cxx_decl)
        | std::views::filter([](const clang::CXXRecordDecl *base) {
            return !base->isInStdNamespace();
          })
        | std::views::transform([&ast](const clang::CXXRecordDecl *rd) {
            const auto *type = clang::cast<clang::TagType>(
              ast.getCanonicalTagType(rd).getTypePtr());

            return meta::reflectable_reference{
              .id = meta::reflectable_type_id(ast, type),
              .concrete_id = meta::canonical_type_id(ast, type),
            };
          })
        | std::ranges::to<std::vector>()
                             : std::vector<meta::reflectable_reference>{},

    .public_fields = public_fields_view(&r_decl)
      | std::views::transform(
        std::bind_front(meta::field_data::from_decl, std::cref(ast)))
      | std::ranges::to<std::vector>(),

    .type = r_decl.isStruct() //
      ? meta::record_data::is_struct
      : r_decl.isClass() //
        ? meta::record_data::is_class
        : meta::record_data::is_union,

    .has_template_parent =
      cxx_decl && has_template_record_parent(cxx_decl->getDeclContext()),

    .template_info =
      fn::maybe(llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(&r_decl))
        .transform(
          std::bind_front(meta::template_data::from_decl, std::cref(ast))),
  };
}

auto meta::enum_data::from_type(const clang::EnumType *t) -> enum_data {
  assert(t);

  const clang::EnumDecl &ed = *t->getDecl();
  util::viewable_range_of<std::string> auto names = //
    ed.enumerators() //
    | std::views::transform(&clang::EnumDecl::getName)
    | std::views::transform(fn::as<std::string_view>);

  // ad hoc: `underlying_type` is emitted into a private, target-specific
  // generated header which, as of this writing, is not intended for
  // distribution. Using the canonical target spelling is assumed safe and
  // avoids aliases whose declarations appear only after the header is
  // force-included.
  return {
    .is_scoped = ed.isScoped(),
    .is_fixed = ed.isFixed(),
    .underlying_type = ed.isFixed() //
      ? ed.getIntegerType().getCanonicalType().getAsString()
      : "",
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
    .id = meta::reflectable_type_id(ast, t),
    .concrete_id = meta::canonical_type_id(ast, t),
    .public_access_path = {},
    .annotation = std::invoke([&ast, t] {
      const auto *spec =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(t->getDecl());

      return meta::annotation_from_decl(ast,
        spec ? static_cast<const clang::Decl *>(spec->getSpecializedTemplate())
             : static_cast<const clang::Decl *>(t->getDecl()));
    }),
    .data = clang::isa<clang::EnumType>(t)
      ? record_or_enum(
          meta::enum_data::from_type(clang::cast<clang::EnumType>(t)))
      : record_or_enum(
          meta::record_data::from_type(ast, clang::cast<clang::RecordType>(t))),
    .definition = resolve_definition(ast, t->getDecl()),
  };
}

auto meta::resolve_reflected_type(const diagnostics &log,
  clang::QualType template_arg,
  const clang::ASTContext &ast,
  const std::set<type_id> &resolved_concrete_types)
  -> std::expected<reflected_type_info, std::string> {
  const auto match_record_type = //
    [&ast](const std::string &id, const clang::RecordType *t)
    -> std::variant<reflectable, non_reflectable, already_reflected> {
    if (t->getDecl()->isInStdNamespace())
      return non_reflectable{
        .id = id,
        .concrete_id = canonical_type_id(ast, t),
      };

    return match_reflectable_type(ast, t);
  };

  while (template_arg->isReferenceType())
    template_arg = template_arg->getPointeeType();

  const clang::Type &template_arg_type =
    *ast.getCanonicalType(template_arg.getUnqualifiedType()).getTypePtr();

  // todo: C-arrays, pointers?

  // not a struct|class|union|enum
  if (!clang::isa<clang::TagType>(template_arg_type)) {
    const type_id type = canonical_type_id(ast, &template_arg_type);

    return reflected_type_info{
      .type =
        non_reflectable{
          .id = type,
          .concrete_id = type,
        },
      .dependencies = {},
      .dependencies_by_type = {},
      .public_access_path_by_type = {},
    };
  }

  const auto *tag_type = clang::cast<clang::TagType>(&template_arg_type);
  const meta::type_id id = reflectable_type_id(ast, tag_type);
  const meta::type_id concrete_id = canonical_type_id(ast, tag_type);

  if (resolved_concrete_types.contains(concrete_id)) {
    return reflected_type_info{
      .type =
        already_reflected{
          .id = id,
          .concrete_id = concrete_id,
        },
      .dependencies = {}, //< refactorme: dependencies here make no sense
      .dependencies_by_type = {},
      .public_access_path_by_type = {},
    };
  }

  if (clang::isa<clang::EnumType>(template_arg_type)) {
    const auto *enum_type = clang::cast<clang::EnumType>(&template_arg_type);
    return reflected_type_info{
      .type =
        reflectable{
          .id = id,
          .concrete_id = concrete_id,
          .public_access_path = {},
          .annotation = annotation_from_decl(ast, enum_type->getDecl()),
          .data = enum_data::from_type(enum_type),
          .definition = resolve_definition(ast, enum_type->getDecl()),
        },
      .dependencies = {},
      .dependencies_by_type = {},
      .public_access_path_by_type = {},
    };
  }

  const auto *record_type = clang::cast<clang::RecordType>(&template_arg_type);
  const clang::RecordDecl *record_decl = record_type->getDecl();
  if (llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(record_decl)) {
    return std::unexpected(
      std::format("unsupported partial record template specialization '{}' "
                  "reached resolution after reflected_call argument validation",
        id));
  }

  if (const auto *spec =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record_decl)) {
    if (clang::TSK_ExplicitSpecialization == spec->getSpecializationKind()) {
      return std::unexpected(std::format(
        "unsupported explicit record template specialization '{}' "
        "reached resolution after reflected_call argument validation",
        id));
    }
  }

  if (has_template_record_parent(record_decl->getDeclContext())) {
    return std::unexpected(std::format(
      "unsupported record '{}' nested inside a template record reached "
      "resolution after reflected_call argument validation",
      id));
  }

  collected_dependencies dependencies =
    recursively_collect_dependency_types(log,
      ast,
      resolved_concrete_types,
      *record_type->getAsCXXRecordDecl());

  return reflected_type_info{
    .type = match_record_type(id, record_type),
    .dependencies = dependencies.types
      | std::views::transform(
        [&ast, &dependencies](const clang::TagType *type) {
          meta::reflectable reflected = match_reflectable_type(ast, type);

          if ((meta::type_definition::non_public
                & reflected.definition.definition_flags)
            && dependencies.public_access_path_by_type.contains(
              reflected.concrete_id)) {
            reflected.public_access_path =
              dependencies.public_access_path_by_type.at(reflected.concrete_id);
          }

          return reflected;
        })
      | std::ranges::to<std::vector>(),
    .dependencies_by_type = std::move(dependencies.dependencies_by_type),
    .public_access_path_by_type =
      std::move(dependencies.public_access_path_by_type),
    .skipped_virtual_base_dependencies =
      std::move(dependencies.skipped_virtual_base_dependencies),
  };
}

namespace render::impl {

std::string escaped_string_literal_content(std::string_view s) {
  std::string out;
  llvm::raw_string_ostream{out}.write_escaped(s);
  return out;
}

std::string documentation_method(std::string_view documentation) {
  return std::format(
    "\n  static constexpr auto documentation() noexcept"
    "\n    -> const char(&)[sizeof(\"{0}\")] {{"
    "\n    return \"{0}\";"
    "\n  }}",
    escaped_string_literal_content(documentation));
}

// render `_omni_{root}_as_root` for `root::inner_type` as input
std::string enclosing_root_as_dependent(const meta::nm_qual_type &inner_type) {
  assert(!inner_type.enclosing_records.empty());
  assert(!inner_type.enclosing_records.front().empty()
    && "can't access unnamed root");

  std::vector elems = //
    inner_type.namespaces //
    | std::views::transform(&meta::namespace_component::name)
    | std::views::transform(fn::as<std::string_view>)
    | std::views::filter([](std::string_view s) { return !s.empty(); })
    | std::ranges::to<std::vector>();

  elems.push_back(inner_type.enclosing_records.front());

  return std::format("_omni_{}_as_root",
    elems | std_c::views::join_with("_"sv) | util::format_range);
}

// Makes a qualified type name dependent so it can be used in SFINAE even when
// the root type is only forward-declared.
// Example: `typename _omni_get_outer<Inner>::inner` denotes `outer::inner`.
// Intended to defer nested-name lookup until the enclosing type is complete.
std::string declaration_for_enclosing_root_as_dependent(
  const meta::nm_qual_type &inner_type) {
  std::vector elems = //
    inner_type.namespaces //
    | std::views::transform(&meta::namespace_component::name)
    | std::views::transform(fn::as<std::string_view>)
    | std::views::filter([](std::string_view s) { return !s.empty(); })
    | std::ranges::to<std::vector>();

  elems.push_back(inner_type.enclosing_records.front());

  return std::format(
    "template <typename Inner>"
    "\nusing {} = typename _wrt<{}, Inner>::type;",
    enclosing_root_as_dependent(inner_type),
    elems | std_c::views::join_with("::"sv) | util::format_range);
}

std::string forward_declaration_for_enclosing_root(
  const meta::nm_qual_type &inner_type) {
  assert(!inner_type.enclosing_records.empty());

  return std::format(
    "{}"
    "{};"
    "{}",
    inner_type.namespaces //
      | std::views::filter(
        [](const meta::namespace_component &ns) { return !ns.name.empty(); })
      | std::views::transform([](const meta::namespace_component &ns) {
          return std::format("{}\n", namespace_opening(ns));
        }) //
      | util::format_range,
    std::format("struct {}", inner_type.enclosing_records.front()),
    inner_type.namespaces //
      | std::views::reverse //
      | std::views::filter(
        [](const meta::namespace_component &ns) { return !ns.name.empty(); })
      | std::views::transform([](const meta::namespace_component &ns) {
          return std::format("\n}} // namespace {}", ns.name);
        }) //
      | util::format_range);
}

std::string format_qualified_inner_type_from_root(const std::string &root,
  const meta::nm_qual_type &inner_type) {
  assert(inner_type.name && "unnamed types are not yet supported");
  const std::string leaf = inner_type.name->contains('<')
    ? std::format("template {}", *inner_type.name)
    : *inner_type.name;

  const std::array spans{
    std::span<const std::string>{&root, 1},
    std::span<const std::string>{inner_type.enclosing_records}.subspan(1),
    std::span<const std::string>{&leaf, 1},
  };

  return std::format("{}",
    spans //
      | std::views::join //
      | std_c::views::join_with("::"sv) //
      | util::format_range);
}

// render `typename _omni_{root}_as_root<{param}>::inner`
std::string qualified_inner_type_from_fwd_root(
  const meta::nm_qual_type &inner_type,
  std::string_view param) {
  assert(inner_type.name && "unnamed types are not yet supported");
  const std::string root =
    std::format("{}<{}>", enclosing_root_as_dependent(inner_type), param);

  return std::format("typename {}",
    format_qualified_inner_type_from_root(root, inner_type));
}

template <meta::reflectable_data Data>
std::string reflectable_tag(const Data &d) {
  if constexpr (std::same_as<meta::record_data, Data>) {
    return std::format("{}", d.type);
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
std::string reflectable_head(const meta::nm_qual_type &t,
  std::string_view annotation,
  const Data &d) {
  // to support, I need to store the decl name for `decltype(instance)`
  assert(t.name && "unnamed structs not supported");

  if constexpr (std::same_as<meta::record_data, Data>) {
    if (d.template_info) {
      const std::string template_args = format_template_args(*d.template_info);
      const std::string reflected_type_name =
        format_primary_template_type_name(t, d.template_info->primary_name);

      const std::string reflected_qualified_type_name =
        format_primary_template_qualified_type_name(t,
          d.template_info->primary_name);

      const std::string generated_type_name =
        std::format("{}<{}>", reflected_qualified_type_name, template_args);

      // todo: when/if explicit or partial specializations are implemented,
      // reflected type names and namespace-qualified names must describe the
      // selected specialization. For primary templates, returning the primary
      // template name is intentional.
      return std::format(
        "{4}"
        "\nstruct _reflected<{0} {1}, omnirefl_binding> {{"
        "\n  static_assert(std::is_same<{0} {1}, omnirefl_binding>::value,"
        "\n    \"omnirefl: unexpected types mismatch, try regenerating\");"
        "\n"
        "\n  using type = omnirefl_binding;"
        "\n"
        "\n  static constexpr omni::reflected_entity entity() noexcept {{"
        "\n    return omni::reflected_entity::{2};"
        "\n  }}"
        "\n"
        "\n  static constexpr auto type_name() noexcept"
        "\n    -> const char(&)[sizeof(\"{3}\")] {{"
        "\n    return \"{3}\";"
        "\n  }}"
        "\n"
        "\n  static constexpr auto qualified_type_name() noexcept"
        "\n    -> const char(&)[sizeof(\"{5}\")] {{"
        "\n    return \"{5}\";"
        "\n  }}"
        "{6}",

        // 0
        reflectable_tag(d),

        // 1
        generated_type_name,

        // 2
        reflectable_entity(d),

        // 3
        reflected_type_name,

        // 4
        std::format("template <\n  {},\n  typename omnirefl_binding\n>",
          format_template_params(d.template_info->params, 1)),

        // 5
        reflected_qualified_type_name,

        // 6
        documentation_method(annotation));
    }
  }

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
    "\n  static constexpr auto type_name() noexcept"
    "\n    -> const char(&)[sizeof(\"{3}\")] {{"
    "\n    return \"{3}\";"
    "\n  }}"
    "\n"
    "\n  static constexpr auto qualified_type_name() noexcept"
    "\n    -> const char(&)[sizeof(\"{4}\")] {{"
    "\n    return \"{4}\";"
    "\n  }}"
    "{5}",

    // 0
    reflectable_tag(d),

    // 1
    meta::format(t),

    // 2
    reflectable_entity(d),

    // 3
    meta::format_type_name(t),

    // 4
    meta::format(t),

    // 5
    documentation_method(annotation));
}

std::string format_accessed_type(std::string root,
  const meta::type_access_path &path) {
  return std::ranges::fold_left(path.steps,
    std::move(root),
    [](std::string type, const auto &step) -> std::string {
      return std::visit(
        [&type]<typename Step>(const Step &access) -> std::string {
          if constexpr (std::same_as<meta::type_access_path::field, Step>) {
            return std::format("decltype(std::declval<{}>().{})",
              type,
              access.name);
          } else if constexpr (std::same_as<meta::type_access_path::member_type,
                                 Step>) {
            return type.starts_with("typename ")
              ? std::format("{}::{}", type, access.name)
              : std::format("typename {}::{}", type, access.name);
          } else {
            static_assert(
              std::same_as<meta::type_access_path::template_arg, Step>);
            return std::format("typename _omni_template_arg<{}, {}>::type",
              access.index,
              type);
          }
        },
        step);
    });
}

// SFINAE specialization for inner types of forward-declared types.
template <meta::reflectable_data Data>
std::string inner_reflectable_head(const meta::nm_qual_type &t,
  const meta::type_access_path &public_access_path,
  std::string_view annotation,
  const Data &d) {
  assert(t.name && "unnamed inner structs not supported");

  const std::string access_root_for =
    std::format("{}<T>", enclosing_root_as_dependent(t));

  const std::string matched_type = public_access_path.steps.empty()
    ? std::format("typename {}",
        format_qualified_inner_type_from_root(access_root_for, t))
    : format_accessed_type(access_root_for, public_access_path);

  return std::format(
    "template <typename T>"
    "\nstruct _reflected<T, typename std::enable_if<"
    "\n  std::is_same<T, {0}>::value, T>::type> {{"
    "\n  "
    "\n  using type = T;"
    "\n"
    "\n  static constexpr omni::reflected_entity entity() noexcept {{"
    "\n    return omni::reflected_entity::{1};"
    "\n  }}"
    "\n"
    "\n  static constexpr auto type_name() noexcept"
    "\n    -> const char(&)[sizeof(\"{2}\")] {{"
    "\n    return \"{2}\";"
    "\n  }}"
    "\n"
    "\n  static constexpr auto qualified_type_name() noexcept"
    "\n    -> const char(&)[sizeof(\"{3}\")] {{"
    "\n    return \"{3}\";"
    "\n  }}"
    "{4}",

    matched_type,
    reflectable_entity(d),
    meta::format_type_name(t),
    meta::format(t),
    documentation_method(annotation));
}

template <meta::reflectable_data Data>
std::string indexed_reflectable_head(const meta::nm_qual_type &t,
  std::string_view annotation,
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
    "\n  static constexpr auto type_name() noexcept"
    "\n    -> const char(&)[sizeof(\"{2}\")] {{"
    "\n    return \"{2}\";"
    "\n  }}"
    "\n"
    "\n  static constexpr auto qualified_type_name() noexcept"
    "\n    -> const char(&)[sizeof(\"{3}\")] {{"
    "\n    return \"{3}\";"
    "\n  }}"
    "{4}",

    index,
    reflectable_entity(d),
    meta::format_type_name(t),
    meta::format(t),
    documentation_method(annotation));
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
      | std_c::views::join_with(",\n        "sv) //
      | util::format_range);
}

std::string reflectable_body(const meta::record_data &d) {
  constexpr auto format_field = //
    [](const auto &field_index) {
      const auto &[index, f] = field_index;

      const std::string accessors = std::invoke([&f] {
        // Misaligned packed raw arrays emit metadata without accessors.
        if (meta::field_data::value_access::misaligned_array == f.access)
          return std::string{};

        const bool by_reference =
          meta::field_data::value_access::reference == f.access;

        const std::string value_type = //
          by_reference
          ? std::format("decltype((t.{}))", f.name)
          : std::format("omni::compat::remove_cvref_t<decltype(t.{})>", f.name);

        return std::format(
          "\n\n    // Emitted for reference and copy access."
          "\n    // Lvalue reads preserve owner qualification."
          "\n    template <typename _T>"
          "\n    static constexpr auto value(const _T &t) noexcept"
          "\n      -> {1} {{"
          "\n      return {2};"
          "\n    }}"
          "\n\n    // Emitted for reference and copy access."
          "\n    // Supports assignment through the public writable-field wrapper."
          "\n    template <typename _T, typename V>"
          "\n    static void set_value(_T &t, V &&v) {{"
          "\n      t.{0} = std::forward<V>(v);"
          "\n    }}"
          "{3}",
          f.name,
          value_type,
          std::format("t.{}", f.name),
          by_reference
            ? std::format(
                "\n\n    // Emitted only for fields that can bind to a reference."
                "\n    template <typename _T>"
                "\n    static constexpr auto ref(_T &t) noexcept"
                "\n      -> decltype((t.{0})) {{"
                "\n      return t.{0};"
                "\n    }}",
                f.name)
            : std::string{});
      });

      return std::format(
        "  struct {0}_t {{"
        "\n    using type ="
        "\n      decltype(std::declval<typename _reflected::type &>().{0});"
        "\n"
        "\n    static constexpr std::size_t index() noexcept {{ return {1}; }}"
        "\n"
        "\n    static constexpr auto name() noexcept"
        "\n      -> const char(&)[sizeof(\"{0}\")] {{"
        "\n      return \"{0}\";"
        "\n    }}"
        "\n"
        "\n    static constexpr auto type_name() noexcept"
        "\n      -> const char(&)[sizeof(\"{8}\")] {{"
        "\n      return \"{8}\";"
        "\n    }}"
        "\n"
        "\n    static constexpr auto qualified_type_name() noexcept"
        "\n      -> const char(&)[sizeof(\"{9}\")] {{"
        "\n      return \"{9}\";"
        "\n    }}"
        "{10}"
        "\n"
        "\n    static constexpr bool is_const() noexcept {{ return {2}; }}"
        "\n    static constexpr bool is_mutable() noexcept {{ return {3}; }}"
        "\n    static constexpr bool is_volatile() noexcept {{ return {4}; }}"
        "\n    static constexpr bool has_value_access() noexcept"
        " {{ return {5}; }}"
        "\n    static constexpr bool has_reference_access() noexcept"
        " {{ return {6}; }}"
        "\n    static constexpr bool is_deprecated() noexcept"
        " {{ return {7}; }}"
        "{11}"
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
        f.is_volatile,

        // 5
        meta::field_data::value_access::misaligned_array != f.access,

        // 6
        meta::field_data::value_access::reference == f.access,

        // 7
        f.is_deprecated,

        // 8
        escaped_string_literal_content(f.type_name),

        // 9
        escaped_string_literal_content(f.qualified_type_name),

        // 10
        documentation_method(f.annotation),

        // 11
        accessors);
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
          d.public_fields | std_c::views::enumerate //
            | std::views::transform(format_field)
            | std_c::views::join_with("\n\n"sv) //
            | util::format_range),

    d.public_fields //
      | std::views::transform(&meta::field_data::name)
      | std::views::transform([](std::string_view f) {
          return std::format("omni::field_meta_t<type, {}_t>", f);
        }) //
      | std_c::views::join_with(",\n      "sv) //
      | util::format_range);
}

} // namespace render::impl

template <typename S>
std::string render_reflectable_head(const S &r) {
  using rc = render::reflection_context;
  const auto &type_name = r.type.definition.type_name;

  return std::visit(
    [&](const meta::reflectable_data auto &d) {
      if constexpr (std::same_as<rc::forward_declarable, S>)
        return render::impl::reflectable_head(type_name, r.type.annotation, d);
      else if constexpr (std::same_as<rc::nested_type, S>)
        return render::impl::inner_reflectable_head(type_name,
          r.type.public_access_path,
          r.type.annotation,
          d);
      else {
        static_assert(std::same_as<rc::indexed_type, S>);
        return render::impl::indexed_reflectable_head(type_name,
          r.type.annotation,
          d,
          r.index);
      }
    },
    r.type.data);
}

std::string render_public_bases(
  const std::map<meta::type_id, meta::nm_qual_type> &type_name_by_id,
  const meta::record_data &r) {
  const auto fetch = [&type_name_by_id](
                       const meta::type_id &id) -> const meta::nm_qual_type & {
    return type_name_by_id.at(id);
  };

  const auto format = //
    [](const meta::nm_qual_type &t) {
      if (t.enclosing_records.empty())
        return meta::format(t);
      return render::impl::qualified_inner_type_from_fwd_root(t, "T");
    };

  return std::format("\n  using public_bases_t = std::tuple<{}>;",
    r.public_bases //
      | std::views::transform(&meta::reflectable_reference::id)
      | std::views::transform(fetch) //
      | std::views::transform(format) //
      | std_c::views::join_with(",\n    "sv) //
      | util::format_range);
}

std::string render_reflectable_body(
  const std::map<meta::type_id, meta::nm_qual_type> &type_name_by_id,
  const meta::reflectable &r) {
  return std::visit(
    [&type_name_by_id]<meta::reflectable_data Data>(const Data &d) {
      if constexpr (std::same_as<meta::enum_data, Data>)
        return render::impl::reflectable_body(d);
      else {
        static_assert(std::same_as<meta::record_data, Data>);
        return std::format(
          "{}"
          "\n{}",
          render_public_bases(type_name_by_id, d),
          render::impl::reflectable_body(d));
      }
    },
    r.data);
}

template <typename T>
std::string render_reflectable(
  const std::map<meta::type_id, meta::nm_qual_type> &type_name_by_id,
  const T &d) {
  return std::format(
    "{}"
    "\n{}",
    render_reflectable_head(d),
    render_reflectable_body(type_name_by_id, d.type));
}

auto render::generate_reflection(reflection_context ctx, std::ofstream file)
  -> std::expected<reflection_context, std::string> {
  const bool has_enums = std::invoke(
    [](const auto &...rs) {
      return (std::ranges::any_of(rs, [](const auto &r) {
        return std::holds_alternative<meta::enum_data>(r.type.data);
      }) || ...);
    },
    ctx.fwd_declarables,
    ctx.nested,
    ctx.indexed);

  const std::vector required_includes = //
    std::to_array<std::string_view>({
      "omnirefl/reflection.hpp",
      has_enums ? "array" : "",
    }) //
    | std::views::filter([](std::string_view s) { return !s.empty(); })
    | std::ranges::to<std::vector>();

  std::format_to(std::ostreambuf_iterator<char>(file),
    "// This file was generated by the omnirefl tool on {0:%F %T}."
    "\n// Do not modify the contents of this file."
    "\n"
    "\n#define OMNI_INCLUDED_GENERATED_REFLECTION_HEADER" //< must come before
                                                          // omni headers
    "\n"
    "\n// _wrt means \"with respect to\"."
    "\n// _wrt<U, T>::type == U, but the selected type syntactically depends on T."
    "\n// Generated reflection uses it to name nested types through a forward-declared"
    "\n// root while delaying nested-name lookup until SFINAE substitution. Without"
    "\n// the dependent wrapper, some compilers perform early non-SFINAE lookup for"
    "\n// `U::...` and reject the generated header while U is still incomplete."
    "\ntemplate <typename U, typename>"
    "\nstruct _wrt {{ using type = U; }};"
    "\n"
    "\n{1}"
    "\n"
    "\n// Keeps template-argument lookup dependent for private nested types"
    "\n// reached through tuple/variant fields."
    "\ntemplate <std::size_t I, typename T>"
    "\nstruct _omni_template_arg;"
    "\n"
    "\ntemplate <std::size_t I, template <typename...> class Template,"
    "\n  typename... T>"
    "\nstruct _omni_template_arg<I, Template<T...>>:"
    "\n    std::tuple_element<I, std::tuple<T...>> {{}};"
    "\n"
    "\n#if defined(__GNUC__) && !defined(__clang__) && 16 <= __GNUC__"
    "\n// GCC 16 diagnoses the intentional incomplete-type SFINAE used to"
    "\n// discover nested reflected types. Existing nested metadata tests validate"
    "\n// specialization selection; keep this workaround monitored."
    "\n#  pragma GCC diagnostic push"
    "\n#  pragma GCC diagnostic ignored \"-Wsfinae-incomplete\""
    "\n#endif"
    "\n"
    "\n// -- forward declarable reflected types --------"
    "\n{2}"
    "\n"
    "\n// -- enclosing root forward declarations --------"
    "\n{3}"
    "\n"
    "\n// -- enclosing root type accessors --------"
    "\n{4}"
    "\n"
    "\nnamespace omni {{"
    "\nnamespace detail {{"
    "\nnamespace {{"
    "\n"
    "\n{5}"
    "\n"
    "\n// -- reflected types --------"
    "\n{6}"
    "\n"
    "\n// -- reflected inner types --------"
    "\n{7}"
    "\n"
    "\n// -- generated fallback reflected types --------"
    "\n{8}"
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
    "\n    omni::compat::void_t<typename omni::detail::_reflected<typename std::decay<T>::type>::type>"
    "\n> : std::true_type {{}};"
    "\n"
    "\n#if defined(__GNUC__) && !defined(__clang__) && 16 <= __GNUC__"
    "\n#  pragma GCC diagnostic pop"
    "\n#endif"
    "\n"
    "\n",

    // 0:
    std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()),

    // 1:
    required_includes //
      | std::views::transform(
        [](std::string_view s) { return std::format("#include <{}>", s); })
      | std_c::views::join_with("\n"sv) //
      | util::format_range,

    // 2:
    ctx.fwd_declarables //
      | std::views::transform(&reflection_context::forward_declarable::type)
      | std::views::transform(render::impl::forward_declaration) //
      | std_c::views::join_with("\n\n"sv) //
      | util::format_range,

    // 3:
    ctx.enclosing_roots //
      | std::views::transform(
        render::impl::forward_declaration_for_enclosing_root) //
      | std_c::views::join_with("\n\n"sv) //
      | util::format_range,

    // 4:
    ctx.enclosing_roots //
      | std::views::transform(
        render::impl::declaration_for_enclosing_root_as_dependent) //
      | std_c::views::join_with("\n\n"sv) //
      | util::format_range,

    // 5:
    "",

    // 6:
    ctx.fwd_declarables //
      | std::views::transform(std::bind_front(
        render_reflectable<render::reflection_context::forward_declarable>,
        std::cref(ctx.type_name_by_id)))
      | std_c::views::join_with("\n\n"sv) //
      | util::format_range,

    // 7:
    ctx.nested //
      | std::views::transform(std::bind_front(
        render_reflectable<render::reflection_context::nested_type>,
        std::cref(ctx.type_name_by_id)))
      | std_c::views::join_with("\n\n"sv) //
      | util::format_range,

    // 8:
    ctx.indexed //
      | std::views::transform(std::bind_front(
        render_reflectable<render::reflection_context::indexed_type>,
        std::cref(ctx.type_name_by_id)))
      | std_c::views::join_with("\n\n"sv) //
      | util::format_range);

  // todo: check if file has errors

  return ctx;
}

} // namespace
