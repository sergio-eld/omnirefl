
// todo:
// - ast caching (?)
// - run queries on the ast and output the code

#include "fmt/base.h"
#include <fmt/format.h>
#include <tl/expected.hpp>
#include <variant>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/AST/ASTImporter.h>
#include <clang/AST/DeclBase.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/PrecompiledPreamble.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#pragma GCC diagnostic pop

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;
namespace matchers = clang::ast_matchers;

using clang::ASTUnit;

namespace util {
namespace str {
struct is_empty {
  constexpr bool operator()(std::string_view s) const {
    return s.empty();
  }
} constexpr const inline is_empty{};
} // namespace str

struct sorted_t {
  template <typename Cmp, typename Container>
  Container operator()(Cmp cmp, Container &&c) const {
    // todo: what if non-const reference?
    Container _s = std::forward<Container>(c);
    std::sort(_s.begin(), _s.end(), cmp);
    return _s;
  }
} constexpr const inline sorted{};

struct filtered {
  template <typename Condition, typename Container>
  auto operator()(Condition cnd, Container &&c) const {
    // todo: what if non-const reference?
    Container _c = std::forward<Container>(c);
    std::erase_if(_c, cnd);
    return _c;
  }
} constexpr const inline filtered{};

std::string get_declaration_source_file(const clang::Decl &d, const clang::SourceManager &sm) {
  const auto loc = d.getLocation();
  // todo: use expected?
  if (!loc.isValid())
    return "";

  const auto pm = sm.getPresumedLoc(loc);
  if (!pm.isValid())
    return "";

  std::string filename = pm.getFilename();
  return filename;
}

bool is_header_file(std::string_view filename) {
  llvm::StringRef ext = llvm::sys::path::extension(filename);
  return ext.equals_insensitive(".h") || ext.equals_insensitive(".hpp")
    || ext.equals_insensitive(".hxx");
}
} // namespace util

// todo: does this namespace make sense now?
namespace actions {

// aggregated arguments of clang tool's `runInvocation` function
struct clang_tool_input {
  const std::shared_ptr<clang::CompilerInvocation> &compiler_invocation;
  clang::FileManager *files;
  const std::shared_ptr<clang::PCHContainerOperations> &pch_cont_ops;
  clang::DiagnosticConsumer *diag_cons;
};

struct print_files_progress {
  size_t total;
  // refactorme: avoid mutable state, it should be an additional argument, return +1
  size_t processing = 0;
  void operator()(const clang_tool_input &i) {
    // invocation is run on a single file. This is not obvious, but apparently
    // it is how it works
    const auto &sourse_file =
      std::string(i.compiler_invocation->getFrontendOpts().Inputs[0].getFile());
    std::cout << fmt::format("[{}/{}] building AST of file: {}\t\r",
      ++processing,
      total,
      sourse_file)
              << std::flush;
  }
};

// refactorme: this should be a result of direct cli options mapping
struct configure_compiler_invocation {
  std::string resource_dir;
  void operator()(clang::CompilerInvocation &c) const {
    // for some reason this doesn't have any effect if set up here, unlike the paths'
    // modifications below
    // c.getHeaderSearchOpts().ResourceDir = resource_dir.getValue();

    c.getHeaderSearchOpts().AddPath(
      // todo: path from cli, since it is architecture dependent
      fmt::format("{}/include/x86_64-unknown-linux-gnu/c++/v1", resource_dir),
      clang::frontend::IncludeDirGroup::System,
      // I have no idea what are these parameters
      /*IsFramework=*/false,
      /*IgnoreSysRoot=*/false);

    c.getHeaderSearchOpts().AddPath(fmt::format("{}/include/c++/v1", resource_dir),
      clang::frontend::IncludeDirGroup::System,
      // I have no idea what are these parameters
      /*IsFramework=*/false,
      /*IgnoreSysRoot=*/false);

    // ad hoc: C++ headers must be included before C's
    std::rotate(c.getHeaderSearchOpts().UserEntries.rbegin(),
      c.getHeaderSearchOpts().UserEntries.rbegin() + 2,
      c.getHeaderSearchOpts().UserEntries.rend());

    for (const auto &h : c.getHeaderSearchOpts().UserEntries) {
      std::cout << "debug: user header: " << h.Path << ", group: " << h.Group
                << ", is framework: " << h.IsFramework << '\n';
    }
    for (const auto &h : c.getHeaderSearchOpts().SystemHeaderPrefixes)
      std::cout << "debug: system header prefix: " << h.Prefix << '\n';
  }
};

struct disable_pch_and_warnings {
  void operator()(clang::CompilerInvocation &ci) {
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
};

// this is an adaptor interface to allow folding the AST into the list of matches
template <typename... Matchers>
struct match_reflections_ast {
  tl::expected<std::vector<std::variant<Matchers...>>, std::string> operator()(ASTUnit &ast) const {
    struct: matchers::MatchFinder::MatchCallback {
      std::vector<std::variant<Matchers...>> result;
      std::optional<std::string> error = std::nullopt;

      void run(const matchers::MatchFinder::MatchResult &mresult) override {
        fmt::println("debug: matched {} nodes", mresult.Nodes.getMap().size());
        // todo: handle errors
        //  - tag has been found but type mismatches
        const auto add_node = [this](auto n) {
          if (n.node)
            result.push_back(n);
        };
        (add_node(Matchers{
           .node = mresult.Nodes.getNodeAs<typename Matchers::node_type>(Matchers::binding_tag),
         }),
          ...);
      }
    } callback;

    matchers::MatchFinder finder;
    (finder.addMatcher(Matchers::make(), &callback), ...);
    finder.matchAST(ast.getASTContext());
    if (callback.error)
      return tl::unexpected(std::move(callback.error).value());
    return std::move(callback.result);
  }
};

} // namespace actions

struct matched_reflection {
  constexpr static const char binding_tag[] = "matched_reflection";
  using node_type = clang::ClassTemplateSpecializationDecl;
  const node_type *node;

