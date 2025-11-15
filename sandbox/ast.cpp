
#include "tool/util.hpp"

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

#include <CLI/CLI.hpp>
#include <CLI/Error.hpp>
#include <CLI/Validators.hpp>

#include <algorithm>
#include <expected>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <stack>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace util {

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

} // namespace util

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
  std::ranges::view auto v_parts = std::views::split(arg, ':')
    | std::views::transform([](const auto &s) { return std::string_view(s); });
  const std::vector parts(v_parts.begin(), v_parts.end());
  if (2 < parts.size() || parts.front().empty())
    return false;

  src.path = parts.front();
  src.output_substr = 2 == parts.size() ? parts.back() : "";

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

std::string to_string(const options &o) {
  // todo: consider printing flags
  return fmt::format(R"(mode:{mode}
sources:[{sources}]
out:{out}
resource_dir:{resource_dir})",
    fmt::arg("mode", to_string(o.mode)),
    fmt::arg("sources",
      util::join(o.sources,
        ",\n",
        [](const source_file &s, fmt::context &ctx) {
          return fmt::format_to(ctx.out(), "{}", s.path.string());
        })),
    fmt::arg("out", o.out.generic_string()),
    fmt::arg("resource_dir", o.resource_dir.generic_string()));
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
            , binding_tag(binding_tag) {
        }
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
};

enum reference_type {
  ref_none,
  ref_lval,
  ref_rval,
};

struct type_definition {
  std::filesystem::path source_file;

  nm_qual_type type_name;

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
  bool is_const : 1;
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
};

std::expected<reflected_call_info, std::string> resolve_reflected_call(
  const clang::CXXMethodDecl &decl,
  const clang::ASTUnit &ast);

} // namespace meta

namespace render {

// todo: here goes formatters implementations for the meta types:
// - debug (dump to ostream)
// - yaml
// - reflected specialization

} // namespace render

} // namespace

int main(int argc, char **argv) {
  const std::expected cli_args = cli::parse(argc, argv);
  if (!cli_args) {
    const auto &[code, error] = cli_args.error();
    std::cerr << error << '\n';
    return code;
  }

  fmt::println("args:\n{}", to_string(*cli_args));

  const llvm::IntrusiveRefCntPtr file_manager = new clang::FileManager({
    .WorkingDir = fs::current_path().parent_path(),
  });

  // loading the ast
  for (const auto &[n_processing, sf] : util::indexed(cli_args->sources, 1)) {
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

    fmt::println("[{}/{}] running {mode} mode for file: {file}\t\r",
      n_processing,
      cli_args->sources.size(),
      fmt::arg("mode", cli::to_string(cli_args->mode)),
      fmt::arg("file", sf.path.string()));

    std::expected compiler_invocation = compiler_invocation_from_args(sf,
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

      fmt::println("{}",
        util::join(po.Macros, "\n", [](const auto &pair, fmt::context &ctx) {
          const auto &[macro, is_undef] = pair;
          return fmt::format_to(ctx.out(),
            "DEBUG: #{} {}",
            is_undef ? "undef" : "define",
            macro);
        }));
      fmt::println("{}",
        util::join(po.Includes, "\n", [] { return "DEBUG: #include \"{}\""; }));
    }

    // fixme: no matches detected. Investigate if it is caused by ast or by
    // reduce todo: how about creating the ast context directly (without
    // clang::ASTUnit helper)
    const std::unique_ptr ast =
      clang::ASTUnit::LoadFromCompilerInvocation(*compiler_invocation,
        std::make_shared<clang::PCHContainerOperations>(),
        diag,
        file_manager.get());

    if (!ast || ast->getDiagnostics().hasUncompilableErrorOccurred()) {
      std::cerr << fmt::format("Failed to build AST Unit for: {}.\n",
        sf.path.generic_string());
      continue;
    }

    using namespace clang::ast_matchers;

    struct context {
      std::vector<meta::reflectable> reflected;
      // todo: enums

      std::set<meta::type_id> resolved_types;
      std::set<meta::type_id> resolved_as_dependent;
      std::vector<std::string> errors;
    };

    context ctx = ast::reduce_matches(*ast,
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
                } else if constexpr (std::same_as<meta::non_reflectable, T>) {
                  // todo:
                  // Only a limited set of T in _relfected_type<T> is
                  // supported, and it should be reported as error diagnostics
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
            // todo: implement
            return a;
          },
      });

    for (const meta::reflectable &r : ctx.reflected) {
      std::vector nm_qual_type = r.definition.type_name.namespaces;
      nm_qual_type.emplace_back(
        r.definition.type_name.name.value_or("unnamed"));

      fmt::println("reflected type{}: {}",
        ctx.resolved_as_dependent.contains(r.id) ? " (as dependency)" : "",
        fmt::join(nm_qual_type, "::"));
    }

    // todo: render to &ostream (file/console/whatever OS wants)
    // todo: generate file/files
  }

  return 0;
}

