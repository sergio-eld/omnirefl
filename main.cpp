
// todo:
// - ast caching (?)
// - run queries on the ast and output the code

#define STRINGIFY(A) #A

#include <tl/expected.hpp>

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
#include <chrono>
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
#include <unordered_set>
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

// refactorme: use chained library (with currying)
struct sorted {
  template <typename Cmp, typename Container>
  Container operator()(Cmp cmp, Container &&c) const {
    // todo: what if non-const reference?
    Container _s = std::forward<Container>(c);
    std::sort(_s.begin(), _s.end(), cmp);
    return _s;
  }

  template <typename Container>
  Container operator()(Container &&c) const {
    return (*this)(std::less{}, std::forward<Container>(c));
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

struct stop_watch {
  std::chrono::steady_clock::time_point start;
};

[[maybe_unused]] void print_elapsed(stop_watch &sw, std::ostream &os, std::string_view tag) {
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now() - sw.start);
  os << "elapsed: " << elapsed.count() << " (" << tag << ")\n";
  sw.start = std::chrono::steady_clock::now();
}

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

// std::string full_func_signature(const context::func_data &fd) {
//   size_t sz = fd.qualified_name.size() + fd.return_type.size() + 3;
//   for (const auto &a : fd.args)
//     sz += a.cvr_qualified_type.size() + 2; // ", "
//   if (fd.args.size() < 2)
//     sz -= 2; // ", "
//
//   std::string sig;
//   sig.reserve(sz);
//   [&sig](const auto &...s) { (sig.append(s), ...); }(fd.return_type, " ",
//                                                      fd.qualified_name, "(");
//   for (size_t i = 0; i < fd.args.size(); ++i) {
//     if (i)
//       sig.append(", ");
//     sig.append(fd.args[i].cvr_qualified_type);
//   }
//   sig.append(")");
//
//   return sig;
// }

// struct resolve_declarations_t {
//   using func_data = context::func_data;
//   struct result {
//     struct definition_info {
//       std::string declaration_file;
//       std::string func_sig;
//     };
//     std::vector<func_data> declarations;
//     std::vector<definition_info> definitions;
//   };
//   // ad hoc to get unique declarations
//   result operator()(const context &ctx) const;
// } const resolve_declarations{};

namespace actions {

struct aggregated {
  struct input {
    const std::shared_ptr<clang::CompilerInvocation> &compiler_invocation;
    clang::FileManager *files;
    const std::shared_ptr<clang::PCHContainerOperations> &pch_cont_ops;
    clang::DiagnosticConsumer *diag_cons;
  };

  template <typename... A>
  auto operator()(std::tuple<A...> &actions) const noexcept {
    struct _impl: clang::tooling::ToolAction {
      _impl(std::tuple<A...> &actions): _actions(actions) {
      }
      std::tuple<A...> &_actions;

      private:
      bool runInvocation(std::shared_ptr<clang::CompilerInvocation> inv,
        clang::FileManager *files,
        std::shared_ptr<clang::PCHContainerOperations> pch_cont_ops,
        clang::DiagnosticConsumer *diag_cons) override {
        return std::apply(
          [&](auto &...actions) -> bool {
            bool res = true;
            ((res =
                 // todo: don't force this interface upon the actions. Use
                 // `operator()` instead + Context arg
               actions(input{
                 .compiler_invocation = inv,
                 .files = files,
                 .pch_cont_ops = pch_cont_ops,
                 .diag_cons = diag_cons,
               }))
              && ...);
            return res;
          },
          _actions);
      }
    };

    return _impl(actions);
  }
} const inline aggregated{};

struct print_files_progress {
  size_t total;
  size_t processing;
  bool operator()(const aggregated::input &i) {
    // invocation is run on a single file. This is not obvious, but apparently
    // it is how it works
    const auto &sourse_file =
      std::string(i.compiler_invocation->getFrontendOpts().Inputs[0].getFile());
    std::cout << '[' << processing << '/' << total << "] building AST of: " << sourse_file << "\t\r"
              << std::flush;

    return true;
  }
};

template <typename Configure>
struct configure_compiler_invocation {
  Configure c;
  bool operator()(aggregated::input i) const {
    return c(*i.compiler_invocation);
  }
};

struct disable_pch_and_warnings {
  bool operator()(clang::CompilerInvocation &ci) {
    // createInvocationFromCommandLine sets DisableFree.
    ci.getFrontendOpts().DisableFree = false;
    // todo: ifdef based on clang verion
    // ci.getLangOpts()->CommentOpts.ParseAllComments = true;
    // ci.getLangOpts()->RetainCommentsFromSystemHeaders = true;
    // todo: configure the resoulrce dir

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

    return true;
  }
};

// refactorme
template <typename Action>
struct ast_action {
  Action action;
  size_t processed = 0;
  std::function<void(size_t)> n_processing{};