  static auto make() noexcept {
    using namespace clang::ast_matchers;
    // TK_AsIs is needed to include template instantiations
    return traverse(clang::TK_AsIs,
      classTemplateSpecializationDecl( //
        unless(isInStdNamespace()),
        unless(isExpansionInSystemHeader()),
        hasAncestor(namespaceDecl(hasName("omni::detail"))),
        isTemplateInstantiation(),
        isDefinition(),
        isStruct())
        .bind(binding_tag));
  }
};

struct matched_reflected_call {
  constexpr static const char binding_tag[] = "matched_reflected_call";
  using node_type = clang::CXXMethodDecl;
  const node_type *node;

  static auto make() noexcept {
    using namespace clang::ast_matchers;
    // TK_AsIs is needed to include template instantiations
    return traverse(clang::TK_AsIs,
      cxxMethodDecl( //
        unless(isInStdNamespace()),
        unless(isExpansionInSystemHeader()),
        hasAncestor(namespaceDecl(hasName("omni"))),
        hasAncestor(cxxRecordDecl(hasName("reflected_call_t"))),
        isTemplateInstantiation(),
        hasName("_call_impl"))
        .bind(binding_tag));
  }
};

// todo: context should be bound to list of matches to avoid compilation errors
// structure that collects and saves intermediate state from several ASTs
struct context {
  // flags for definition properties
  enum type_definition_flags {
    // no flags has been set
    none = 0x0,
    // unnamed structure
    unnamed = 0x1,
    // definition within a scope
    local = 0x1 << 1,
    // defined within a .cpp file (source): when using a standalone tool
    // definition can (probably) be generated within the same .cpp file via
    // a force-include compiler command
    in_cpp = 0x1 << 2,
  };

  struct type_definition {
    // fully namespace-qualified type name
    std::string name;
    fs::path source_file;
    type_definition_flags definition_flags = none;
  };

  struct field_data {
    std::string name;

    // fully namespace-qualified type
    std::string qualified_type;
  };

  struct func_arg {
    // fully cv and namespace-qualified type
    std::string qual_type;
    std::string name;
  };

  struct func_signature {
    std::vector<func_arg> args;
  };