namespace {

std::expected<cli::options, std::pair<int, std::string>> cli::parse(int argc,
  const char *const *argv) noexcept {
  CLI::App app{
    R"(
C++ reflection code generator that operates in three modes:

Source Mode (default):
  Generates a single .cpp file containing reflected call implementations for a 
  list of .cpp sources. Compiled object file needs to be linked to the resulting binary.

Header Mode:
  Generates .hpp header files containing reflected call implementations for 
  given .cpp files. Headers must be implicitly included at the start of each 
  translation unit.
  WARNING: Uses compile time counters via friend injection, which is not guaranteed
           by the C++ Standard to be consistent between compiler implementations.

Dump yaml:
  todo: explain
)"};

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
    ->default_val(fs::current_path());

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

      std::ranges::view auto invalid_sources = cli_sources
        | std::views::filter(
          [](const cli_source_file &s) -> bool { return !fs::exists(s.path); });

      if (!invalid_sources.empty()) {
        throw CLI::ValidationError("Non-existent sources:",
          fmt::format("[{}]",
            util::join(
              std::vector(invalid_sources.begin(), invalid_sources.end()),
              ", ",
              [](const cli_source_file &s, fmt::context &ctx) {
                return fmt::format_to(ctx.out(), "{}", s.path.string());
              })));
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
          fmt::format("failed to resolve from {} commands by substring `{}`",
            commands.size(),
            s.output_substr));
        return accum;
      });

    if (!invalid.empty())
      throw CLI::ValidationError(
        "Failed to resolve sources from compilation db",
        fmt::format("[{}]",
          util::join(invalid, ",\n", [](const auto &pair, fmt::context &ctx) {
            const auto &[path, reason] = pair;
            return fmt::format_to(ctx.out(),
              "{}: {}",
              path.generic_string(),
              reason);
          })));

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
      fmt::format("Error: Missing argument for option at index {}\n",
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
  fmt::println("input flags: [{}]",
    util::join(flags, "\n", [] { return "{}"; }));

  std::expected argList = parse_driver_args(flags);

  if (!argList)
    return std::unexpected(argList.error());

  std::vector cc1_args =
    // ad hoc: need to append these, since as of now the actual Driver is used
    // for parsing and configuration. There are only 2 things (I think) I
    // actually need (but forced to use the Driver):
    // 1. Map non-clang args to cc1 format
    // 2. Resolve system include paths (non-std)
    util::concat(
      std::vector<std::string>{
        "omnirefl",
        source.generic_string(),
        "-fsyntax-only", //< AST only
        "-nostdinc++", //< prevents from picking up on another compiler's C++
                       //< std libs
        fmt::format("-resource-dir={}", a.resource_dir.generic_string()),
      },
      filter_ast_related_args(*argList));

  // ad hoc: this is a heavy-weight solution just to get proper argument
  // translations (for other compilers' commands) and to resolve system
  // include paths...
  // {
  const std::string target_triple = std::invoke([&] {
    if (const auto *a = argList->getLastArgNoClaim(options::OPT_target))
      return llvm::Triple::normalize(a->getValue());
    return llvm::sys::getProcessTriple();
  });

  clang::driver::Driver driver("omnirefl",
    target_triple,
    a.diag,
    "omnirefl reflection tool");
  driver.setCheckInputsExist(false);

  const std::vector cc1_args_ref = to_vector_of_raw_pointers(cc1_args);
  const std::unique_ptr<clang::driver::Compilation> compilation(
    driver.BuildCompilation(llvm::ArrayRef(cc1_args_ref.data(),
      cc1_args_ref.data() + cc1_args_ref.size())));

  if (!compilation || compilation->getJobs().empty()) {
    return std::unexpected(
      fmt::format("Failed to build compilatioin for: {}.\n",
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
  fmt::println("using cc1 args: [{}]",
    // util::join(cc1_args, "\n", [] { return "{}"; }));
    util::join(compilation_args, "\n", [] { return "{}"; }));

  std::shared_ptr compiler_invocation =
    std::make_shared<clang::CompilerInvocation>();

  {
    const std::vector cc1_args_ref = to_vector_of_raw_pointers(cc1_args);
    if (!clang::CompilerInvocation::CreateFromArgs(*compiler_invocation,
          // fixme: cc1_args is empty after filtering
          // {cc1_args_ref.data(), cc1_args_ref.data() + cc1_args_ref.size()},
          compilation_args,
          a.diag)) {
      return std::unexpected(
        fmt::format("Failed to create CompilerInvocation for: {}.",
          source.generic_string()));
    }
  }

  {
    clang::HeaderSearchOptions &o = compiler_invocation->getHeaderSearchOpts();

    // own libc++ includes are bundled
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
      fmt::format("invalid template signature for internal omnirefl type {}",
        detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Type != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(fmt::format("non-type template argument `{}` of {}",
      n,
      detail_struct_name));
  }

  return arg.getAsType().getTypePtr();
}

std::expected<int, std::string> get_template_value_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) noexcept {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() < n) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(
      fmt::format("invalid template signature for internal omnirefl type {}",
        detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Integral != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(
      fmt::format("non-integral template argument `{}` of {}",
        n,
        detail_struct_name));
  }

  return arg.getAsIntegral().getExtValue();
}

meta::nm_qual_type resolve_nm_qual_type(const clang::TagDecl &td) noexcept {
  return {
    .name =
      std::invoke([id = td.getIdentifier()] -> std::optional<std::string> {
        if (id)
          return id->getName().str();
        return std::nullopt;
      }),
    .namespaces = std::invoke([&decl_ctx = *td.getDeclContext()] {
      std::vector<std::string> result;

      const clang::DeclContext *dc = &decl_ctx;
      while (!llvm::isa<clang::TranslationUnitDecl>(dc)) {
        if (const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(dc)) {
          result.emplace_back(
            ns->isAnonymousNamespace() ? "" : ns->getName().str());
        }
        dc = dc->getParent();
      }

      std::ranges::reverse(result);
      return result;
    }),
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

  return {
    .source_file = get_declaration_source_file(td, sm),
    .type_name = resolve_nm_qual_type(td),
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
    for (const member_typedef_decl &md : util::filtered(
           [](const member_typedef_decl &m) -> bool {
             const static std::set<std::string_view> aliases{{
               "key_type",
               "value_type",
               "value",
             }};
             return !aliases.contains(m.name);
           },
           member_typedefs(*cur))) {
      if (md.qual_type->isStructureOrClassType())
        to_visit.push(md.qual_type->getAsCXXRecordDecl());
    }
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
    return std::unexpected(fmt::format("unsupported type {} in {}",
      // fixme:
      //   I don't think it does what I need (namespace-qualified typename)
      reflected_type.getTypeClassName(),
      template_decl.getName()));
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
      fmt::format("forward declarations are not allowed: {}", nm_qual_type));
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

    // refactorme: I want a monadic 'ternary' operation to avoid creating
    // lvalues:
    //    .type_name = value_or(base_q->getAsTagDecl(), resolve_nm_qual_type,
    //      meta::nm_qual_type{})
    const clang::TagDecl *tag = base_q->getAsTagDecl();
    return {
      .type_name = tag 
        ? resolve_nm_qual_type(*tag)
        : meta::nm_qual_type{
          .name = base_q.getAsString(), //< no don't need printing policy for fundamental
          .namespaces = {}, //< fundamental type doesn't have namespace
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
      // todo: I'm not sure this is correct
      const clang::QualType base_q =
        q->isLValueReferenceType() || q->isRValueReferenceType()
        ? q->getPointeeType()
        : q;

      const clang::Type *ty = base_q.getTypePtrOrNull();
      const clang::CXXRecordDecl &rd = *ty->getAsCXXRecordDecl();
      const clang::CXXRecordDecl &def = *rd.getDefinition();

      // fixme: I only need the source_file, not the whole definition
      const auto def_info = resolve_definition(def, sm);
      const bool is_std = rd.isInStdNamespace();

      return {
        .include = is_std ? def_info.source_file.stem() : def_info.source_file,
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
