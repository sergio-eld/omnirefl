#include "tool/tool_template.hpp"
#include "tool/util.hpp"

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

#include <fmt/base.h>

#include <string_view>

tl::expected<std::vector<std::filesystem::path>, std::string> tool::filter_db_sources_t::operator()(
  args a,
  std::vector<std::filesystem::path> db_sources) const noexcept {
  db_sources = util::sorted(std::less{},
    util::filtered([](const std::filesystem::path &p) -> bool { return !p.empty(); },
      std::move(db_sources)));
  a.specified_sources = util::sorted(std::less{}, std::move(a.specified_sources));
  a.excluded_folders = util::sorted(std::less{}, std::move(a.excluded_folders));

  // as of now (just because I say so) `specified_sources` take precedense over `excluded_folders`
  if (!a.specified_sources.empty()) {
    // todo: validate that all the specified_sources are found within db_sources,
    // error otherwise

    return {tl::in_place, std::move(a.specified_sources)};
  }

  db_sources = util::filtered(
    [&excluded = a.excluded_folders](const std::filesystem::path &db_path) {
      for (const std::filesystem::path &e : excluded) {
        if (util::is_subpath(db_path, e))
          return true;
      }
      return false;
    },
    std::move(db_sources));

  if (db_sources.empty())
    return tl::unexpected("no sources for reflection provided");
  return {tl::in_place, std::move(db_sources)};

  // todo: this might be considered if `allow_missing_sources` option is introduced that allows a
  // specified source to be missing from the db sources if (!a.specified_sources.empty()) {
  //   std::vector<std::filesystem::path> intersected;
  //   intersected.reserve(a.specified_sources.size());
  //   std::set_intersection(a.specified_sources.cbegin(),
  //     a.specified_sources.cend(),
  //     result->cbegin(),
  //     result->cend(),
  //     std::back_inserter(intersected));
  //   result.emplace(std::move(intersected));
  // }
  // _filtered = util::filtered(
  //   std::move(_filtered));
}

namespace {
void configure_compiler_invocation(bool a_print_debug,
  const std::string_view &resource_dir,
  clang::CompilerInvocation &ci) {
  // for some reason this doesn't have any effect if set up here, unlike the paths'
  // modifications below
  // c.getHeaderSearchOpts().ResourceDir = resource_dir.getValue();

  ci.getHeaderSearchOpts().AddPath(
    // todo: path from cli, since it is architecture dependent
    fmt::format("{}/include/x86_64-unknown-linux-gnu/c++/v1", resource_dir),
    clang::frontend::IncludeDirGroup::System,
    // I have no idea what are these parameters
    /*IsFramework=*/false,
    /*IgnoreSysRoot=*/false);

  ci.getHeaderSearchOpts().AddPath(fmt::format("{}/include/c++/v1", resource_dir),
    clang::frontend::IncludeDirGroup::System,
    // I have no idea what are these parameters
    /*IsFramework=*/false,
    /*IgnoreSysRoot=*/false);

  // ad hoc: C++ headers must be included before C's
  std::rotate(ci.getHeaderSearchOpts().UserEntries.rbegin(),
    ci.getHeaderSearchOpts().UserEntries.rbegin() + 2,
    ci.getHeaderSearchOpts().UserEntries.rend());

  if (a_print_debug) {
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
      fmt::println("debug: system header prefix: {prefix}", fmt::arg("prefix", h.Prefix));
    }
  }

  // disable pch and warnings

  // createInvocationFromCommandLine sets DisableFree.
  ci.getFrontendOpts().DisableFree = false;
  // todo: ifdef based on clang version, otherwise these code results in compilation errors
  // ci.getLangOpts()->CommentOpts.ParseAllComments = true;
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

  [](auto &preproc) {
    // Disable any pch generation/usage operations. Since serialized preamble
    // format is unstable, using an incompatible one might result in
    // unexpected behaviours, including crashes.
    preproc.ImplicitPCHInclude.clear();
    preproc.PrecompiledPreambleBytes = {0, false};
    preproc.PCHThroughHeader.clear();
    preproc.PCHWithHdrStop = false;
    preproc.PCHWithHdrStopCreate = false;
  }(ci.getPreprocessorOpts());
}
} // namespace

tool::parse_ast_t::result_t tool::parse_ast_t::operator()(args a) const {
  struct: clang::tooling::ToolAction {
    result_t m_result = tl::unexpected("unexpected: tool was not invoked");
    std::string_view m_resource_dir;
    bool print_debug;

    // as of now this ad hoc is only needed because ClangTool initializes the args for
    // LoadFromCompilerInvocation
    bool runInvocation(std::shared_ptr<clang::CompilerInvocation> inv,
      clang::FileManager *files,
      std::shared_ptr<clang::PCHContainerOperations> pch_cont_ops,
      clang::DiagnosticConsumer *diag_cons) override {
      configure_compiler_invocation(print_debug, m_resource_dir, *inv);

      // todo: this should be sufficient, without ClangTool
      // parse AST
      std::unique_ptr<clang::ASTUnit> ast = clang::ASTUnit::LoadFromCompilerInvocation(inv,
        pch_cont_ops,
        clang::CompilerInstance::createDiagnostics(&inv->getDiagnosticOpts(),
          diag_cons,
          /*ShouldOwnClient=*/false),
        files);

      if (ast->getDiagnostics().hasUnrecoverableErrorOccurred()) {
        // todo: filename and diagnostics
        m_result = tl::unexpected(std::string("failed to parse AST"));
        return false;
      }

      m_result.emplace(std::move(ast));
      return true;
    }
  } adapter{};
  auto str_resource_dir = a.resource_dir.string();
  adapter.m_resource_dir = str_resource_dir;
  adapter.print_debug = a.print_debug;

  clang::tooling::ClangTool tool(a.db, {a.source.string()});

  // bolnoi ubliudok... this works
  // todo: try to set them in `configure_compiler_invocation`
  tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
    {
      {"-nostdinc++"}, // prevents from picking up on another compiler's C++ std libs
      {"-resource-dir=" + str_resource_dir},
      {"-fno-delayed-template-parsing"}, // it seems to not work thought
    },
    clang::tooling::ArgumentInsertPosition::BEGIN));

  if (0 == tool.run(&adapter))
    return std::move(adapter.m_result);
  return tl::unexpected("errors while invoking ClangTool::run");
}