  // definitions for reflected types and implementations
  std::unordered_map<std::string /*qualified_type*/, type_definition> definitions;
  std::unordered_map<std::string /*qualified_type*/, std::vector<field_data>> reflected_types;
  std::vector<std::string /*qualified_type*/> reflected_implementations;
  std::vector<func_signature> reflected_calls;
};

struct map_matches {
  tl::expected<context, std::string> operator()(
    std::vector<std::variant<matched_reflection, matched_reflected_call>> matches) const noexcept {
    tl::expected<context, std::string> result{tl::in_place};
    for (const auto &m : matches) {
      (void)m;
      // if (const auto *struct_decl =
      //       result.Nodes.getNodeAs<clang::CXXRecordDecl>(context::k_struct_decl)) {
      //   const std::string name = struct_decl->getQualifiedNameAsString();
      //   if (_c.definitions.contains(name))
      //     return;
      //   if (auto d = resolve_definiton(*static_cast<const clang::RecordDecl *>(struct_decl),
      //         *result.SourceManager)) {
      //     _c.definitions[name] = std::move(d).value();
      //   }

      //   return;
      // }

      // const auto *requested_impl_decl =
      // result.Nodes.getNodeAs<clang::CXXRecordDecl>(context::k_impl); const auto *user_type_param
      // =
      //   result.Nodes.getNodeAs<clang::ParmVarDecl>(context::reflected_type::matcher_binding);
      // const auto *serialization_type_param =
      //   result.Nodes.getNodeAs<clang::ParmVarDecl>(context::serialization_type::matcher_binding);

      // if (const auto [found, expected] =
      //       [](const auto *...n) {
      //         return std::pair{(size_t(nullptr != n) + ...), sizeof...(n)};
      //       }(requested_impl_decl, user_type_param, serialization_type_param);
      //     found != expected) {
      //   llvm::errs() << "unexpected case: " << found << '/' << expected << " nodes found\n";
      //   return;
      // }

      // const auto requested_impl_type = resolve_requested_impl_type(*requested_impl_decl);
      // if (!requested_impl_type) {
      //   llvm::errs() << "unexpected error: failed to resolve requested implementation\n";
      //   return;
      // }

      // auto user_type = resolve_reflected_type(*user_type_param, *requested_impl_type);
      // // auto serialization_type =
      // //   resolve_serialization_type(*serialization_type_param, *requested_impl_type);
      // if (!user_type /*|| !serialization_type*/) {
      //   llvm::errs() << (user_type ? "" : "failed to resolve user provided type\n");
      //   // llvm::errs() << (serialization_type ? "" : "failed to resolve serialization type\n");

      //   return;
      // }

      // _c.reflected_types.push_back(std::move(user_type).value());
      // todo: implement in the next iterations
      // _c.serialization_types.push_back(std::move(serialization_type).value());

      // todo: implement

      // const auto func = result.Nodes.getNodeAs<clang::FunctionDecl>(kFuncName);
      // if (!func)
      //   return;

      // // todo: (debug) remove or disable
      // print_func_decl(func);

      // const auto add_func = [this, is_definition =
      //                                  func->hasBody()](context::func_data d) {
      //   is_definition ? _c.definitions.push_back(std::move(d))
      //                 : _c.declarations.push_back(std::move(d));
      // };

      // // todo: not capture this... this is rather unclear
      // const auto get_param_types =
      //     [this, &sm = *result.SourceManager](const auto &params) {
      //       const static auto printing_policy = [] {
      //         clang::PrintingPolicy p{{}};
      //         p.SuppressTagKeyword = true;
      //         p.SuppressScope = false;
      //         p.PrintCanonicalTypes = true;

      //         return p;
      //       }();

      //       const auto get_struct_field_names = [](const clang::ParmVarDecl *p)
      //           -> std::optional<std::vector<std::string>> {
      //         const clang::RecordDecl *rd =
      //             p->getType().getNonReferenceType()->getAsRecordDecl();
      //         if (!rd)
      //           return std::nullopt;

      //         // todo: handle forward delcarations
      //         // todo: more specific logic
      //         if (!rd->isStruct() || !rd->isThisDeclarationADefinition())
      //           return std::nullopt;

      //         std::vector<std::string> names;
      //         // todo: there should be checks for correct podd-like types at
      //         some
      //         // point
      //         for (const clang::FieldDecl *d : rd->fields())
      //           names.push_back(d->getNameAsString());

      //         return {std::move(names)};
      //       };

      //       std::vector<context::func_data::arg_data> args;
      //       args.reserve(params.size());

      //       for (const clang::ParmVarDecl *p : params) {
      //         auto _split = p->getType().getNonReferenceType().split();
      //         _split.Quals.removeCVRQualifiers();
      //         args.push_back({
      //             .cvr_qualified_type = p->getType().getAsString(),
      //             .scoped_type =
      //                 clang::QualType::getAsString(_split, printing_policy),
      //         });
      //         const auto &arg_type = args.back().scoped_type;
      //         if (auto s = _c.structs.find(arg_type); s == _c.structs.cend()) {
      //           auto fnames = get_struct_field_names(p);
      //           if (fnames)
      //             _c.structs.insert(
      //                 {arg_type,
      //                  {
      //                      .declaration_file =
      //                          util::get_declaration_source_file(*p, sm),
      //                      .field_names = std::move(fnames).value(),
      //                  }});
      //         }
      //       }
      //       return args;
      //     };

      // add_func({
      //     .declaration_file = get_declaration_source_file(
      //         *func, *result.SourceManager), // todo: get
      //     .qualified_name = func->getQualifiedNameAsString(),
      //     .return_type = func->getReturnType().getAsString(),
      //     .args = get_param_types(func->parameters()),
      // });
      // todo: implement
    }
    return result;
  };
};

tl::expected<context, std::string> update(context _current, context /*delta*/) noexcept {
  tl::expected<context, std::string> result{std::move(_current)};

  // todo: implement
  return result;
};

struct emit_code_t {
  struct options {
    // todo: options
  };