  bool operator()(aggregated::input i) {
    if (n_processing)
      n_processing(processed + 1);

    // todo: go go Matchers
    std::unique_ptr<ASTUnit> ast = ASTUnit::LoadFromCompilerInvocation(i.compiler_invocation,
      i.pch_cont_ops,
      clang::CompilerInstance::createDiagnostics(&i.compiler_invocation->getDiagnosticOpts(),
        i.diag_cons,
        /*ShouldOwnClient=*/false),
      i.files);

    if (!ast)
      return false;

    action(*ast);
    ++processed;
    return true;
  }
};
} // namespace actions

// for each template instantiation of `_impl` need to collect the info:
// User type:
// - definition
//   - file[header|cpp]
//   - type[unnamed|local] (flags set)
//   - full namespace
//   - qualifiers (? can infer now just const ref and ref)
// Data type (next iteration):
// - definition [header]
// - type
struct context {
  static constexpr const char k_namespace[] = "omni";
  static constexpr const char k_impl[] = "_impl";
  static constexpr const char k_arg_0[] = "arg_0";
  static constexpr const char k_arg_1[] = "arg_1";
  static constexpr const char k_serialize[] = "serialize_t";
  static constexpr const char k_deserialize[] = "deserialize_t";
  static constexpr const char k_struct_decl[] = "struct_decl";

  // type for requested implementation. In next iterations should be removed.
  enum implementation_type {
    serialized = 0x1,
    deserialized = 0x1 << 1,
  };

  // flags for definition properties
  enum definition_flags_t {
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

  struct field_data {
    // field name
    std::string name;

    // todo: handle std containers, std::tuple and std::variant
    //   for containers and wrapped types traits can be used, like `value_type`.
    //   however, this might not be a good place: the type itself is of no interest, it is necessary
    //   to detect all the types which may be used here to generate reflection for them
    // fully namespace-qualified type name
    // std::string qualified_type;
    std::vector<std::string> types_to_reflect;
  };

  struct definition_t {
    // path to a file containing the definition (header or .cpp)
    fs::path source_file;
    // todo: field types also have to be resolved. This requires collecting definitions when
    // parsing each unit to avoid reparsing
    std::vector<field_data> fields;

    definition_flags_t definition_flags = none;
  };

  // refactorme: better name
  struct reflected_type {
    // binging string used for ast matchers
    static constexpr const char matcher_binding[] = "user_type";
    // fully namespace-qualified type name
    std::string name;
    implementation_type requested_implementation;
    // todo: remove? Since we have collected all the definitions...
    // std::optional<definition_t> definition;
  };

  // todo: implement in the next iterations
  struct serialization_type {
    //  struct definition_t {
    //    fs::path included;
    //  };
    static constexpr const char matcher_binding[] = "serialization_type";
    // fully namespace-qualified type name
    //   std::string name;
    //   implementation_type impl_requested;
    //   std::optional<definition_t> definition;
  };

  // todo: profile
  std::unordered_map<std::string /*qualified_type*/, definition_t> definitions;
  std::vector<reflected_type> reflected_types;
  // todo: implement in the next iterations
  // std::vector<serialization_type> serialization_types;
};

struct emit_code_t {
  struct input {
    std::vector<context::reflected_type> user_types;
    const std::unordered_map<std::string, context::definition_t> &definitions;
    // todo: options
  };

  tl::expected<void, std::string> operator()(std::ostream &os, input i) const;

  private:
  struct _resolved_type {
    context::reflected_type type;
    context::definition_t definition;
  };
  static void _emit(std::ostream &os, const std::vector<_resolved_type> &types);
} const inline emit_code{};

class match_deserializations: public matchers::MatchFinder::MatchCallback {
  context &_c;
  // refactorme: is it possible to go from event-driven to just collecting the data?
  void run(const matchers::MatchFinder::MatchResult &result) override;

