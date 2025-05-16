#include "tool/ast.hpp"
#include "fmt/base.h"
#include "tool/cli.hpp"
#include "tool/util.hpp"
#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Tooling/Tooling.h>
#pragma GCC diagnostic pop

#include <fmt/core.h>

#include <algorithm>
#include <string_view>

namespace {
template <typename Mode>
void configure_compiler_invocation(const Mode &m,
  tool::cli::verbosity_level verbosity,
  const std::string_view &resource_dir,
  clang::CompilerInvocation &ci) {
  // for some reason this doesn't have any effect if set up here, unlike the
  // paths' modifications below c.getHeaderSearchOpts().ResourceDir =
  // resource_dir.getValue();

  ci.getHeaderSearchOpts().AddPath(
    // todo: path from cli, since it is architecture dependent
    fmt::format("{}/include/x86_64-unknown-linux-gnu/c++/v1", resource_dir),
    clang::frontend::IncludeDirGroup::System,
    // I have no idea what are these parameters
    /*IsFramework=*/false,
    /*IgnoreSysRoot=*/false);

  ci.getHeaderSearchOpts().AddPath(
    fmt::format("{}/include/c++/v1", resource_dir),
    clang::frontend::IncludeDirGroup::System,
    // I have no idea what are these parameters
    /*IsFramework=*/false,
    /*IgnoreSysRoot=*/false);

  // ad hoc: C++ headers must be included before C's
  std::rotate(ci.getHeaderSearchOpts().UserEntries.rbegin(),
    ci.getHeaderSearchOpts().UserEntries.rbegin() + 2,
    ci.getHeaderSearchOpts().UserEntries.rend());

  if (tool::cli::print_debug(verbosity)) {
    for (const auto &h : ci.getHeaderSearchOpts().UserEntries) {
      fmt::println(
        "debug: user header: {header},"
        " group: {group},"
        " is framework: {is_framework}",
        fmt::arg("header", h.Path),
        fmt::arg("group", int(h.Group)),
        fmt::arg("is_framework", h.IsFramework));
    }

    for (const auto &h : ci.getHeaderSearchOpts().SystemHeaderPrefixes) {
      fmt::println("debug: system header prefix: {prefix}",
        fmt::arg("prefix", h.Prefix));
    }
  }

  // disable pch and warnings

  // createInvocationFromCommandLine sets DisableFree.
  ci.getFrontendOpts().DisableFree = false;
  // todo: ifdef based on clang version, otherwise these code results in
  // compilation errors ci.getLangOpts()->CommentOpts.ParseAllComments = true;
  // ci.getLangOpts()->RetainCommentsFromSystemHeaders = true;

  [](auto &diag) {
    diag.Warnings.clear();
    diag.UndefPrefixes.clear();
    diag.Remarks.clear();
    diag.VerifyPrefixes.clear();
  }(ci.getDiagnosticOpts());

  [](auto &output) {
    // Disable any dependency outputting, we don't want to generate files or
    // write to stdout/stderr.
    output.ShowIncludesDest = clang::ShowIncludesDestination::None;
    output.OutputFile.clear();
    output.HeaderIncludeOutputFile.clear();
    output.DOTOutputFile.clear();
    output.ModuleDependencyOutputDir.clear();
  }(ci.getDependencyOutputOpts());

  [&m, verbosity](clang::PreprocessorOptions &p) {
    // Disable any pch generation/usage operations. Since serialized preamble
    // format is unstable, using an incompatible one might result in
    // unexpected behaviours, including crashes.
    p.ImplicitPCHInclude.clear();
    p.PrecompiledPreambleBytes = {0, false};
    p.PCHThroughHeader.clear();
    p.PCHWithHdrStop = false;
    p.PCHWithHdrStopCreate = false;

    if constexpr (std::is_same_v<Mode, tool::cli::header_mode>) {
      const tool::cli::header_mode &mode = m;
      constexpr std::string_view k_omni_macro = "OMNI_HEADER_REFLECTION";

      const auto omni_defined = std::find_if(p.Macros.begin(),
        p.Macros.end(),
        [k_omni_macro](const auto &macro_def) {
          const auto &[macro, is_undef] = macro_def;
          return macro == k_omni_macro;
        });

      if (p.Macros.cend() == omni_defined)
        p.Macros.emplace_back(k_omni_macro, /*isUndef*/ false);
      else {
        auto &[_, is_undef] = *omni_defined;
        // todo: warning if `true == is_undef`?
        is_undef = false;
      }

      // removing force-included reflection header that is being generated
      p.Includes = util::filtered(
        [&output_dir = mode.output_dir](const std::string &s) -> bool {
          // todo: should I remove the exact path?
          return util::is_subpath(s, output_dir);
        },
        std::move(p.Includes));

      if (tool::cli::print_debug(verbosity)) {
        fmt::println("{}",
          util::join(p.Macros, "\n", [](const auto &pair, fmt::context &ctx) {
            const auto &[macro, is_undef] = pair;
            return fmt::format_to(ctx.out(),
              "DEBUG: #{} {}",
              is_undef ? "undef" : "define",
              macro);
          }));
        fmt::println("{}", util::join(p.Includes, "\n", [] {
          return "DEBUG: #include \"{}\"";
        }));
      }
    } else {
      static_assert(std::is_same_v<Mode, tool::cli::source_mode>);
      (void)m;
      (void)verbosity;
    }
  }(ci.getPreprocessorOpts());
}

template <typename Mode>
tl::expected<std::unique_ptr<clang::ASTUnit>, std::string>
  parse_ast_from_source(const Mode &mode,
    const std::filesystem::path &resource_dir,
    const std::filesystem::path &source,
    // refactorme: pass command-line args
    const clang::tooling::CompilationDatabase &db,
    const std::optional<std::filesystem::path> &output_path,
    tool::cli::verbosity_level verbosity) noexcept {
  using result_t = tl::expected<std::unique_ptr<clang::ASTUnit>, std::string>;
  struct: clang::tooling::ToolAction {
    result_t m_result = tl::unexpected("unexpected: tool was not invoked");
    std::string_view m_resource_dir;
    const Mode *mode;
    tool::cli::verbosity_level m_verbosity;

    // ClangTool may find several entries for the provided `source` in the
    // compilation database, and will run the tool for each one. I don't need
    // that. So, either wait for the specified file, or select the first one if
    // none was specified
    const std::filesystem::path *m_output_path = nullptr;

    // as of now this ad hoc is only needed because ClangTool initializes the
    // args for LoadFromCompilerInvocation
    bool runInvocation(std::shared_ptr<clang::CompilerInvocation> inv,
      clang::FileManager *files,
      std::shared_ptr<clang::PCHContainerOperations> pch_cont_ops,
      clang::DiagnosticConsumer *diag_cons) override {
      configure_compiler_invocation(*mode, m_verbosity, m_resource_dir, *inv);

      // Skip duplicate compile-commands created when several CMake targets
      // share this source file.
      const std::filesystem::path output_file =
        inv->getFrontendOpts().OutputFile;

      if (m_output_path) {
        // Run only the entry whose object-file path matches m_output_file.
        if (output_file != *m_output_path) {
          if (tool::cli::verbosity_level::info & m_verbosity) {
            fmt::println(
              "INFO: Skipping invocation for mismatched output file.\nExpected: {}\nInvoked: {}\n",
              m_output_path->string(),
              output_file.string());
          }
          return true;
        }
      } else {
        // Run only first successfult source.
        if (m_result) {
          if (tool::cli::verbosity_level::info & m_verbosity) {
            fmt::println(
              "INFO: Skipping invocation for duplicate entry. \nInvoked: {}\n",
              output_file.string());
          }
          return true;
        }
      }

      // todo: this should be sufficient, without ClangTool
      // parse AST
      std::unique_ptr<clang::ASTUnit> ast =
        clang::ASTUnit::LoadFromCompilerInvocation(inv,
          pch_cont_ops,
          clang::CompilerInstance::createDiagnostics(&inv->getDiagnosticOpts(),
            diag_cons,
            /*ShouldOwnClient=*/false),
          files);

      if (!ast || ast->getDiagnostics().hasUnrecoverableErrorOccurred()) {
        // todo: filename and diagnostics
        m_result = tl::unexpected(std::string("failed to parse AST"));
        return false;
      }

      m_result.emplace(std::move(ast));
      return true;
    }
  } adapter{};

  auto str_resource_dir = resource_dir.string();
  adapter.m_resource_dir = str_resource_dir;
  adapter.mode = &mode;
  adapter.m_verbosity = verbosity;
  if (output_path)
    adapter.m_output_path = std::addressof(*output_path);

  clang::tooling::ClangTool tool(db, {source.string()});

  // bolnoi ubliudok... this works
  // todo: try to set them in `configure_compiler_invocation`
  tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
    {
      {"-nostdinc++"}, //< prevents from picking up on another compiler's C++
                       //< std libs
      {fmt::format("-resource-dir={}", str_resource_dir)},
      {"-fno-delayed-template-parsing"}, //< it seems to not work thought
    },
    clang::tooling::ArgumentInsertPosition::BEGIN));

  const int run_resutl = tool.run(&adapter);
  if (!adapter.m_result || 0 == run_resutl)
    return std::move(adapter.m_result);
  return tl::unexpected("errors while invoking ClangTool::run");
}

} // namespace

