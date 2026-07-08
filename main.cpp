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
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Options/Options.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Option/Arg.h>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/Option.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#pragma GCC diagnostic pop

#pragma pop_macro("_FORTIFY_SOURCE")

#include <CLI/CLI.hpp>
#include <CLI/Error.hpp>
#include <CLI/Validators.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <flat_map>
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
#include <span>
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
constexpr std::expected<std::remove_cvref_t<T>, std::remove_cvref_t<E>> expect(
  T &&value,
  E &&error) {
  if (true == static_cast<bool>(value))
    return std::forward<T>(value);

  return std::unexpected(std::forward<E>(error));
}

} // namespace fn

namespace util {

std::expected<std::ofstream, std::string> create_file_for_writing(
  const fs::path &file) {
  if (std::error_code ec; (fs::create_directories(file.parent_path(), ec), ec)) {
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

bool is_subpath(const std::filesystem::path &path,
  const std::filesystem::path &base) {
  return std::mismatch(path.begin(), path.end(), base.begin(), base.end())
           .second
    == base.end();
}

template <typename V, typename CharT = char>
struct format_range_t {
  V rng;
};

struct format_range_adaptor {
  template <std::ranges::viewable_range R>
    requires std::ranges::input_range<std::views::all_t<R>>
  constexpr auto operator()(R &&r) const {
    using V = std::views::all_t<R>;
    return format_range_t<V, char>{std::views::all(std::forward<R>(r))};
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
  static constexpr auto format(util::format_range_t<V, CharT> x,
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

struct app_error {
  int code;
  std::string message;
};

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

std::optional<log_level> parse_log_level(std::string_view name) {
  static const std::flat_map<std::string_view, log_level> log_level_by_name =
    log_levels //
    | std::views::transform([](const auto &level) {
        return std::pair{level.second, level.first};
      }) //
    | std::ranges::to<std::flat_map>();

  if (!log_level_by_name.contains(name))
    return std::nullopt;

  return log_level_by_name.at(name);
}

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
  source_file source;

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
    "mapped_type",
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

  friend bool operator<(const source_location &lhs,
    const source_location &rhs) {
    return std::tie(lhs.source_file, lhs.line, lhs.column)
      < std::tie(rhs.source_file, rhs.line, rhs.column);
  }
};

struct invalid_reflection_query {
  std::string query;
  source_location location;
  std::string note;
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

std::string annotation_from_decl(const clang::ASTContext &ast,
  const clang::Decl *decl);

// as of now - public only
struct field_data {
  std::string name;
  std::string type_name;
  std::string annotation;
  bool is_bitfield;

  enum {
    none = 0,
    as_const,
    as_mutable,
  } qualified;

  static field_data from_decl(const clang::ASTContext &ast,
    const clang::FieldDecl *d) {
    assert(d);
    return {
      .name = std::string(fn::as<std::string_view>(d->getName())),
      .type_name = d->getType().getCanonicalType().getAsString(
        ast.getPrintingPolicy()),
      .annotation = annotation_from_decl(ast, d),
      .is_bitfield = d->isBitField(),
      .qualified = d->isMutable()
        ? as_mutable
        : (d->getType().isConstQualified() ? as_const : none),
    };
  }
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

using template_param = std::variant<
  template_type_param,
  template_value_param,
  template_template_param>;

struct template_template_param {
  std::vector<template_param> params;
  std::string name;
  bool is_pack;
};

struct template_data {
  std::string primary_name;
  std::vector<template_param> params;

  static template_data from_decl(const clang::ASTContext &ast,
    const clang::ClassTemplateSpecializationDecl *spec);
};

struct record_data {
  // only public-nonvirtual bases
  std::vector<type_id> public_bases;

  /// protected and private are not supported now (too complicated)
  std::vector<field_data> public_fields;

  enum type_t {
    is_struct,
    is_class,
    is_union,
  } type;

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
  type_id id;
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
  clang::QualType type,
  const clang::ASTContext &ast,
  const std::set<type_id> &resolved_types);

struct source_file_context {
  source_file sf;

  std::vector<meta::reflectable> reflected{};

  // Reducer state used while resolving reflected calls. Tracks ids already
  // seen during direct and dependency resolution.
  std::set<meta::type_id> resolved_types{};
  std::set<meta::type_id> resolved_as_dependency{};
  std::set<meta::type_id> non_reflectable_types{}; //< for debug or info

  // Render lookup for bases and other references by type id. Stored by value to
  // avoid coupling render lifetime to reflected collection layout.
  std::map<meta::type_id, meta::nm_qual_type> type_name_by_id{};

  std::map<meta::type_id, std::size_t> index_by_type_id{};
  std::map<meta::type_id, std::set<meta::source_location>>
    callsites_by_type_id{};

  std::vector<meta::invalid_reflection_query> invalid_reflection_queries{};

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

  // (experimental) can only be reflected using a generated index
  struct indexed_type {
    meta::reflectable type;
    std::size_t index;
  };

  source_file instrumented_source_file;

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

meta::type_id render_type_id(const clang::ASTContext &ast,
  const clang::Type *type);

meta::type_id render_reflectable_id(const clang::ASTContext &ast,
  const clang::TagType *t);

namespace pipeline {

struct compiler_invocation {
  cli::options args;
  source_file sf;

  std::shared_ptr<clang::CompilerInvocation> ci;
};

std::expected<compiler_invocation, app_error> to_compiler_invocation(
  const diagnostics &log,
  cli::options cli_args) noexcept;

struct parsed_ast {
  cli::options args;
  source_file sf;
  std::unique_ptr<clang::ASTUnit> ast;

  // will be populated during ASTUnit creation
  std::set<fs::path> includes_deps;
};

std::expected<parsed_ast, app_error> to_parsed_ast(const diagnostics &log,
  compiler_invocation ci);

struct reduced_matches {
  cli::options args;
  meta::source_file_context ctx;
};

std::expected<reduced_matches, app_error> reduce_matches(
  const diagnostics &log,
  parsed_ast ast);

struct reflection_context {
  cli::options args;
  render::reflection_context ctx;
};

std::expected<reflection_context, app_error> resolve_reflection_context(
  const diagnostics &log,
  reduced_matches rm);

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
  reflection_context ctx);

} // namespace pipeline

std::string format_options(const cli::options &args);
std::string format_instrumented_source(const cli::options &args);
std::string format_reduced_matches(const pipeline::reduced_matches &rm);
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
struct std::formatter<log_level, char>:
    std::formatter<std::string_view, char> {
  template <typename FormatContext>
  auto format(log_level level, FormatContext &ctx) const {
    static const std::flat_map string_by_enum =
      log_levels | std::ranges::to<std::flat_map>();

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
  log(log_level::info, [&args = *args] {
    return format_instrumented_source(args);
  });

  action_timer measured;

  const auto to_compiler_invocation = std::bind_front(
    measured("compiler invocation", pipeline::to_compiler_invocation),
    std::cref(log));

  const auto to_parsed_ast = std::bind_front(
    measured("parsed ast", pipeline::to_parsed_ast),
    std::cref(log));

  const auto reduce_matches = std::bind_front(
    measured("reduce matches", pipeline::reduce_matches),
    std::cref(log));

  const auto resolve_reflection_context = std::bind_front(
    measured("resolve reflection context",
      pipeline::resolve_reflection_context),
    std::cref(log));

  const auto emit_outputs = std::bind_front(
    measured("emit outputs", pipeline::emit_outputs),
    std::cref(log));

  const auto with_print_reduced_matches =
    fn::with([&log](const pipeline::reduced_matches &rm) {
      log(log_level::debug, [&rm] { return format_reduced_matches(rm); });
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

  return to_compiler_invocation(std::move(*args))
    .and_then(to_parsed_ast)
    .and_then(reduce_matches)
    .transform(with_print_reduced_matches)
    .and_then(resolve_reflection_context)
    .and_then(emit_outputs)
    .transform(report_success)
    .or_else(report_error)
    .value();
}

namespace {

namespace render::impl {

std::string format_location(const meta::source_location &d) {
  return std::format("{}:{}:{}",
    d.source_file.generic_string(),
    d.line,
    d.column);
}

std::string format_nm_qual_type(const meta::nm_qual_type &t) {
  std::vector<std::string> elems;
  elems.reserve(
    t.namespaces.size() + t.enclosing_records.size() + std::size_t{1});
  std::ranges::copy(t.namespaces, std::back_inserter(elems));
  std::ranges::copy(t.enclosing_records, std::back_inserter(elems));
  elems.emplace_back(t.name.value_or("(unnamed)"));

  return std::format("{}",
    elems //
      | std::views::join_with("::"sv) //
      | util::format_range);
}

std::string format_type_name(const meta::nm_qual_type &t) {
  std::vector<std::string> elems;
  elems.reserve(t.enclosing_records.size() + std::size_t{1});
  std::ranges::copy(t.enclosing_records, std::back_inserter(elems));
  elems.emplace_back(t.name.value_or("(unnamed)"));

  return std::format("{}",
    elems //
      | std::views::join_with("::"sv) //
      | util::format_range);
}

std::string format_primary_template_type_name(
  const meta::nm_qual_type &t,
  std::string_view primary_name) {
  std::vector<std::string> elems;
  elems.reserve(t.enclosing_records.size() + std::size_t{1});
  std::ranges::copy(t.enclosing_records, std::back_inserter(elems));
  elems.emplace_back(primary_name);

  return std::format("{}",
    elems //
      | std::views::join_with("::"sv) //
      | util::format_range);
}

std::string format_primary_template_qualified_type_name(
  const meta::nm_qual_type &t,
  std::string_view primary_name) {
  std::vector<std::string> elems;
  elems.reserve(
    t.namespaces.size() + t.enclosing_records.size() + std::size_t{1});
  std::ranges::copy(t.namespaces, std::back_inserter(elems));
  std::ranges::copy(t.enclosing_records, std::back_inserter(elems));
  elems.emplace_back(primary_name);

  return std::format("{}",
    elems //
      | std::views::join_with("::"sv) //
      | util::format_range);
}

std::string format_template_params(const std::vector<meta::template_param> &params,
  std::size_t indent_level) {
  const auto indent = [](std::size_t level) {
    return std::string(level * 2, ' ');
  };

  const auto format_list =
    [&indent](auto self,
      const std::vector<meta::template_param> &params,
      std::size_t level,
      bool with_names) -> std::string {
    std::string out;
    for (std::size_t i = 0; params.size() > i; ++i) {
      if (0 != i)
        out += std::format(",\n{}", indent(level));

      out += std::visit(
        [&indent, self, i, level, with_names]<typename Param>(const Param &p)
          -> std::string {
          const std::string name =
            with_names ? std::format("_T{}", i) : std::string();
          if constexpr (std::same_as<meta::template_type_param, Param>) {
            if (!with_names)
              return std::format("typename{}", p.is_pack ? "..." : "");

            return std::format("typename{}{}", p.is_pack ? "..." : " ", name);
          } else if constexpr (std::same_as<meta::template_value_param, Param>) {
            if (!with_names)
              return std::format("{}{}", p.type, p.is_pack ? "..." : "");

            return std::format("{}{}{}", p.type, p.is_pack ? "..." : " ", name);
          } else {
            static_assert(std::same_as<meta::template_template_param, Param>);
            if (!with_names) {
              return std::format("template <\n{0}{1}\n{2}> class{3}",
                indent(level + 1),
                self(self, p.params, level + 1, false),
                indent(level),
                p.is_pack ? "..." : "");
            }

            return std::format("template <\n{0}{1}\n{2}> class{3}{4}",
              indent(level + 1),
              self(self, p.params, level + 1, false),
              indent(level),
              p.is_pack ? "..." : " ",
              name);
          }
        },
        params[i]);
    }
    return out;
  };

  return format_list(format_list, params, indent_level, true);
}

std::string format_template_args(const meta::template_data &t) {
  return std::format("{}",
    t.params //
      | std::views::enumerate //
      | std::views::transform([](const auto &param_index) {
          return std::format("_T{}", std::get<0>(param_index));
        }) //
      | std::views::join_with(", "sv) //
      | util::format_range);
}

std::string forward_declaration(const meta::reflectable &t) {
  assert(t.definition.type_name.name);

  std::vector<std::string> lines = t.definition.type_name.namespaces
    | std::views::transform(
      [](const std::string &n) { return std::format("namespace {} {{", n); })
    | std::ranges::to<std::vector>();

  const std::string name = std::format("{}{}{}",
    t.definition.type_name.enclosing_records //
      | std::views::transform(fn::as<std::string_view>)
      | std::views::join_with("::"sv) //
      | util::format_range,
    t.definition.type_name.enclosing_records.empty() ? ""sv : "::"sv,
    *t.definition.type_name.name);

  const auto format_data_sum =
    [&name, &t]<typename... U>(const std::variant<U...> &data) -> std::string {
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
              std::format("{}{}{}",
                t.definition.type_name.enclosing_records //
                  | std::views::transform(fn::as<std::string_view>)
                  | std::views::join_with("::"sv) //
                  | util::format_range,
                t.definition.type_name.enclosing_records.empty() ? ""sv : "::"sv,
                struct_data.template_info->primary_name));
          }

          return std::format("{} {};",
            struct_data.type,
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
            render::impl::format_location(r.definition.location),
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
      | util::format_range);
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
            | std::views::join_with(" \\\n  "sv) //
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

app_error processing_error(const source_file &sf, std::string error) {
  return {
    .code = -1,
    .message = std::format("\nFailed to process '{}' with error: {}.",
      sf.path.string(),
      std::move(error)),
  };
}

std::expected<fs::path, app_error> infer_resource_dir(const char *argv0) {
  const fs::path executable = llvm::sys::fs::getMainExecutable(argv0,
    (void *)(intptr_t)&cli::parse);

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
          acc.skipped.push_back(std::format("{}: {}",
            entry.path().generic_string(),
            ec.message()));
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
          acc.skipped.push_back(std::format("{}: {}",
            include.generic_string(),
            ec.message()));
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
            scan.skipped | std::views::join_with("\n"sv)
              | util::format_range)),
  });
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
    "\nUsage: omnirefl -o <reflection.hpp> --source <source.cpp> -- <compiler command...>"
    "\n  WARNING: Uses compile time counters via friend injection, which is not guaranteed"
    "\n           by the C++ Standard to be consistent between compiler implementations.",
  };

  CLI::Option *const resource_dir = //
    app
    .add_option("--resource-dir",
      "Override inferred Clang resource directory.")
    ->check(CLI::ExistingDirectory);

  CLI::Option *const source = //
    app
      .add_option("-s,--source", ".cpp file path to run the tool on.")
    ->type_name("FILE")
    ->check(CLI::ExistingFile)
    ->required();

  CLI::Option *const out = //
    app
    .add_option("-o,--out", "output directory (may contain filename)")
    ->type_name("PATH")
    ->default_val(fs::current_path().generic_string());

  CLI::Option *const no_annotations = //
    app
    .add_flag("--no-annotations",
      "Disable reflected documentation comment annotations.")
    ->configurable(false);

  const std::string log_level_help = std::format("Log level: {}.",
    log_levels //
      | std::views::values
      | std::views::join_with(", "sv) | util::format_range);

  CLI::Option *const cli_log_level = //
    app
    .add_option("--log-level",
      log_level_help)
    ->check(CLI::IsMember(log_levels
      | std::views::values
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
    .source =
      {
        .path =
          fs::absolute(source->as<fs::path>()).lexically_normal(),
        .flags = std::move(flags),
      },
    .out = out->as<fs::path>(),
    .resource_dir = std::move(*parsed_resource_dir),
    .annotations = !no_annotations->as<bool>(),
    .level = *parsed_level,
    .timings = 0 == timings->count()
      ? log_level::silent != *parsed_level
      : timings->as<bool>(),
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
    clang::getDriverOptTable().ParseArgs(
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
  namespace options = clang::options;

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
auto pipeline::to_compiler_invocation(const diagnostics &log,
  cli::options cli_args) noexcept
  -> std::expected<compiler_invocation, app_error> {
  namespace options = clang::options;
  const fs::path &resource_dir = cli_args.resource_dir;
  source_file sf = cli_args.source;

  const auto to_vector_of_raw_pointers =
    [](const std::vector<std::string> &v) -> std::vector<const char *> {
    return v //
      | std::views::transform(&std::string::c_str)
      | std::ranges::to<std::vector>();
  };

  const auto &[source, flags] = sf;

  log(log_level::debug, [&flags] {
    return std::format("\ninput flags:\n{}",
      flags | std::views::join_with("\n"sv) | util::format_range);
  });

  const std::expected normalized_args = parse_driver_args(flags);
  if (!normalized_args)
    return std::unexpected(processing_error(sf, normalized_args.error()));

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

  const std::string driver_triple =
    std::invoke([msvc_used, &normalized_args, &flags] {
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

      if (!msvc_used
        && !normalized_args->getLastArgNoClaim(options::OPT_target)
        && !flags.empty()) {
        // refactorme(cc1_flags): infer the target from GCC-style cross compiler
        // names until compiler-driver mapping is split out of omnirefl.
        const std::string compiler =
          fs::path(flags.front()).filename().string();
        if (compiler.starts_with("x86_64-w64-mingw32-"))
          return std::string{"x86_64-w64-windows-gnu"};
        if (compiler.starts_with("i686-w64-mingw32-"))
          return std::string{"i686-w64-windows-gnu"};
      }

      return triple.str();
    });

  const bool mingw_used = llvm::Triple(driver_triple).isWindowsGNUEnvironment();

  const std::vector<fs::path> mingw_include_paths = std::invoke([&] {
    std::vector<fs::path> paths;
    if (!mingw_used || flags.empty())
      return paths;

    const std::string compiler_name =
      fs::path(flags.front()).filename().string();

    const std::string gcc_machine = std::invoke([&] {
      if (compiler_name.starts_with("x86_64-w64-mingw32-"))
        return std::string{"x86_64-w64-mingw32"};
      if (compiler_name.starts_with("i686-w64-mingw32-"))
        return std::string{"i686-w64-mingw32"};
      return std::string{};
    });

    const fs::path compiler_path = std::invoke([&flags] {
      std::error_code ec;
      return fs::weakly_canonical(flags.front(), ec);
    });
    const fs::path compiler_root = compiler_path.parent_path().parent_path();

    if (fs::is_directory(compiler_root / "include/c++/v1")) {
      std::array ordered = {
        compiler_root / "include/c++/v1",
        compiler_root / "include",
      };

      std::ranges::copy(ordered | std::views::filter([](const fs::path &path) {
        return fs::is_directory(path);
      }), std::back_inserter(paths));

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

    const fs::path gcc_root = fs::path("/usr/lib/gcc") / gcc_machine;
    if (!fs::is_directory(gcc_root))
      return paths;

    std::vector<fs::path> candidates;
    std::ranges::copy(fs::directory_iterator(gcc_root)
        | std::views::filter([](const fs::directory_entry &entry) {
            return entry.is_directory();
          })
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

    std::array ordered = {
      gcc_dir / "include/c++",
      gcc_dir / "include/c++" / gcc_machine,
      gcc_dir / "include/c++/backward",
      gcc_dir / "include",
      gcc_dir / "include-fixed",
    };

    std::ranges::copy(ordered | std::views::filter([](const fs::path &path) {
      return fs::is_directory(path);
    }), std::back_inserter(paths));

    return paths;
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

  // ad hoc: this is a heavy-weight solution just to get proper argument
  // translations (for other compilers' commands) and to resolve system
  // include paths...
  clang::driver::Driver driver(msvc_used ? "clang-cl" : "clang",
    driver_triple,
    *log.clang_engine,
    "omnirefl reflection tool");
  driver.setCheckInputsExist(false);

  const std::unique_ptr<clang::driver::Compilation> compilation(
    driver.BuildCompilation(
      llvm::ArrayRef(to_vector_of_raw_pointers(cc1_driver_args))));

  if (!compilation || compilation->getJobs().empty()) {
    return std::unexpected(processing_error(sf,
      std::format("Failed to build compilatioin for: {}.\n",
        source.generic_string())));
  }

  const auto &compilation_args = compilation->getJobs().begin()->getArguments();

  log(log_level::debug, [&compilation_args] {
    return std::format("\nusing cc1 args:\n{}",
      compilation_args
        | std::views::transform([](llvm::StringRef arg) {
            return arg.str();
          })
        | std::views::join_with("\n"sv) | util::format_range);
  });

  std::shared_ptr clang_invocation =
    std::make_shared<clang::CompilerInvocation>();

  if (!clang::CompilerInvocation::CreateFromArgs(*clang_invocation,
        compilation_args,
        *log.clang_engine)) {
    return std::unexpected(
      processing_error(sf, "Failed to create CompilerInvocation."));
  }

  {
    clang::HeaderSearchOptions &o = clang_invocation->getHeaderSearchOpts();

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
      log(log_level::debug, [&inc] {
        return std::format("\npreprocessor #include \"{}\"", inc);
      });
    });
  }

  // do not need this noise when parsing the AST
  clang_invocation->getDiagnosticOpts().IgnoreWarnings = 1;

  return compiler_invocation{
    .args = std::move(cli_args),
    .sf = std::move(sf),
    .ci = std::move(clang_invocation),
  };
}

std::expected<pipeline::parsed_ast, app_error> pipeline::to_parsed_ast(
  const diagnostics &log,
  compiler_invocation ci) {
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
      processing_error(ci.sf, "Failed to build AST Unit."));

  return parsed_ast{
    .args = std::move(ci.args),
    .sf = std::move(ci.sf),
    .ast = std::move(ast),

    .includes_deps = deps_collector.getDependencies()
      | std::views::transform(fn::as<fs::path>) | std::ranges::to<std::set>(),
  };
}

// refactorme: make match rules immediately visible as `pattern |> reducer`.
// Keep this wrapper until the AST ownership model is settled (lazy vs eager)
// and match chains can be composed into the same reduced context type without
// hiding the concrete rules behind a premature generic abstraction.
std::expected<pipeline::reduced_matches, app_error> pipeline::reduce_matches(
  const diagnostics &log,
  parsed_ast wast) {
  (void)log;
  using namespace clang::ast_matchers;

  const auto source_location_from =
    [](const clang::ASTContext &ast, clang::SourceLocation loc)
    -> std::expected<meta::source_location, std::string> {
    if (!loc.isValid())
      return std::unexpected("invalid source location");

    const clang::SourceManager &sm = ast.getSourceManager();
    const clang::PresumedLoc presumed = sm.getPresumedLoc(loc);
    if (!presumed.isValid())
      return std::unexpected("invalid presumed source location");

    return meta::source_location{
      .source_file = presumed.getFilename(),
      .line = sm.getSpellingLineNumber(loc),
      .column = sm.getSpellingColumnNumber(loc),
    };
  };

  const auto is_omni_frontend_location =
    [](const meta::source_location &loc) {
    const std::string path = loc.source_file.generic_string();
    return std::string::npos != path.find("/include/omnirefl/");
  };

  const auto is_concrete_template_arg =
    [](const clang::TemplateArgument &arg) {
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
      return expr && !expr->isTypeDependent()
        && !expr->isInstantiationDependent()
        && !expr->containsUnexpandedParameterPack();
    }
    case clang::TemplateArgument::Pack:
    case clang::TemplateArgument::Null:
      return false;
    }

    return false;
  };

  const auto instantiation_or_decl_location =
    [](const auto &decl) {
    clang::SourceLocation loc = decl.getPointOfInstantiation();
    if (!loc.isValid())
      loc = decl.getLocation();
    return loc;
  };

  const auto append_invalid_reflection_query =
    [source_location_from,
      is_omni_frontend_location](meta::source_file_context a,
      const clang::ASTContext &ast,
      std::string query,
      clang::SourceLocation loc,
      std::string note) {
    std::expected location = source_location_from(ast, loc);
    if (!location) {
      a.errors.emplace_back(std::format(
        "tool error: invalid reflection query matcher location: {}",
        std::move(location).error()));
      return a;
    }

    if (is_omni_frontend_location(*location))
      return a;

    a.invalid_reflection_queries.push_back({
      .query = std::move(query),
      .location = *std::move(location),
      .note = std::move(note),
    });
    return a;
  };

  meta::source_file_context ctx = ast::reduce_matches(*wast.ast,
    meta::source_file_context{
      .sf = std::move(wast.sf),
      .file_dependencies = std::move(wast.includes_deps),
    },

    ast::rule{
      .pattern = ast::pattern(classTemplateSpecializationDecl,
        isTemplateInstantiation(),
        isDefinition(),
        anyOf(
          hasName("::omni::is_reflected"),
          hasName("::omni::meta_t"),
          hasName("::omni::binding_t"),
          hasName("::omni::field_meta_t"),
          hasName("::omni::field_binding_t"))),
      .reduce =
        [&ast = wast.ast->getASTContext(),
          is_concrete_template_arg,
          instantiation_or_decl_location,
          append_invalid_reflection_query](meta::source_file_context a,
          const clang::ClassTemplateSpecializationDecl &decl) {
          const clang::TemplateArgumentList &args = decl.getTemplateArgs();
          if (!std::ranges::all_of(args.asArray(), is_concrete_template_arg))
            return a;

          const clang::ClassTemplateDecl *tmpl = decl.getSpecializedTemplate();
          const std::string query = tmpl
            ? tmpl->getQualifiedNameAsString()
            : decl.getQualifiedNameAsString();

          return append_invalid_reflection_query(std::move(a),
            ast,
            query,
            instantiation_or_decl_location(decl),
            "reflection query class template instantiated during the tool run");
        },
    },

    ast::rule{
      .pattern = ast::pattern(functionDecl,
        isTemplateInstantiation(),
        isDefinition(),
        hasName("::omni::reflected")),
      .reduce =
        [&ast = wast.ast->getASTContext(),
          instantiation_or_decl_location,
          append_invalid_reflection_query](meta::source_file_context a,
          const clang::FunctionDecl &decl) {
          return append_invalid_reflection_query(std::move(a),
            ast,
            decl.getQualifiedNameAsString(),
            instantiation_or_decl_location(decl),
            "reflection query function instantiated during the tool run");
        },
    },

    ast::rule{
      .pattern = ast::pattern(cxxOperatorCallExpr,
        hasOverloadedOperatorName("()")),
      .reduce =
        [&ast = wast.ast->getASTContext()](meta::source_file_context a,
          const clang::CXXOperatorCallExpr &call) {
          if (call.getNumArgs() <= 2)
            return a;

          const clang::Type *call_object_type =
            ast.getCanonicalType(call.getArg(0)->getType())
              .getUnqualifiedType()
              .getTypePtr();
          const auto *call_object_decl =
            call_object_type->getAsCXXRecordDecl();
          if (!call_object_decl
            || "omni::reflected_call_t"
              != call_object_decl->getQualifiedNameAsString()) {
            return a;
          }

          const clang::SourceManager &sm = ast.getSourceManager();
          const clang::SourceLocation loc = call.getExprLoc();
          const clang::PresumedLoc presumed = sm.getPresumedLoc(loc);
          if (!presumed.isValid()) {
            a.errors.emplace_back(
              "tool error: reflected_call matcher could not resolve a source location");
            return a;
          }

          meta::source_location callsite{
            .source_file = presumed.getFilename(),
            .line = sm.getSpellingLineNumber(loc),
            .column = sm.getSpellingColumnNumber(loc),
          };

          const auto reflected_arg_type =
            [](clang::QualType type) -> clang::QualType {
            while (type->isReferenceType())
              type = type->getPointeeType();

            const auto *record = type->getAs<clang::RecordType>();
            if (!record)
              return type;

            const auto *spec =
              llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                record->getDecl());
            if (!spec
              || "omni::type_t" != spec->getSpecializedTemplate()
                   ->getQualifiedNameAsString()) {
              return type;
            }

            const clang::TemplateArgumentList &args = spec->getTemplateArgs();
            if (1 != args.size()
              || clang::TemplateArgument::Type != args.get(0).getKind()) {
              return type;
            }

            return args.get(0).getAsType();
          };

          for (unsigned i = 2; call.getNumArgs() > i; ++i) {
            std::expected resolved = meta::resolve_reflected_type(
              reflected_arg_type(call.getArg(i)->getType()),
              ast,
              a.resolved_types);

            if (!resolved) {
              a.errors.emplace_back(std::move(resolved).error());
              return a;
            }

            meta::reflected_type_info &info = *resolved;
            const meta::type_id id = std::visit(
              []<typename T>(const T &t) {
                return t.id;
              },
              info.type);

            a.callsites_by_type_id[id].emplace(callsite);

            std::visit(
              [&a]<typename T>(T t) {
                a.resolved_types.emplace(t.id);

                if constexpr (std::same_as<meta::reflectable, T>) {
                  meta::reflectable &r = t;
                  a.type_name_by_id.emplace(r.id, r.definition.type_name);
                  a.reflected.emplace_back(std::move(r));
                } else if constexpr (std::same_as<meta::non_reflectable, T>) {
                  // todo:
                  // Only a limited set of reflected_call arguments is
                  // supported, and it should be reported as error diagnostics.
                } else {
                  static_assert(std::same_as<meta::already_reflected, T>);
                  // todo: log for info?
                }
              },
              std::move(info.type));

            std::ranges::copy(info.dependencies
                | std::views::transform(&meta::reflectable::id),
              std::inserter(a.resolved_types, a.resolved_types.begin()));

            std::ranges::copy(info.dependencies
                | std::views::transform(&meta::reflectable::id),
              std::inserter(a.resolved_as_dependency,
                a.resolved_as_dependency.begin()));

            for (meta::reflectable &dep : info.dependencies) {
              a.type_name_by_id.emplace(dep.id, dep.definition.type_name);
              a.reflected.emplace_back(std::move(dep));
            }
          }

          return a;
        },
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
          const clang::TemplateArgumentList &args = decl.getTemplateArgs();
          if (args.size() <= 1) {
            a.errors.emplace_back(
              "invalid _reflected_indexed_type signature");
            return a;
          }

          const clang::TemplateArgument &type_arg = args.get(0);
          if (clang::TemplateArgument::Type != type_arg.getKind()) {
            a.errors.emplace_back(
              "invalid _reflected_indexed_type signature: non-type first argument");
            return a;
          }

          const clang::TemplateArgument &index_arg = args.get(1);
          if (clang::TemplateArgument::Integral != index_arg.getKind()) {
            a.errors.emplace_back(
              "invalid _reflected_indexed_type signature: non-integral index argument");
            return a;
          }

          const clang::Type &type = *type_arg.getAsType();
          const meta::type_id id = clang::isa<clang::TagType>(type)
            ? render_reflectable_id(ast, clang::cast<clang::TagType>(&type))
            : render_type_id(ast, &type);
          a.index_by_type_id.emplace(
            std::move(id),
            index_arg.getAsIntegral().getExtValue());
          return a;
        },
    });

  return reduced_matches{
    .args = std::move(wast.args),
    .ctx = std::move(ctx),
  };
}

std::expected<pipeline::reflection_context, app_error>
  pipeline::resolve_reflection_context(const diagnostics &log,
    reduced_matches rm) {
  using render_context = render::reflection_context;

  const source_file source = rm.ctx.sf;

  if (!rm.ctx.invalid_reflection_queries.empty()) {
    std::string message =
      "reflection query instantiated during the tool run";
    for (const meta::invalid_reflection_query &q :
      rm.ctx.invalid_reflection_queries) {
      message += std::format("\n  {}:{}:{}: {} ({})",
        q.location.source_file.generic_string(),
        q.location.line,
        q.location.column,
        q.query,
        q.note);
    }
    message +=
      "\nreflection queries are valid only inside deferred reflected scope. "
      "Move the query into a reflected_call visitor body, make the visitor "
      "dependent/deferred, and add an explicit trailing return type when "
      "deduced return type would instantiate the body during the tool run.";

    return std::unexpected(processing_error(source, std::move(message)));
  }

  std::vector<render_context::forward_declarable> fwd;
  std::vector<render_context::nested_type> nested;
  std::vector<render_context::indexed_type> indexed;
  std::vector<meta::reflectable> errors;

  for (meta::reflectable &&t : rm.ctx.reflected | std::views::as_rvalue) {
    const std::optional index = rm.ctx.index_by_type_id.contains(t.id)
      ? std::optional(rm.ctx.index_by_type_id.at(t.id))
      : std::nullopt;

    const bool as_dependency = rm.ctx.resolved_as_dependency.contains(t.id);

    const bool is_nested = !t.definition.type_name.enclosing_records.empty();
    const bool is_unnamed = !t.definition.type_name.name;
    const bool is_public_non_local =
      meta::type_definition::none == t.definition.definition_flags;

    if (is_nested && is_unnamed) {
      // todo: report source location and reflected-call stack. Unnamed nested
      // types need `decltype(std::declval<root>().field)`-style access, which
      // is not implemented yet.
      errors.push_back(std::move(t));
      continue;
    }

    // A generated fallback type that can be forward-declared is promoted to a
    // named specialization. This avoids emitting competing fallback metadata.
    if (std::visit(
          [is_nested,
            is_unnamed,
            is_public_non_local]<meta::reflectable_data Data>(
            const Data &data) {
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
        log(log_level::debug, [index, id = t.id] {
          return std::format(
            "\ngenerated fallback '{}' type '{}' will be rendered as forward-declarable.",
            *index,
            id);
        });
      }

      fwd.push_back({
        .type = std::move(t),
        .index = index,
        .as_dependent = as_dependency,
      });

      continue;
    }

    // todo: fail with diagnostics if the enclosing root type is not reflected.
    // todo: fail with diagnostics if the enclosing root is not
    // public/non-local. As of now this info is not collected/resolved: only
    // names are collected.
    if (is_nested
      // ad hoc to distinguish from fallback-specialized types that has
      // non-empty enclosing_records. Unnamed nested types can be supported,
      // but the distinction should be stronger.
      && !is_unnamed
      && !t.definition.type_name.enclosing_records.front().empty()) {
      if (index) {
        log(log_level::debug, [index, id = t.id] {
          return std::format(
            "\ngenerated fallback '{}' type '{}' will be rendered as nested forward-declarable.",
            *index,
            id);
        });
        log(log_level::debug, [is_nested] {
          return std::format("\nis_nested: {}", is_nested);
        });
        log(log_level::debug,
          [&records = t.definition.type_name.enclosing_records] {
          return std::format("\nenclosing_records: [{}]",
            records | std::views::join_with(", "sv) | util::format_range);
          });
      }

      nested.push_back({
        .type = std::move(t),
        .index = index,
        .as_dependent = as_dependency,
      });

      continue;
    }

    if (index) {
      indexed.push_back({
        .type = std::move(t),
        .index = *index,
      });

      continue;
    }

    // todo: fail with diagnostics explaining why this type cannot be emitted
    // and how to make it reflectable. Typical fixes: make the type
    // forward-declarable, move the enclosing type out of a local scope, or
    // avoid unsupported non-forward-declarable dependency routes.
    errors.push_back(std::move(t));
  }

  // std::ranges::to<std::vector>() can precompute size for filtered ranges,
  // causing another pass over a stateful dedup predicate. Keep this as explicit
  // single-pass mutation.
  std::set<meta::type_id> emitted_forward_declarables;
  std::erase_if(fwd, [&emitted_forward_declarables](const auto &t) {
    return !emitted_forward_declarables.emplace(t.type.id).second;
  });

  const auto missing_base = [&rm](const meta::reflectable &r)
    -> std::optional<meta::type_id> {
    return std::visit(
      [&rm](const meta::reflectable_data auto &data)
        -> std::optional<meta::type_id> {
        if constexpr (std::same_as<meta::record_data,
                        std::remove_cvref_t<decltype(data)>>) {
          for (const meta::type_id &base_id : data.public_bases)
            if (!rm.ctx.type_name_by_id.contains(base_id))
              return base_id;
        }

        return std::nullopt;
      },
      r.data);
  };

  for (const render_context::forward_declarable &t : fwd) {
    if (const std::optional base_id = missing_base(t.type))
      return std::unexpected(processing_error(source,
        std::format("missing reflected public base metadata: '{}' required by '{}'",
          *base_id,
          t.type.id)));
  }

  for (const render_context::nested_type &t : nested) {
    if (const std::optional base_id = missing_base(t.type))
      return std::unexpected(processing_error(source,
        std::format("missing reflected public base metadata: '{}' required by '{}'",
          *base_id,
          t.type.id)));
  }

  for (const render_context::indexed_type &t : indexed) {
    if (const std::optional base_id = missing_base(t.type))
      return std::unexpected(processing_error(source,
        std::format("missing reflected public base metadata: '{}' required by '{}'",
          *base_id,
          t.type.id)));
  }

  // todo: validate that no two classified types emit the same specialization
  // target. This is a tool bug: report both declarations and the generated
  // specialization target so the backend resolution can be fixed.

  if (!errors.empty())
    // todo: list types. "qualified::type declared at: loc"
    return std::unexpected(
      processing_error(source, "non-reflectable types: []"));

  constexpr auto cmp_roots = //
    [](const meta::nm_qual_type &lhs, const meta::nm_qual_type &rhs) -> bool {
    return std::tie(lhs.namespaces, lhs.name)
      < std::tie(rhs.namespaces, rhs.name);
  };

  std::vector enclosing_roots = nested //
    | std::views::transform([](const render_context::nested_type &n) {
        return n.type.definition.type_name;
      })
    | std::ranges::to<std::set>(cmp_roots) //
    | std::views::as_rvalue //
    | std::ranges::to<std::vector>();

  return reflection_context{
    .args = std::move(rm.args),
    .ctx =
      render_context{
        .instrumented_source_file = std::move(rm.ctx.sf),

        .fwd_declarables = std::move(fwd),
        .nested = std::move(nested),
        .indexed = std::move(indexed),
        .enclosing_roots = std::move(enclosing_roots),

        .type_name_by_id = std::move(rm.ctx.type_name_by_id),
        .file_dependencies = std::move(rm.ctx.file_dependencies),
      },
  };
}

std::expected<pipeline::run_report, app_error> pipeline::emit_outputs(
  const diagnostics &log,
  reflection_context rc) {
  const auto reflection_header_path = //
    [&o = rc.args.out](const source_file &instrumented_source) {
      // ad hoc to disable fs exception throwing
      return std::invoke([&o] {
        std::error_code ec;
        return fs::is_directory(o, ec);
      }) //
        ? fs::path(o)
          / std::format("{}_omni_reflection_header.h",
            instrumented_source.path.string())
        : o;
    };

  const fs::path out = reflection_header_path(rc.ctx.instrumented_source_file);

  log(log_level::info, [&out] {
    return std::format("\ncreating reflection header: {}",
      out.generic_string());
  });

  const source_file sf = rc.ctx.instrumented_source_file;
  const std::size_t forward_declarable_count = rc.ctx.fwd_declarables.size();
  const std::size_t nested_type_count = rc.ctx.nested.size();
  const std::size_t indexed_type_count = rc.ctx.indexed.size();
  const std::size_t file_dependency_count = rc.ctx.file_dependencies.size();
  const std::size_t reflected_type_count =
    forward_declarable_count + nested_type_count + indexed_type_count;

  return util::create_file_for_writing(out) //
    .and_then(std::bind_front(render::generate_reflection, std::move(rc.ctx)))
    .and_then([=](render::reflection_context ctx) {
      log(log_level::info, [out] {
        return std::format("\nwriting deps file for: {}", out.generic_string());
      });

      return render::write_dependencies_file(out, ctx.file_dependencies)
        .transform([=](fs::path deps) {
          return run_report{
            .instrumented_source = sf.path,
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
    .transform_error([&sf](std::string error) {
      return processing_error(sf, std::move(error));
    });
}

std::string format_definition_flags(int flags) {
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

std::string format_type_definition(const meta::type_definition &d) {
  return std::format(
    "declared at: {}{}"
    "\n  {}",
    render::impl::format_location(d.location),
    std::invoke([f = format_definition_flags(d.definition_flags)] {
      return f.empty() ? std::string() : std::format("\n  [{}]", f);
    }),

    render::impl::format_nm_qual_type(d.type_name));
}

std::string format_record_data(const meta::record_data &d) {
  if (d.public_fields.empty())
    return std::format("{} {{}}", d.type);

  return std::format("{} {{ {} }}",
    d.type,
    d.public_fields //
      | std::views::transform(&meta::field_data::name) //
      | std::views::join_with(", "sv) //
      | util::format_range);
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
      | std::views::join_with(", "sv) //
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
    source_file::path_as_string(args.source),
    args.out.generic_string(),
    args.resource_dir.generic_string());
}

std::string format_instrumented_source(const cli::options &args) {
  return std::format("\nrunning for file: {}\t\r",
    args.source.path.string());
}

std::string format_reduced_matches(const pipeline::reduced_matches &rm) {
  return std::format(
    "\nprocessed source: {}"
    "\n\n-- types --------\n{}"
    "\n\n-- includes --------\n{}",
    source_file::path_as_string(rm.ctx.sf),

    rm.ctx.reflected //
      | std::views::transform(std::bind_front(format_reflectable,
        std::cref(rm.ctx.resolved_as_dependency),
        std::cref(rm.ctx.index_by_type_id))) //
      | std::views::join_with("\n\n"sv) //
      | util::format_range,

    rm.ctx.file_dependencies //
      | std::views::transform([](const fs::path &p) { return p.string(); })
      | std::views::join_with("\n"sv) //
      | util::format_range);
}

std::string format_success(const pipeline::run_report &out) {
  return std::format(
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
  std::string suffix = std::format("{:03}",
    1000 * fractional.count() / scale.count());

  while (!suffix.empty() && '0' == suffix.back())
    suffix.pop_back();

  return suffix.empty()
    ? std::format("{} {}", whole, unit)
    : std::format("{}.{} {}",
        whole,
        suffix,
        unit);
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
      | std::views::join_with("\n"sv) //
      | util::format_range);
}

} // namespace

namespace {

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

meta::type_id render_primary_template_id(
  const clang::ClassTemplateDecl *template_decl) {
  assert(template_decl);
  // ad hoc: type_id is still a rendered string, not a Clang-provided opaque
  // identity. Prefix primary-template declaration ids so they cannot collide
  // with rendered concrete type ids if full specializations are supported later.
  return std::format("primary-template:{}",
    template_decl->getCanonicalDecl()->getQualifiedNameAsString());
}

meta::type_id render_reflectable_id(const clang::ASTContext &ast,
  const clang::TagType *t) {
  assert(t);

  const auto *template_spec =
    llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
      t->getDecl());
  // todo: replace this ad hoc primary-template collapse with general template
  // specialization reflection metadata. `crtp_base<X>` and `crtp_base<Y>` are
  // treated as the same reflected template shape for now, which is only valid
  // while full and partial specializations are unsupported/rejected.
  return template_spec
    ? render_primary_template_id(template_spec->getSpecializedTemplate())
    : render_type_id(ast, t);
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

std::string meta::annotation_from_decl(const clang::ASTContext &ast,
  const clang::Decl *decl) {
  assert(decl);

  // Raw-comment lookup may be relatively expensive; skip it when annotations
  // were explicitly disabled.
  if (!ast.getLangOpts().CommentOpts.ParseAllComments)
    return "";

  const clang::Decl *canonical = decl->getCanonicalDecl();
  const clang::RawComment *comment =
    ast.getRawCommentForDeclNoCache(canonical);
  if (!comment)
    comment = ast.getRawCommentForDeclNoCache(decl);
  if (!comment)
    return "";

  return comment->getBriefText(ast);
}

meta::template_param map_template_param(const clang::ASTContext &ast,
  const clang::NamedDecl *decl) {
  assert(decl);

  // TemplateParameterList exposes parameters as NamedDecl*. The runtime domain
  // handled here is TemplateTypeParmDecl, NonTypeTemplateParmDecl, or
  // TemplateTemplateParmDecl.
  if (const auto *p = llvm::dyn_cast<clang::TemplateTypeParmDecl>(decl)) {
    return meta::template_type_param{
      .name = p->getName().str(),
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

    return meta::template_value_param{
      .type = p->getType().getAsString(pp),
      .name = p->getName().str(),
      .is_pack = p->isParameterPack(),
    };
  }

  const auto *p = clang::cast<clang::TemplateTemplateParmDecl>(decl);
  return meta::template_template_param{
    .params = *p->getTemplateParameters() //
      | std::views::transform(std::bind_front(map_template_param,
        std::cref(ast)))
      | std::ranges::to<std::vector>(),
    .name = p->getName().str(),
    .is_pack = p->isParameterPack(),
  };
}

auto meta::template_data::from_decl(
  const clang::ASTContext &ast,
  const clang::ClassTemplateSpecializationDecl *spec) -> template_data {
  assert(spec);

  const clang::ClassTemplateDecl *primary = spec->getSpecializedTemplate();
  return meta::template_data{
    .primary_name = primary->getNameAsString(),
    .params = *primary->getTemplateParameters() //
      | std::views::transform(std::bind_front(map_template_param,
        std::cref(ast)))
      | std::ranges::to<std::vector>(),
  };
}

bool has_template_record_parent(const clang::DeclContext *dc) {
  for (const clang::DeclContext *cur = dc; cur; cur = cur->getParent()) {
    const auto *rd = llvm::dyn_cast<clang::CXXRecordDecl>(cur);
    if (rd
      && (llvm::isa<clang::ClassTemplateSpecializationDecl>(rd)
        || llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(rd)))
      return true;
  }

  return false;
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

      std::string name =
        // fixme:
        // ad hoc: for template specializations use the printing
        // policy to render <template args> properly.
        !llvm::isa<clang::ClassTemplateSpecializationDecl>(td)
        ? id->getName().str()
        : clang::QualType(td->getASTContext().getCanonicalTagType(td))
            .getAsString(
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
    | std::views::transform(&clang::Type::getAsCXXRecordDecl)
    | std::views::filter([](const clang::CXXRecordDecl *base) {
        return base && !base->isInStdNamespace();
      });
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

  const auto not_null = //
    [](const auto *t) { return nullptr != t; };

  const auto not_in_std = //
    [](const clang::TagType *t) { return !t->getDecl()->isInStdNamespace(); };

  const auto not_resolved = //
    [&ast, &resolved_types](const clang::TagType *t) {
      return !resolved_types.contains(render_reflectable_id(ast, t));
    };

  std::set<const clang::TagType *> collected;

  // only CXXRecordDecl may have dependencies
  std::stack<const clang::CXXRecordDecl *> to_visit;
  to_visit.push(&root);

  while (!to_visit.empty()) {
    const clang::CXXRecordDecl *cur_decl = to_visit.top();
    const clang::TagType *cur_type =
      meta::map_decl_to_canonical_type(ast, cur_decl);
    const meta::type_id cur_id = render_reflectable_id(ast, cur_type);

    to_visit.pop();

    if (collected.contains(cur_type) || resolved_types.contains(cur_id))
      continue;

    std::ranges::move(member_aliases_view(ast, *cur_decl) //
        | std::views::filter(&clang::Type::isEnumeralType) //
        | std::views::transform(to_tag_type) //
        | std::views::filter(not_null) //
        | std::views::filter(not_in_std) //
        | std::views::filter(not_resolved),
      std::inserter(collected, collected.begin()));

    std::ranges::for_each(member_aliases_view(ast, *cur_decl) //
        | std::views::filter(&clang::Type::isRecordType) //
        | std::views::transform(&clang::Type::getAsCXXRecordDecl),
      [&to_visit](const clang::CXXRecordDecl *rd) { to_visit.push(rd); });

    // refactorme: clean up
    // Dependency route only:
    //   struct r { std::tuple<a, b> v; };
    //   struct r { variant<a, b> v; };
    // This does not mean compound inputs are reflected-call arguments:
    //   reflected_call(f, std::tuple<a, b>{}); //< unsupported
    // A visitor receives meta_t<T> or binding_t<T>; the argument type itself
    // must be reflected, which is not true for tuple/variant wrapper types.
    if (is_supported_template_specialization(cur_decl)) {
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

      std::ranges::move(
        template_specialization_types(arg_list) //
          | std::views::filter(&clang::Type::isEnumeralType) //
          | std::views::transform(to_tag_type) //
          | std::views::filter(not_null) //
          | std::views::filter(not_in_std) //
          | std::views::filter(not_resolved),
        std::inserter(collected, collected.begin()));

      std::ranges::for_each(
        template_specialization_types(arg_list) //
          | std::views::filter(&clang::Type::isRecordType)
          | std::views::transform(&clang::Type::getAsCXXRecordDecl),
        [&to_visit](const clang::CXXRecordDecl *rd) { to_visit.push(rd); });

      // not collecting fields or bases.
      continue;
    }

    // Standard-library wrappers may expose dependency protocol aliases above,
    // but unsupported wrappers are not themselves reflectable records. Do not
    // traverse their bases/fields: some declarations visible through bundled
    // headers have no definition data, and Clang's `bases()` requires it.
    if (cur_decl->isInStdNamespace())
      continue;

    // bases
    std::ranges::for_each(public_bases_view(cur_decl),
      [&to_visit](const clang::CXXRecordDecl *rd) { to_visit.push(rd); });

    // public fields
    std::ranges::move(public_fields_view(cur_decl) //
        | std::views::transform(&clang::FieldDecl::getType)
        | std::views::transform(&clang::QualType::getTypePtr)
        | std::views::filter(&clang::Type::isEnumeralType)
        | std::views::transform(to_tag_type) //
        | std::views::filter(not_null) //
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
        | std::views::transform([&ast](const clang::CXXRecordDecl *rd) {
            return clang::cast<clang::TagType>(
              ast.getCanonicalTagType(rd).getTypePtr());
          })
        | std::views::transform(
          std::bind_front(render_reflectable_id, std::cref(ast)))
        | std::ranges::to<std::vector>()
                             : std::vector<std::string>{},

    .public_fields = public_fields_view(&r_decl)
      | std::views::transform(std::bind_front(meta::field_data::from_decl,
        std::cref(ast)))
      | std::ranges::to<std::vector>(),

    .type = r_decl.isStruct() //
      ? meta::record_data::is_struct
      : r_decl.isClass() //
        ? meta::record_data::is_class
        : meta::record_data::is_union,

    .template_info =
      fn::maybe(llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(&r_decl))
        .transform(std::bind_front(meta::template_data::from_decl,
          std::cref(ast))),
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
    .id = render_reflectable_id(ast, t),
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
    .definition = resolve_definition(ast.getSourceManager(), t->getDecl()),
  };
}

auto meta::resolve_reflected_type(
  clang::QualType template_arg,
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

  while (template_arg->isReferenceType())
    template_arg = template_arg->getPointeeType();

  const clang::Type &template_arg_type =
    *ast.getCanonicalType(template_arg.getUnqualifiedType()).getTypePtr();
  // todo: C-arrays, pointers?

  // not a struct|class|union|enum
  if (!clang::isa<clang::TagType>(template_arg_type)) {
    return reflected_type_info{
      .type = non_reflectable{.id = render_type_id(ast, &template_arg_type)},
      .dependencies = {},
    };
  }

  const auto *tag_type = clang::cast<clang::TagType>(&template_arg_type);
  const meta::type_id id = render_reflectable_id(ast, tag_type);
  if (resolved_types.contains(id)) {
    return reflected_type_info{
      .type = already_reflected{.id = id},
      .dependencies = {}, //< refactorme: dependencies here make no sense
    };
  }

  if (clang::isa<clang::EnumType>(template_arg_type)) {
    const auto *enum_type = clang::cast<clang::EnumType>(&template_arg_type);
    return reflected_type_info{
      .type =
        reflectable{
          .id = id,
          .annotation = annotation_from_decl(ast, enum_type->getDecl()),
          .data = enum_data::from_type(enum_type),
          .definition =
            resolve_definition(ast.getSourceManager(), enum_type->getDecl()),
        },
      .dependencies = {},
    };
  }

  const auto *record_type = clang::cast<clang::RecordType>(&template_arg_type);
  const clang::RecordDecl *record_decl = record_type->getDecl();
  if (llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(record_decl)) {
    // todo: report source location and reflected-call stack for unsupported
    // template diagnostics.
    return std::unexpected(std::format(
      "unsupported reflected record template '{}': partial specializations are not supported",
      id));
  }

  if (const auto *spec =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record_decl)) {
    if (clang::TSK_ExplicitSpecialization == spec->getSpecializationKind()) {
      // todo: report source location and reflected-call stack for unsupported
      // template diagnostics.
      return std::unexpected(std::format(
        "unsupported reflected record template '{}': explicit specializations are not supported",
        id));
    }

    if (has_template_record_parent(record_decl->getDeclContext())) {
      // todo: report source location and reflected-call stack for unsupported
      // template diagnostics.
      return std::unexpected(std::format(
        "unsupported reflected record template '{}': nested record templates inside template records are not supported yet",
        id));
    }
  }

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

namespace render::impl {

std::string escaped_string_literal_content(std::string_view s) {
  std::string out;
  for (const unsigned char c : s) {
    switch (c) {
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    default:
      if (0x20 <= c && 0x7f > c)
        out += static_cast<char>(c);
      else
        out += std::format("\\{:03o}", static_cast<unsigned int>(c));
      break;
    }
  }
  return out;
}

std::string annotation_method(std::string_view annotation) {
  const std::string escaped = escaped_string_literal_content(annotation);
  return std::format(
    "\n  static constexpr auto annotation() noexcept"
    "\n    -> const char(&)[sizeof(\"{0}\")] {{"
    "\n    return \"{0}\";"
    "\n  }}",
    escaped);
}

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
    elems | std::views::join_with("_"sv) | util::format_range);
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
    elems | std::views::join_with("::"sv) | util::format_range);
}

std::string forward_declaration_for_enclosing_root(
  const meta::nm_qual_type &inner_type) {
  assert(!inner_type.enclosing_records.empty());

  return std::format(
    "{}"
    "{};"
    "{}",
    inner_type.namespaces //
      | std::views::filter([](std::string_view ns) { return !ns.empty(); })
      | std::views::transform([](std::string_view ns) {
          return std::format("namespace {} {{\n", ns);
        }) //
      | util::format_range,
    std::format("struct {}", inner_type.enclosing_records.front()),
    inner_type.namespaces //
      | std::views::filter([](std::string_view ns) { return !ns.empty(); })
      | std::views::transform([](std::string_view ns) {
          return std::format("\n}} // namespace {}", ns);
        }) //
      | util::format_range);
}

std::string format_qualified_inner_type_from_root(
  const std::string &root,
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
    spans | std::views::join | std::views::join_with("::"sv)
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
      std::vector<std::string> elems;
      elems.reserve(
        t.namespaces.size() + t.enclosing_records.size() + std::size_t{1});
      std::ranges::copy(t.namespaces, std::back_inserter(elems));
      std::ranges::copy(t.enclosing_records, std::back_inserter(elems));
      elems.emplace_back(d.template_info->primary_name);

      const std::string reflected_type_name =
        format_primary_template_type_name(t, d.template_info->primary_name);
      const std::string reflected_qualified_type_name =
        format_primary_template_qualified_type_name(t,
          d.template_info->primary_name);
      const std::string generated_type_name = std::format("{}<{}>",
        elems | std::views::join_with("::"sv) | util::format_range,
        template_args);

      // todo: when/if explicit or partial specializations are implemented,
      // reflected type names and namespace-qualified names must describe the
      // selected specialization. For primary templates, returning the primary
      // template name is intentional.
      return std::format(
        "{4}"
        "\nstruct _reflected<{0} {1}, _omni_binding> {{"
        "\n  static_assert(std::is_same<{0} {1}, _omni_binding>::value,"
        "\n    \"omnirefl: unexpected types mismatch, try regenerating\");"
        "\n"
        "\n  using type = _omni_binding;"
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
        std::format("template <\n  {},\n  typename _omni_binding\n>",
          format_template_params(d.template_info->params, 1)),

        // 5
        reflected_qualified_type_name,

        // 6
        annotation_method(annotation));
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
    format_nm_qual_type(t),

    // 2
    reflectable_entity(d),

    // 3
    format_type_name(t),

    // 4
    format_nm_qual_type(t),

    // 5
    annotation_method(annotation));
}

// SFINAE specialization for inner types of forward-declared types.
template <meta::reflectable_data Data>
std::string inner_reflectable_head(const meta::nm_qual_type &t,
  std::string_view annotation,
  const Data &d) {
  // to support, I need to store the decl field name for
  // `decltype(std::declval<root>().inner)
  assert(t.name && "unnamed inner structs not supported");

  const std::string access_root_for =
    std::format("{}<T>", enclosing_root_as_dependent(t));

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

    format_qualified_inner_type_from_root(access_root_for, t),
    reflectable_entity(d),
    format_type_name(t),
    format_nm_qual_type(t),
    annotation_method(annotation));
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
    format_type_name(t),
    format_nm_qual_type(t),
    annotation_method(annotation));
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
      | util::format_range);
}

std::string reflectable_body(const meta::record_data &d) {
  constexpr auto format_field = //
    [](const auto &field_index) {
      const auto &[index, f] = field_index;
      return std::format(
        "  struct {0}_t {{"
        "\n    static constexpr std::size_t index() noexcept {{ return {1}; }}"
        "\n"
        "\n    static constexpr auto name() noexcept"
        "\n      -> const char(&)[sizeof(\"{0}\")] {{"
        "\n      return \"{0}\";"
        "\n    }}"
        "\n"
        "\n    static constexpr auto type_name() noexcept"
        "\n      -> const char(&)[sizeof(\"{5}\")] {{"
        "\n      return \"{5}\";"
        "\n    }}"
        "{6}"
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
                      : std::format("const decltype(t.{})&", f.name),

        // 5
        escaped_string_literal_content(f.type_name),

        // 6
        annotation_method(f.annotation));
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
          d.public_fields | std::views::enumerate //
            | std::views::transform(format_field)
            | std::views::join_with("\n\n"sv) //
            | util::format_range),

    d.public_fields //
      | std::views::transform(&meta::field_data::name)
      | std::views::transform(
        [](std::string_view f) {
          return std::format("omni::field_meta_t<type, {}_t>", f);
        }) //
      | std::views::join_with(",\n      "sv) //
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
        return render::impl::reflectable_head(type_name,
          r.type.annotation,
          d);
      else if constexpr (std::same_as<rc::nested_type, S>)
        return render::impl::inner_reflectable_head(type_name,
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
        return render::impl::format_nm_qual_type(t);
      return render::impl::qualified_inner_type_from_fwd_root(t, "T");
    };

  return std::format("\n  using public_bases_t = std::tuple<{}>;",
    r.public_bases //
      | std::views::transform(fetch) //
      | std::views::transform(format) //
      | std::views::join_with(",\n    "sv) //
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
std::string render_reflectable_impl(
  const std::map<meta::type_id, meta::nm_qual_type> &type_name_by_id,
  const T &d) {
  return std::format(
    "{}"
    "\n{}",
    render_reflectable_head(d),
    render_reflectable_body(type_name_by_id, d.type));
}

std::string render_forward_declarable(
  const std::map<meta::type_id, meta::nm_qual_type> &type_name_by_id,
  const render::reflection_context::forward_declarable &d) {
  return render_reflectable_impl(type_name_by_id, d);
}

std::string render_nested_type(
  const std::map<meta::type_id, meta::nm_qual_type> &type_name_by_id,
  const render::reflection_context::nested_type &d) {
  return render_reflectable_impl(type_name_by_id, d);
}

std::string render_indexed_type(
  const std::map<meta::type_id, meta::nm_qual_type> &type_name_by_id,
  const render::reflection_context::indexed_type &d) {
  return render_reflectable_impl(type_name_by_id, d);
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

  const std::vector required_includes =
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
    "\n// -- reflected types --------"
    "\n{5}"
    "\n"
    "\n// -- reflected inner types --------"
    "\n{6}"
    "\n"
    "\n// -- generated fallback reflected types --------"
    "\n{7}"
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
    required_includes //
      | std::views::transform(
        [](std::string_view s) { return std::format("#include <{}>", s); })
      | std::views::join_with("\n"sv) //
      | util::format_range,

    // 2:
    ctx.fwd_declarables //
      | std::views::transform(&reflection_context::forward_declarable::type)
      | std::views::transform(render::impl::forward_declaration) //
      | std::views::join_with("\n\n"sv) //
      | util::format_range,

    // 3:
    ctx.enclosing_roots //
      | std::views::transform(
        render::impl::forward_declaration_for_enclosing_root) //
      | std::views::join_with("\n\n"sv) //
      | util::format_range,

    // 4:
    ctx.enclosing_roots //
      | std::views::transform(
        render::impl::declaration_for_enclosing_root_as_dependent) //
      | std::views::join_with("\n\n"sv) //
      | util::format_range,

    // 5:
    ctx.fwd_declarables //
      | std::views::transform(std::bind_front(render_forward_declarable,
        std::cref(ctx.type_name_by_id)))
      | std::views::join_with("\n\n"sv) //
      | util::format_range,

    // 6:
    ctx.nested //
      | std::views::transform(
        std::bind_front(render_nested_type, std::cref(ctx.type_name_by_id)))
      | std::views::join_with("\n\n"sv) //
      | util::format_range,

    // 7:
    ctx.indexed //
      | std::views::transform(
        std::bind_front(render_indexed_type, std::cref(ctx.type_name_by_id)))
      | std::views::join_with("\n\n"sv) //
      | util::format_range);

  // todo: check if file has errors

  return ctx;
}

} // namespace