  // todo: remove, this is for debugging purposes
  void print_func_decl(const clang::FunctionDecl *f) {
    std::cout << "FunctionDecl@:" << f << ":" << f->getReturnType().getAsString() << " "
              << f->getQualifiedNameAsString() << "(";

    for (size_t i = 0; i < f->getNumParams(); i++) {
      if (i > 0)
        std::cout << ", ";
      std::cout << clang::QualType::getAsString(f->parameters()[i]->getType().split(),
        clang::PrintingPolicy{{}})
                << "" << f->parameters()[i]->getQualifiedNameAsString();
    }

    std::cout << ")"
              << "   Definition@" << f->getDefinition() << "\n";
  }

  public:
  match_deserializations(context &c): _c(c) {
  }
};

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

  /* todo: use chains...
   * ```
   * auto filtered =
   *   compilation_db.get()->getAllFiles()
   *   >>= filtered << str::is_empty
   *   | to_std_paths
   *   | sorted;
   * ```
   */
  const auto filtered_sources = [&]() -> std::vector<fs::path> {
    using util::sorted;
    using util::filtered;
    const auto is_subpath = [](const fs::path &path, const fs::path &base) {
      const auto mismatch_pair = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
      return mismatch_pair.second == base.end();
    };

    auto _filtered =
      sorted(to_std_paths(filtered(util::str::is_empty, compilation_db.get()->getAllFiles())));
    // refactorme: pass `source_paths` as an argument instead of capturing
    if (!source_paths.empty()) {
      // todo: what if the user excludes the `build` directory, but specifies a
      // source file that is expected to be generated there...
      auto specified_sources = sorted(to_std_paths(source_paths));
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
        sorted(to_std_paths(excluded_folders)),
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

  /*
   * iteration 2
   * todo:
   *    - match all the invocations of (make it one function) a static template
   *      member function `serialize_t::_impl` and `deserialize_t::_impl`
   *    - map all the structs by: has definition && where definition (header,
   *      cpp, local)
   *    - for header-defined (non-local) structs make a template specialization
   *      of `omni::data_fields<T>()` which
   *    - (?) for local and in-cpp definitions make
   * `omni::data_fields<type_index<T>>()`,
   *    - ???
   *    - profit
   */

  // todo: instead of all this stateful mombo-jombo just use a functional `fold`
  // on the AST
  using namespace actions;

  context ctx;
  match_deserializations collector{ctx};
  matchers::MatchFinder finder;

  {
    using namespace clang::ast_matchers;
    // todo: normal formatting...
    // todo: add matcher for `context::kSerialize`. Both functions has 2 arguments, but their
    // positions is different, since the convention is `serialize(from, to)` and
    // `deserialize(from, to)`
    finder.addMatcher(
      // TK_AsIs is needed to include template instantiations
      traverse(clang::TK_AsIs,
        cxxMethodDecl(hasAncestor( //
                        cxxRecordDecl( //
                          hasAncestor(namespaceDecl(hasName(context::k_namespace))),
                          hasName(context::k_deserialize))
                          .bind(context::k_impl)),
          hasName(context::k_impl),
          isTemplateInstantiation(),
          parameterCountIs(2),
          hasParameter(0,
            parmVarDecl() //
                          // refactorme: just use arg_0
              .bind(context::serialization_type::matcher_binding)),
          hasParameter(1,
            parmVarDecl() //
                          // refactorme: just use arg_1
              .bind(context::reflected_type::matcher_binding)))),
      &collector);

    // this seems to be a suboptimal solution, since it matches a ton of definitions...
    finder.addMatcher(traverse(clang::TK_AsIs,
                        // todo: what about templates or template instantiations? Some templates is
                        // impossible to instantiate (local or unnamed types as template parameters)
                        cxxRecordDecl( //
                          unless(isInStdNamespace()),
                          unless(isExpansionInSystemHeader()),
                          isDefinition(),
                          // todo: consider allowing classes
                          isStruct())
                          .bind(context::k_struct_decl)),
      &collector);
  }

  std::tuple tool_action{
    // this should be configured once for each TU, since every TU has a specific set of compile
    // commands in `compile_commands.json`
    // todo: make this work by not using `ClangTool`. For some reason it exposes the invocation for
    // modification, which only has a partial effect
    configure_compiler_invocation{
      .c =
        [&](clang::CompilerInvocation &c) {
          // todo: configure

          // fixme: I have no idea at this point why other compiler's header paths are added here
          std::erase_if(c.getHeaderSearchOpts().UserEntries,
            [](const clang::HeaderSearchOptions::Entry &e) {
              return clang::frontend::IncludeDirGroup::System == e.Group
                && e.Path.find("omnirefl") == e.Path.npos;
            });

          // for some reason this doesn't have any effect if set up here, unlike the paths'
          // modifications below 
          // c.getHeaderSearchOpts().ResourceDir = resource_dir.getValue();

          // todo: path from cli, since it is architecture dependent
          c.getHeaderSearchOpts().AddPath(resource_dir.getValue()
              + "/include/x86_64-unknown-linux-gnu/c++/v1",
            clang::frontend::IncludeDirGroup::System,
            /*IsFramework=*/false,
            /*IgnoreSysRoot=*/false);

          c.getHeaderSearchOpts().AddPath(resource_dir.getValue() + "/include/c++/v1",
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

          return disable_pch_and_warnings{}(c);
        },
    },
    print_files_progress{},
    ast_action{[&finder](ASTUnit &ast) {
      finder.matchAST(ast.getASTContext());
      // todo: (context) sort, resolve, etc.
    }},
  };

  {
    // refactorme: `ast_action` should not be involved here
    auto &[total, processing] = std::get<print_files_progress>(tool_action);
    total = filtered_sources.size();
    std::get</*ast_action*/ 2>(tool_action).n_processing = [&processing](
                                                             size_t n) { processing = n; };
  }

  auto ref = aggregated(tool_action);

  // todo: don't use the tool, since it doesn't allow for parallel computations
  clang::tooling::ClangTool tool(*(compilation_db->get()), str_sources);
  // bolnoi ubliudok... this works
  // todo: try to set them in `configure_compiler_invocation`
  tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
    {
    // for some reason it will only work from here
      {"-resource-dir=" + resource_dir.getValue()},
      // {"-isystem" + resource_dir.getValue() + "/include/c++/v1"},
      // // todo: this should be configured at runtime!
      // {"-isystem" + resource_dir.getValue() + "/include/x86_64-unknown-linux-gnu/c++/v1"},
    },
    clang::tooling::ArgumentInsertPosition::BEGIN));

  // fixme: this actually doesn't fail, even with `fatal error:`
  if (const int error = tool.run(&ref)) {
    llvm::errs() << "Failed to build asts with error: " << error;
    return error;
  }

  std::cout << "Generating file: " << output_file << std::endl;
  std::ofstream f{std::filesystem::path{output_file.getValue()}, std::ios::binary};
  if (const auto res = emit_code(f,
        {
          .user_types = std::move(ctx.reflected_types),
          .definitions = ctx.definitions,
        });
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

std::optional<context::implementation_type> resolve_requested_impl_type(
  const clang::CXXRecordDecl &rd) {
  if (rd.isStruct()) {
    // todo: unit test
    // todo: what about allowing classes with public fields? Clarify the
    // requirements.
    llvm::errs() << "Only C++ struct can be serialized/deserialized\n";
    return std::nullopt;
  }

  static const auto _printing_policy = [](clang::PrintingPolicy p) {
    p.SuppressScope = true;
    return p;
  }(printing_policy);
  auto _split = rd.getTypeForDecl()->getCanonicalTypeUnqualified().getNonReferenceType().split();
  _split.Quals.removeCVRQualifiers();
  const std::string name = clang::QualType::getAsString(_split, _printing_policy);
  if (context::k_serialize == name)
    return context::implementation_type::serialized;
  if (context::k_deserialize == name)
    return context::implementation_type::deserialized;

  llvm::errs() << "Unexpected requested implementation type: " << name << "\n";
  return std::nullopt;
}

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

std::optional<context::definition_t> resolve_definiton(const clang::RecordDecl &rd,
  const clang::SourceManager &sm) {
  // todo: unit test for this case, where definition is not available yet
  if (!rd.isThisDeclarationADefinition())
    return std::nullopt;

  const std::string source_file = util::get_declaration_source_file(rd, sm);
  return context::definition_t{
    .source_file = source_file,
    .fields =
      [fields = rd.fields()] {
        std::vector<context::field_data> v;
        v.reserve(std::distance(fields.begin(), fields.end()));
        for (const clang::FieldDecl *d : fields) {
          v.push_back({
            .name = d->getNameAsString(),
            .types_to_reflect = resolve_types_field_depends_on(*d),
          });
        }
        return v;
      }(),
    .definition_flags =
      [&rd, &source_file] {
        // todo: unit testing
        //    - ensure that parsing c++ structs initializes correct flags
        // refactorme: flags initialization here is uglee
        // todo: I am not sure the flags are correctly deduced
        using flags_t = context::definition_flags_t;
        flags_t flags{};
        if (!rd.hasNameForLinkage())
          flags = (flags_t)(flags | flags_t::unnamed);

        if (rd.isInLocalScopeForInstantiation())
          flags = (flags_t)(flags | flags_t::local);

        if (!util::is_header_file(source_file))
          flags = (flags_t)(flags | flags_t::in_cpp);

        return flags;
      }(),
  };
};

std::optional<context::reflected_type> resolve_reflected_type(const clang::ParmVarDecl &pvd,
  context::implementation_type irt) {
  const clang::RecordDecl *rd = pvd.getType().getNonReferenceType()->getAsRecordDecl();
  if (!rd || !rd->isStruct()) {
    llvm::errs() << "Unexpected: non-struct user type found\n";
    return std::nullopt;
  }

  return context::reflected_type{
    .name = resolve_qualified_name(pvd.getType()),
    .requested_implementation = irt,
  };
}

// todo: () expected
// resoulve data type which the user type is being serialized to or deserialized
// from
// std::optional<context::serialization_type> resolve_serialization_type(const clang::ParmVarDecl
// &pvd,
//   context::implementation_type irt) {
//   return context::serialization_type{
//     .name =
//       [&pvd] {
//         // refactorme: (?) use `rd` instead
//         auto _split = pvd.getType().getNonReferenceType().split();
//         _split.Quals.removeCVRQualifiers();
//         return clang::QualType::getAsString(_split, printing_policy);
//       }(),
//     .impl_requested = irt,
//     // todo: logic for definition initialization. The type can have only a
//     // forward-declaration within this translation unit, in this case we might
//     // try to search the other translation units, given the user type is
//     // definied in a header file...
//     // Maybe I should collect all the definitioins I found for every TU, since
//     // reparsing is unfeasable.
//     .definition = std::nullopt,
//   };
// }

// todo: unit test
tl::expected<void, std::string> emit_code_t::operator()(std::ostream &os, input i) const {
  std::unordered_set<std::string> resolved;
  std::vector<_resolved_type> result;

  // fixme: condition to stop. Only user-defined structures need to be resolved
  const auto resolve_fields =
    [&resolved, &definitions = i.definitions, &result](auto resolve_fields,
      const _resolved_type &rt) -> tl::expected<void, std::string> {
    for (const auto &field : rt.definition.fields) {
      for (const auto &depends_on_type : field.types_to_reflect) {
        if (resolved.contains(depends_on_type))
          continue;
        const auto d = definitions.find(depends_on_type);
        // fixme:
        if (d == definitions.cend())
          continue; // return tl::unexpected("unresolved definition for type: " + f.qualified_type);
        const auto &rf = result.emplace_back(_resolved_type{
          .type =
            {
              .name = depends_on_type,
              .requested_implementation = rt.type.requested_implementation,
            },
          .definition = d->second,
        });
        resolved.emplace(rf.type.name);
        if (auto res = resolve_fields(resolve_fields, rf); !res)
          return res;
      }
    }

    return {};
  };

  for (auto &&t : i.user_types) {
    const auto d = i.definitions.find(t.name);
    // fixme: no way to distinguish which types should be reflected, and which not...
    //   at this point definitions will not contain the std definitions, and they are not needed
    if (d == i.definitions.cend())
      continue; // return tl::unexpected("unresolved definition for type: " + t.name);

    const auto &rt = result.emplace_back(_resolved_type{
      .type = std::move(t),
      .definition = d->second,
    });
    if (auto res = resolve_fields(resolve_fields, rt); !res)
      return res;
  }

  _emit(os, result);
  return {};
}

void emit_code_t::_emit(std::ostream &os, const std::vector<_resolved_type> &types) {
  // todo: assert all the user types have definitions
  os << "// This file has been generated by omnirefl tool (todo: timestamp, data, etc)."
     << "\n// Do not modify this file manually.\n";

  for (std::string_view header :
    // headers for default implementation for iteration 1
    {
      "omnirefl/refl.h",
      "omnirefl/impl_ryml.h",
    }) {
    os << "\n#include <" << header << ">";
  }

  os << "\n\n// Detected user-defined types";
  // refactorme: use chain
  for (const auto &h : [](const auto &types) {
         std::set<std::string> headers;
         for (const auto &[_, definition] : types) {
           // todo: source_file path should not be absolute
           // this should be configurable via input::options
           headers.emplace(definition.source_file.generic_string());
         }
         return headers;
       }(types)) {
    // todo: source_file path should not be absolute
    //   this should be configurable via input::options
    os << "\n#include <" << h << ">";
  }

  os << "\n\n// Generated implementation"
     << "\nnamespace {";
  for (const auto &[t, d] : types) {
    // refactorme: use fmt
    os << "\ntemplate <>"
       << "\nconstexpr auto impl::mem_vars<" << t.name << "> ="
       << "\n  impl::make_reflect(";
    size_t n = 0;
    const size_t last_field = d.fields.size() - 1;
    // refactorme: use fmt
    for (const auto &f : d.fields) {
      const std::string_view before = 0 == n ? "" : "\n    ";
      const std::string_view after = last_field != n ? "," : "";
      os << before << "impl::mem_refl{"
         << "&" << t.name << "::" << f.name << ", \"" << f.name << "\"}" << after;
      ++n;
    }
    os << ");";
  }
  os << "\n} // namespace\n";

  for (const auto &[t, _] : types) {
    // refactorme: use fmt
    // refactorme: deserialized and serialized have mostly the same string template. Only the order
    // and qualifiers of the arguments changes.
    if (context::implementation_type::deserialized & t.requested_implementation) {
      os
        << "\ntemplate<>"
        << "\ntl::expected<void, std::string> omni::deserialize_t::_impl(const ryml::ConstNodeRef &from,"
        << "\n  " << t.name << " &to) {"
        << "\n    return impl::deserialize(from, to);"
        << "\n};";
    }
    // todo: `serialized`
  }

  os << '\n';
}

void match_deserializations::run(const matchers::MatchFinder::MatchResult &result) {
  if (const auto *struct_decl =
        result.Nodes.getNodeAs<clang::CXXRecordDecl>(context::k_struct_decl)) {
    const std::string name = struct_decl->getQualifiedNameAsString();
    if (_c.definitions.contains(name))
      return;
    if (auto d = resolve_definiton(*static_cast<const clang::RecordDecl *>(struct_decl),
          *result.SourceManager)) {
      _c.definitions[name] = std::move(d).value();
    }

    return;
  }

  const auto *requested_impl_decl = result.Nodes.getNodeAs<clang::CXXRecordDecl>(context::k_impl);
  const auto *user_type_param =
    result.Nodes.getNodeAs<clang::ParmVarDecl>(context::reflected_type::matcher_binding);
  const auto *serialization_type_param =
    result.Nodes.getNodeAs<clang::ParmVarDecl>(context::serialization_type::matcher_binding);

  if (const auto [found, expected] =
        [](const auto *...n) {
          return std::pair{(size_t(nullptr != n) + ...), sizeof...(n)};
        }(requested_impl_decl, user_type_param, serialization_type_param);
      found != expected) {
    llvm::errs() << "unexpected case: " << found << '/' << expected << " nodes found\n";
    return;
  }

  const auto requested_impl_type = resolve_requested_impl_type(*requested_impl_decl);
  if (!requested_impl_type) {
    llvm::errs() << "unexpected error: failed to resolve requested implementation\n";
    return;
  }

  auto user_type = resolve_reflected_type(*user_type_param, *requested_impl_type);
  // auto serialization_type =
  //   resolve_serialization_type(*serialization_type_param, *requested_impl_type);
  if (!user_type /*|| !serialization_type*/) {
    llvm::errs() << (user_type ? "" : "failed to resolve user provided type\n");
    // llvm::errs() << (serialization_type ? "" : "failed to resolve serialization type\n");

    return;
  }

  _c.reflected_types.push_back(std::move(user_type).value());
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
}

} // namespace