  struct reflection_data {
    // used to generate `omni::reflected_t<user_type>` specializations
    struct reflected_type {
      // fully namespace-qualified type
      std::string name;
      std::vector<std::string> field_names;
    };

    // list of unique header paths or reflected types' headers
    std::vector<fs::path> refl_includes;

    // list of unique header paths or reflected implementations' headers
    std::vector<fs::path> refl_impl_includes;

    // list of unique reflected types
    std::vector<reflected_type> reflected_types;

    // list of unique reflected call function signatures
    std::vector<context::func_signature> reflected_calls;
  };

  static tl::expected<reflection_data, std::string> prepare_input(context) noexcept {
    tl::expected<reflection_data, std::string> result{tl::in_place};
    // std::unordered_set<std::string> resolved;
    // std::vector<_resolved_type> result;

    // // fixme: condition to stop. Only user-defined structures need to be resolved
    // const auto resolve_fields =
    //   [&resolved, &definitions = i.definitions, &result](auto resolve_fields,
    //     const _resolved_type &rt) -> tl::expected<void, std::string> {
    //   for (const auto &field : rt.definition.fields) {
    //     for (const auto &depends_on_type : field.types_to_reflect) {
    //       if (resolved.contains(depends_on_type))
    //         continue;
    //       const auto d = definitions.find(depends_on_type);
    //       // fixme:
    //       if (d == definitions.cend())
    //         continue; // return tl::unexpected("unresolved definition for type: " +
    //         f.qualified_type);
    //       const auto &rf = result.emplace_back(_resolved_type{
    //         .type =
    //           {
    //             .name = depends_on_type,
    //             .requested_implementation = rt.type.requested_implementation,
    //           },
    //         .definition = d->second,
    //       });
    //       resolved.emplace(rf.type.name);
    //       if (auto res = resolve_fields(resolve_fields, rf); !res)
    //         return res;
    //     }
    //   }

    //   return {};
    // };

    // for (auto &&t : i.user_types) {
    //   const auto d = i.definitions.find(t.name);
    //   // fixme: no way to distinguish which types should be reflected, and which not...
    //   //   at this point definitions will not contain the std definitions, and they are not
    //   needed if (d == i.definitions.cend())
    //     continue; // return tl::unexpected("unresolved definition for type: " + t.name);

    //   const auto &rt = result.emplace_back(_resolved_type{
    //     .type = std::move(t),
    //     .definition = d->second,
    //   });
    //   if (auto res = resolve_fields(resolve_fields, rt); !res)
    //     return res;
    // }
    // todo: implement
    return result;
  }

  tl::expected<void, std::string>
    operator()(options, std::ostream &, const reflection_data &) const;
} const inline emit_code{};

// clang-format off
const char help_message[] =
// todo: document all the parameters
    "\n"
    "-p <build-path> is used to read a compile command database.\n"
    "\n"
    "\tFor example, it can be a CMake build directory in which a file named\n"
    "\tcompile_commands.json exists (use -DCMAKE_EXPORT_COMPILE_COMMANDS=ON\n"
    "\tCMake option to get this output).\n"
    "\n"
    "[<source> ...] optionally specify the paths of source files. These paths are\n"
    "\tlooked up in the compile command database. If the path of a file is\n"
    "\tabsolute, it needs to point into CMake's source tree. If the path is\n"
    "\trelative, the current working directory needs to be in the CMake\n"
    "\tsource tree and the file must be in a subdirectory of the current\n"
    "\tworking directory. \"./\" prefixes in the relative files will be\n"
    "\tautomatically removed, but the rest of a relative path must be a\n"
    "\tsuffix of a path in the compile command database.\n"
    "\tIf no sources are specified, all the files from the compilation database\n"
    "\twill be used.\n";
// clang-format on
} // namespace