tl::expected<std::unique_ptr<clang::ASTUnit>, std::string>
  tool::parse_ast_from_source(const cli::source_mode &m,
    const std::filesystem::path &resource_dir,
    const std::filesystem::path &source,
    const clang::tooling::CompilationDatabase &db,
    const std::optional<std::filesystem::path> &output_path,
    cli::verbosity_level verbosity) noexcept {
  return ::parse_ast_from_source(m,
    resource_dir,
    source,
    db,
    output_path,
    verbosity);
}

tl::expected<std::unique_ptr<clang::ASTUnit>, std::string>
  tool::parse_ast_from_source(const cli::header_mode &m,
    const std::filesystem::path &resource_dir,
    const std::filesystem::path &source,
    const clang::tooling::CompilationDatabase &db,
    const std::optional<std::filesystem::path> &output_path,
    cli::verbosity_level verbosity) noexcept {
  return ::parse_ast_from_source(m,
    resource_dir,
    source,
    db,
    output_path,
    verbosity);
}

tl::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
  tool::load_compilation_db(
    const std::filesystem::path &compilation_db_path) noexcept {
  std::string err;
  auto ptr = clang::tooling::CompilationDatabase::loadFromDirectory(
    compilation_db_path.string(),
    err);

  if (!ptr)
    // todo: reference the path in the error
    return tl::unexpected(std::move(err));
  return {
    tl::in_place,
    std::move(ptr),
  };
};