int main(int argc, char **argv) {
  namespace cl = llvm::cl;

  cl::extrahelp common_help{help_message};
  cl::OptionCategory option_category{"Generation Options"};

  cl::opt<std::string> compilation_db_path(cl::cat(option_category),
    "p",
    cl::desc("Specify path to `compile_commands.json`"),
    cl::value_desc("path"),
    cl::Required,
    cl::ValueRequired);

  cl::opt<std::string> output_file(cl::cat(option_category),
    "o",
    cl::desc("Specify output filename"),
    cl::value_desc("filename"),
    cl::Required,
    cl::ValueRequired);
  cl::list<std::string> source_paths(cl::cat(option_category),
    cl::desc("[<source>...]"),
    cl::ZeroOrMore,
    cl::Positional);

  cl::list<std::string> excluded_folders(cl::cat(option_category),
    "excluded",
    cl::desc("Specify paths within `compile_commands.json` to ignore"),
    cl::value_desc("path or filename"),
    cl::ZeroOrMore,
    cl::ValueOptional);

  cl::opt<std::string> resource_dir{
    "resource-dir",
    cl::cat(option_category),
    cl::desc("Directory for system clang headers"),
    cl::Hidden,
    // todo: consider adding default value
    cl::Required,
  };

  cl::ResetAllOptionOccurrences();
  cl::HideUnrelatedOptions(option_category);
  std::string _why_do_i_need_this;
  llvm::raw_string_ostream _why_llvm_why_do_you_crash_without_it{_why_do_i_need_this};
  if (!cl::ParseCommandLineOptions(argc,
        argv,
        /*TODO: Overview*/ {},
        &_why_llvm_why_do_you_crash_without_it)) {
    return -1;
  }

  using clang::tooling::CompilationDatabase;
  auto compilation_db =
    [](llvm::StringRef path) -> llvm::Expected<std::unique_ptr<CompilationDatabase>> {
    std::string err;
    auto res = CompilationDatabase::loadFromDirectory(path, err);
    if (res)
      return {std::move(res)};
    // todo: reference the path in the error
    return llvm::make_error<llvm::StringError>(std::move(err), llvm::inconvertibleErrorCode());
  }(compilation_db_path.getValue());

  if (!compilation_db) {
    llvm::errs() << compilation_db.takeError();
    llvm::errs() << compilation_db_path.getValue() << '\n';
    return -1;
  }

  // todo: error for non-path strings
  const auto to_std_paths = [](const auto &strings) {
    std::vector<fs::path> paths;
    paths.reserve(strings.size());
    std::error_code ec{};
    for (const auto &s : strings) {
      paths.push_back(fs::absolute(s, ec).lexically_normal());
      if (ec) {
        throw std::invalid_argument("Invalid path `" + s + "`: " + ec.message());
      }
    }
    return paths;
  };

  // todo: use pipes
  const auto filtered_sources = [&]() -> std::vector<fs::path> {
    using util::sorted;
    using util::filtered;
    const auto is_subpath = [](const fs::path &path, const fs::path &base) {
      const auto mismatch_pair = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
      return mismatch_pair.second == base.end();
    };

    auto _filtered = sorted(std::less{},
      to_std_paths(filtered(util::str::is_empty, compilation_db.get()->getAllFiles())));
    // refactorme: pass `source_paths` as an argument instead of capturing
    if (!source_paths.empty()) {
      // todo: what if the user excludes the `build` directory, but specifies a
      // source file that is expected to be generated there...
      auto specified_sources = sorted(std::less{}, to_std_paths(source_paths));
      std::vector<fs::path> intersected;
      intersected.reserve(specified_sources.size());
      std::set_intersection(specified_sources.cbegin(),
        specified_sources.cend(),
        _filtered.cbegin(),
        _filtered.cend(),
        std::back_inserter(intersected));
      _filtered = std::move(intersected);
    }

    _filtered = filtered(
      [excluded =
          // refactorme: `filter(util::str::is_empty, excluded_folders)`, but
          // `excluded_folders` is `cl::list`, which will fail to compile
        sorted(std::less{}, to_std_paths(excluded_folders)),
        is_subpath](const fs::path &db_path) {
        for (const fs::path &e : excluded) {
          if (is_subpath(db_path, e))
            return true;
        }

        return false;
      },
      std::move(_filtered));

    return _filtered;
  }();

  // llvm doesn't know how to work with std::filesystem::path
  // What a rotten way to die...
  const auto str_sources = [](const std::vector<fs::path> &paths) -> std::vector<std::string> {
    std::vector<std::string> res;
    res.reserve(paths.size());
    for (const auto &p : paths)
      res.push_back(p.string());
    return res;
  }(filtered_sources);

  // todo: remove or disable in release
  // todo: add option for resource-dir to just print it
  std::cout << "-resource-dir=" << resource_dir.getValue() << '\n';
  std::cout << "-p=" << compilation_db_path.getValue() << '\n';
  std::cout << "-o=" << output_file.getValue() << '\n';
  std::cout << "-excluded=[" << llvm::join(excluded_folders, ",") << "]\n";
  std::cout << "sources: " << llvm::join(source_paths, ",") << '\n';

  std::cout << "Filtered files: " << llvm::join(str_sources, "\n") << '\n';

  const auto make_tool_invocation = [](auto f) {
    struct _adapter: clang::tooling::ToolAction {
      _adapter(decltype(f) f): _f(f) {
      }
      decltype(f) _f;

      private:
      bool runInvocation(std::shared_ptr<clang::CompilerInvocation> inv,
        clang::FileManager *files,
        std::shared_ptr<clang::PCHContainerOperations> pch_cont_ops,
        clang::DiagnosticConsumer *diag_cons) override {
        return _f(actions::clang_tool_input{
          .compiler_invocation = inv,
          .files = files,
          .pch_cont_ops = pch_cont_ops,
          .diag_cons = diag_cons,
        });
      }
    };

    return _adapter(f);
  };

  actions::print_files_progress print_progress{
    .total = filtered_sources.size(),
  };
  tl::expected<context, std::string> ctx{tl::in_place};

  const auto tr_unit_pipeline = //
    [&ctx, resource_dir = resource_dir.getValue(), &print_progress](
      actions::clang_tool_input tool_input) {
      using namespace actions;

      configure_compiler_invocation{.resource_dir = resource_dir}(*tool_input.compiler_invocation);
      disable_pch_and_warnings{}(*tool_input.compiler_invocation);

      print_progress(tool_input);

      // parse AST
      std::unique_ptr<ASTUnit> ast =
        ASTUnit::LoadFromCompilerInvocation(tool_input.compiler_invocation,
          tool_input.pch_cont_ops,
          clang::CompilerInstance::createDiagnostics(
            &tool_input.compiler_invocation->getDiagnosticOpts(),
            tool_input.diag_cons,
            /*ShouldOwnClient=*/false),
          tool_input.files);

      if (ast->getDiagnostics().hasUnrecoverableErrorOccurred()) {
        // todo: filename and diagnostics
        ctx = tl::unexpected(std::string("failed to parse AST"));
        return false;
      }
      // refactorme: list of template arguments here will cause a lot of problems, because the order
      // must be specified exactly like in other places
      auto matches = match_reflections_ast<matched_reflection, matched_reflected_call>{}(*ast);
      if (!matches) {
        ctx = tl::unexpected(std::move(matches).error());
        return false;
      }

      auto ctx_delta = map_matches{}(std::move(matches).value());
      if (!ctx_delta) {
        ctx = tl::unexpected(std::move(ctx_delta).error());
        return false;
      }

      assert(ctx);
      ctx = update(std::move(ctx).value(), std::move(ctx_delta).value());
      return bool(ctx);
    };

  // todo: don't use the tool. Should be similar to:
  // ```
  // -- Processing a single source file and updating the context
  // tu_pipeline : context -> src -> context
  // tu_pipeline ctx src =
  // let configured_src = configure_compiler_invocation cli.compilation_db src
  // let _ = print_files_progress configured_src  -- side-effect, logs progress
  // let ast = parse_ast(configured_src)
  // let matches = match_reflections(ast)
  // let context_delta = map_matches(matches)
  // ctx + context_delta  -- update the context with new data
  //
  // -- Folding over all sources to aggregate the context
  // process_all_sources : sources -> context
  // process_all_sources sources =
  // foldl(tu_pipeline, context{}, sources)
  //
  // -- Emitting code based on the final aggregated context
  // emit_generated_code : context -> ()
  // emit_generated_code ctx =
  // emit_code(cli.output_file, ctx)
  //
  // -- The overall program pipeline
  // program : sources -> ()
  // program sources =
  // let final_context = process_all_sources(sources)
  // emit_generated_code(final_context)
  // ```
  clang::tooling::ClangTool tool(*(compilation_db->get()), str_sources);
  // bolnoi ubliudok... this works
  // todo: try to set them in `configure_compiler_invocation`
  tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
    {
      {"-nostdinc++"}, // prevents from picking up on another compiler's C++ std libs
      {"-resource-dir=" + resource_dir.getValue()},
    },
    clang::tooling::ArgumentInsertPosition::BEGIN));

  auto adapted_tool_invocation = make_tool_invocation(std::ref(tr_unit_pipeline));
  if (const int error = tool.run(&adapted_tool_invocation)) {
    assert(!ctx);
    llvm::errs() << fmt::format("Clang tool error {}: {}", error, ctx.error());
    return error;
  }

  auto validated_reflection_data = emit_code_t::prepare_input(std::move(ctx).value());
  if (!validated_reflection_data) {
    llvm::errs() << validated_reflection_data.error();
    return -1;
  }

  fmt::println("Generating file: {}\n", output_file);
  std::ofstream f{std::filesystem::path{output_file.getValue()}, std::ios::binary};
  if (const auto res = emit_code(
        {
          // todo: options
        },
        f,
        *validated_reflection_data);
      !res) {
    llvm::errs() << res.error() << '\n';
    return -1;
  };
  return 0;
}

namespace {

const static auto printing_policy = [] {
  clang::PrintingPolicy p{{}};
  p.SuppressTagKeyword = true;
  p.SuppressScope = false;
  p.PrintCanonicalTypes = true;

  return p;
}();

// todo: take implementation from here
// std::optional<context::implementation_type> resolve_requested_impl_type(
//   const clang::CXXRecordDecl &rd) {
//   if (rd.isStruct()) {
//     // todo: unit test
//     // todo: what about allowing classes with public fields? Clarify the
//     // requirements.
//     llvm::errs() << "Only C++ struct can be serialized/deserialized\n";
//     return std::nullopt;
//   }
//
//   static const auto _printing_policy = [](clang::PrintingPolicy p) {
//     p.SuppressScope = true;
//     return p;
//   }(printing_policy);
//   auto _split = rd.getTypeForDecl()->getCanonicalTypeUnqualified().getNonReferenceType().split();
//   _split.Quals.removeCVRQualifiers();
//   const std::string name = clang::QualType::getAsString(_split, _printing_policy);
//   if (context::k_serialize == name)
//     return context::implementation_type::serialized;
//   if (context::k_deserialize == name)
//     return context::implementation_type::deserialized;
//
//   llvm::errs() << "Unexpected requested implementation type: " << name << "\n";
//   return std::nullopt;
// }

std::string resolve_qualified_name(const clang::QualType &qt) {
  auto _split = qt.getNonReferenceType().split();
  _split.Quals.removeCVRQualifiers();
  return clang::QualType::getAsString(_split, printing_policy);
};

std::vector<std::string> resolve_types_field_depends_on(const clang::FieldDecl &fd) {
  const auto has_value_type = [](const clang::CXXRecordDecl &rd) -> std::optional<clang::QualType> {
    for (const auto *d : rd.decls()) {
      if (const auto *alias_decl = llvm::dyn_cast<clang::TypedefNameDecl>(d);
          alias_decl && alias_decl->getName() == "value_type") {
        return alias_decl->getUnderlyingType();
      }
    }
    return std::nullopt;
  };

  const auto resolve_types = [has_value_type](auto resolve_types,
                               const clang::QualType &qtype,
                               std::vector<std::string> types) -> std::vector<std::string> {
    const clang::CXXRecordDecl *rdecl = qtype->getAsCXXRecordDecl();
    if (!rdecl)
      return types;

    const bool is_std_type = rdecl->isInStdNamespace();
    if (!is_std_type && rdecl->isStruct())
      types.emplace_back(resolve_qualified_name(qtype));

    if (auto value_type = has_value_type(*rdecl))
      return resolve_types(resolve_types, *value_type, std::move(types));

    // naive implementation
    // todo: find a way to support tuple-like and variant-like types
    if (const auto &[type_name, templ_type] =
          std::tuple{
            rdecl->getName(),
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(rdecl),
          };
        is_std_type && ("tuple" == type_name || "variant" == type_name) && templ_type) {
      const auto arg_list = templ_type->getTemplateInstantiationArgs().asArray();
      if (arg_list.size() != 1 || arg_list.front().getKind() != clang::TemplateArgument::Pack) {
        // todo: log, this should not happen
      } else {
        for (const auto &t_arg : arg_list.front().getPackAsArray()) {
          // todo: also check that `t_arg` is a Type
          types = resolve_types(resolve_types, t_arg.getAsType(), std::move(types));
        }
      }
    }

    return types;
  };

  return resolve_types(resolve_types, fd.getType(), {});
}

// todo: take implementation from here
// std::optional<context::type_definition> resolve_definiton(const clang::RecordDecl &rd,
//   const clang::SourceManager &sm) {
//   // todo: unit test for this case, where definition is not available yet
//   if (!rd.isThisDeclarationADefinition())
//     return std::nullopt;
//
//   const std::string source_file = util::get_declaration_source_file(rd, sm);
//   return context::type_definition{
//     .source_file = source_file,
//     .fields =
//       [fields = rd.fields()] {
//         std::vector<context::field_data> v;
//         v.reserve(std::distance(fields.begin(), fields.end()));
//         for (const clang::FieldDecl *d : fields) {
//           v.push_back({
//             .name = d->getNameAsString(),
//             .types_to_reflect = resolve_types_field_depends_on(*d),
//           });
//         }
//         return v;
//       }(),
//     .definition_flags =
//       [&rd, &source_file] {
//         // todo: unit testing
//         //    - ensure that parsing c++ structs initializes correct flags
//         // refactorme: flags initialization here is uglee
//         // todo: I am not sure the flags are correctly deduced
//         using flags_t = context::type_definition_flags;
//         flags_t flags{};
//         if (!rd.hasNameForLinkage())
//           flags = (flags_t)(flags | flags_t::unnamed);
//
//         if (rd.isInLocalScopeForInstantiation())
//           flags = (flags_t)(flags | flags_t::local);
//
//         if (!util::is_header_file(source_file))
//           flags = (flags_t)(flags | flags_t::in_cpp);
//
//         return flags;
//       }(),
//   };
// };

// todo: take implementation from here
// std::optional<context::reflected_type> resolve_reflected_type(const clang::ParmVarDecl &pvd,
//   context::implementation_type irt) {
//   const clang::RecordDecl *rd = pvd.getType().getNonReferenceType()->getAsRecordDecl();
//   if (!rd || !rd->isStruct()) {
//     llvm::errs() << "Unexpected: non-struct user type found\n";
//     return std::nullopt;
//   }
//
//   return context::reflected_type{
//     .name = resolve_qualified_name(pvd.getType()),
//     .requested_implementation = irt,
//   };
// }

// todo: unit test
tl::expected<void, std::string>
  emit_code_t::operator()(options /*opts*/, std::ostream &os, const reflection_data &data) const {
  os << "// This file has been generated by omnirefl tool (todo: timestamp, data, etc)."
        "\n// Do not modify this file manually.\n";

  os << "\n#include <omnirefl/refl.h>";

  // todo: source paths should not be absolute.
  //    use `options::target_include_paths` to resolve relative paths.
  os << "\n\n// Headers of reflected types";
  for (const fs::path &p : data.refl_includes)
    os << fmt::format("\n#include \"{}\"", p.string());

  os << "\n\n// Headers of reflected implementations";
  for (const fs::path &p : data.refl_impl_includes)
    os << fmt::format("\n#include \"{}\"", p.string());

  // todo: write implementation
  os << "\n// Generated specializations for types' reflection";
  os << "\nnamespace {";
  for (const auto &[reflected_type, field_names] : data.reflected_types) {
    // refactorme: use fmt
    // todo: use ctx
    os << fmt::format(
      "\ntemplate <>"
      "\nstruct omni::reflected_t<{}> {{",
      // todo: use fmt to print fields
      reflected_type);
    size_t n = 0;
    const size_t last_field = field_names.size() - 1;
    // refactorme: use fmt
    for (const auto &field : field_names) {
      const std::string_view before = 0 == n ? "" : "\n    ";
      const std::string_view after = last_field != n ? "," : "";
      os << before << "impl::mem_refl{"
         << "&" << reflected_type << "::" << field << ", \"" << field << "\"}" << after;
      ++n;
    }
    os << ");";
  }
  os << "\n} // namespace\n";

  os << "\n// Generated implementations for reflected calls";
  for (const context::func_signature &func_sig : data.reflected_calls) {
    // refactorme: use fmt
    os << "\ntemplate<>"
          "\nvoid omni::reflected_call_t::_impl(";
    size_t n = 0;
    for (const auto &[arg_type, arg_name] : func_sig.args) {
      os << (0 < n++ ? ",\n  " : "");
      os << fmt::format("{} {}", arg_type, arg_name);
    }
    os << ") {"
       << "\n"
       << func_sig.args.front().name << "(";
    for (size_t i = 1; i < func_sig.args.size(); ++i) {
      os << (1 < i ? ", " : "");
      os << func_sig.args[i].name;
    }

    os << ");\n}\n";
  }

  os << '\n';
  return {};
}

} // namespace
