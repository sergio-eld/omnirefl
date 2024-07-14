
// Declares clang::SyntaxOnlyAction.
#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
// Declares llvm::cl::extrahelp.
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <llvm/Support/CommandLine.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace clang::ast_matchers;

constexpr const char kFuncName[] = "deserialize";

struct context {
  struct func_data {
    struct arg_data {
      // for function signature generation
      std::string cvr_qualified_type;

      // for referencing member data fields
      std::string scoped_type;
    };

    std::string declaration_file;
    std::string qualified_name;
    std::string return_type;

    // qualified arg names
    std::vector<arg_data> args;
  };

  // todo: handle forward declarations
  struct struct_data {
    std::string declaration_file;
    std::vector<std::string> field_names;
  };

  std::vector<func_data> declarations;
  std::vector<func_data> definitions;
  std::map<std::string /*scoped_type*/, struct_data> structs;
};

std::string full_func_signature(const context::func_data &fd) {
  size_t sz = fd.qualified_name.size() + fd.return_type.size() + 3;
  for (const auto &a : fd.args)
    sz += a.cvr_qualified_type.size() + 2; // ", "
  if (fd.args.size() < 2)
    sz -= 2; // ", "

  std::string sig;
  sig.reserve(sz);
  [&sig](const auto &...s) { (sig.append(s), ...); }(fd.return_type, " ",
                                                     fd.qualified_name, "(");
  for (size_t i = 0; i < fd.args.size(); ++i) {
    if (i)
      sig.append(", ");
    sig.append(fd.args[i].cvr_qualified_type);
  }
  sig.append(")");

  return sig;
}

struct {
  using func_data = context::func_data;
  struct result {
    struct definition_info {
      std::string declaration_file;
      std::string func_sig;
    };
    std::vector<func_data> declarations;
    std::vector<definition_info> definitions;
  };

  // ad hoc to get unique declarations
  result operator()(const context &ctx) const {
    // todo: profile & refactor
    std::unordered_map<std::string /*fullSig*/, const func_data *>
        unique_declarations;
    for (const auto &fd : ctx.declarations)
      unique_declarations.try_emplace(full_func_signature(fd), &fd);

    std::vector<result::definition_info> definitions =
        [](const auto &definitions) {
          std::unordered_map<std::string, const func_data *> unique;
          for (const auto &fd : definitions)
            unique.try_emplace(full_func_signature(fd), &fd);

          std::vector<result::definition_info> result;
          result.reserve(unique.size());

          while (!unique.empty()) {
            auto node = unique.extract(unique.begin());
            result.push_back({
                .declaration_file = node.mapped()->declaration_file,
                .func_sig = std::move(node).key(),
            });
          }

          return result;
        }(ctx.definitions);

    for (const auto &d : definitions)
      unique_declarations.erase(d.func_sig);

    return {.declarations =
                [&unique_declarations] {
                  std::vector<func_data> declarations;
                  declarations.reserve(unique_declarations.size());
                  for (const auto &[k, v] : unique_declarations)
                    declarations.push_back(*v);

                  return declarations;
                }(),
            .definitions = std::move(definitions)};
  }
} const resolve_declarations{};

bool is_header_file(const std::string &filename) {
  llvm::StringRef ext = llvm::sys::path::extension(filename);
  return ext.equals_insensitive(".h") || ext.equals_insensitive(".hpp") ||
         ext.equals_insensitive(".hxx");
}

std::string get_declaration_source_file(const clang::Decl &d,
                                        const clang::SourceManager &sm) {
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

// todo: profie + compare with ast merging
class match_deserializations : public MatchFinder::MatchCallback {
  context &_c;
  void run(const MatchFinder::MatchResult &result) override {
    const auto as_string = [](llvm::StringRef s) -> std::string {
      return {s.data(), s.size()};
    };

    const auto func = result.Nodes.getNodeAs<clang::FunctionDecl>(kFuncName);
    if (!func)
      return;

    // todo: (debug) remove or disable
    print_func_decl(func);

    const auto add_func = [this, is_definition =
                                     func->hasBody()](context::func_data d) {
      is_definition ? _c.definitions.push_back(std::move(d))
                    : _c.declarations.push_back(std::move(d));
    };

    // todo: not capture this... this is rather unclear
    const auto get_param_types = [this, &sm = *result.SourceManager](
                                     const auto &params) {
      const static auto printing_policy = [] {
        clang::PrintingPolicy p{{}};
        p.SuppressTagKeyword = true;
        p.SuppressScope = false;
        p.PrintCanonicalTypes = true;

        return p;
      }();

      const auto get_struct_field_names = [](const clang::ParmVarDecl *p)
          -> std::optional<std::vector<std::string>> {
        const clang::RecordDecl *rd =
            p->getType().getNonReferenceType()->getAsRecordDecl();
        if (!rd)
          return std::nullopt;

        // todo: handle forward delcarations
        // todo: more specific logic
        if (!rd->isStruct() || !rd->isThisDeclarationADefinition())
          return std::nullopt;

        std::vector<std::string> names;
        // todo: there should be checks for correct podd-like types at some
        // point
        for (const clang::FieldDecl *d : rd->fields())
          names.push_back(d->getNameAsString());

        return {std::move(names)};
      };

      std::vector<context::func_data::arg_data> args;
      args.reserve(params.size());

      for (const clang::ParmVarDecl *p : params) {
        auto _split = p->getType().getNonReferenceType().split();
        _split.Quals.removeCVRQualifiers();
        args.push_back({
            .cvr_qualified_type = p->getType().getAsString(),
            .scoped_type =
                clang::QualType::getAsString(_split, printing_policy),
        });
        const auto &arg_type = args.back().scoped_type;
        if (auto s = _c.structs.find(arg_type); s == _c.structs.cend()) {
          auto fnames = get_struct_field_names(p);
          if (fnames)
            _c.structs.insert(
                {arg_type,
                 {
                     .declaration_file = get_declaration_source_file(*p, sm),
                     .field_names = std::move(fnames).value(),
                 }});
        }
      }
      return args;
    };

    add_func({
        .declaration_file = get_declaration_source_file(
            *func, *result.SourceManager), // todo: get
        .qualified_name = func->getQualifiedNameAsString(),
        .return_type = func->getReturnType().getAsString(),
        .args = get_param_types(func->parameters()),
    });
  }

  // todo: remove, this is for debugging purposes
  void print_func_decl(const clang::FunctionDecl *f) {
    std::cout << "FunctionDecl@:" << f << ":"
              << f->getReturnType().getAsString() << " "
              << f->getQualifiedNameAsString() << "(";

    for (int i = 0; i < f->getNumParams(); i++) {
      if (i > 0)
        std::cout << ", ";
      std::cout << clang::QualType::getAsString(
                       f->parameters()[i]->getType().split(),
                       clang::PrintingPolicy{{}})
                << "" << f->parameters()[i]->getQualifiedNameAsString();
    }

    std::cout << ")"
              << "   Definition@" << f->getDefinition() << "\n";
  }

public:
  match_deserializations(context &c) : _c(c) {}
};

} // namespace

int main(int argc, const char **argv) {
  using namespace clang::tooling;

  llvm::cl::OptionCategory option_category("matcher options");
  // todo: remove this file from `getAllFiles()`, since it might not exist
  llvm::cl::opt<std::string> gen_cpp_path{
      "gen", llvm::cl::desc{"Path to generate .cpp file"},
      llvm::cl::value_desc{"path"}, llvm::cl::cat(option_category)};

  // todo: fix segmentation fault when OccurrencesFlag != llvm::cl::OneOrMore
  // now one has to provide a positional argument
  auto options_parser =
      CommonOptionsParser::create(argc, argv, option_category);
  if (!options_parser) {
    llvm::errs() << options_parser.takeError();
    return -1;
  }

  const auto &compilations = options_parser->getCompilations();
  const auto all_sources = [](std::vector<std::string> from_compilation_db,
                              std::string gen_out) {
    std::erase(from_compilation_db, gen_out);
    return from_compilation_db;
  };

  ClangTool tool(
      options_parser->getCompilations(),
      // todo: fix segmentation fault when no positional argument is
      // specified command_line_sources.size() ? command_line_sources
      //                             : compilations.getAllFiles()
      all_sources(compilations.getAllFiles(),
                  std::filesystem::weakly_canonical(gen_cpp_path.getValue())));

  context ctx;
  match_deserializations collector{ctx};
  MatchFinder finder;

  finder.addMatcher(traverse(clang::TK_IgnoreUnlessSpelledInSource,
                             functionDecl(hasName(kFuncName)).bind(kFuncName)),
                    &collector);
  if (const auto err = tool.run(newFrontendActionFactory(&finder).get());
      err != 0) {
    return err;
  }

  const auto print_func = [&ctx](const context::func_data &fd,
                                 std::ostream &os) {
    os << fd.return_type << ' ' << fd.qualified_name << '(';
    for (size_t i = 0; i < fd.args.size(); ++i) {
      if (i)
        os << ", ";

      os << fd.args[i].cvr_qualified_type;
      os << " /*" << fd.args[i].scoped_type << "*/";
    }
    os << ") {\n // todo: definition here\n";
    os << "/* Fields:\n";

    // todo: some logic here
    const std::string sn = fd.args.front().scoped_type;
    for (std::string_view fn : ctx.structs.at(sn).field_names)
      os << "* &" << sn << "::" << fn << ";\n";

    os << "*/\n";
    os << "return \"reflected: " << fd.args[0].scoped_type << "\";";
    os << "\n}\n";
  };

  const auto resolved = resolve_declarations(ctx);
  for (const auto &d : resolved.declarations) {
    std::cout << "// declaration found in '" << d.declaration_file << "'\n";
    print_func(d, std::cout);
    std::cout << '\n';
  }

  std::cout << "Generating file: " << gen_cpp_path << '\n';
  std::ofstream f{std::filesystem::path{gen_cpp_path.getValue()},
                  std::ios::binary};

  [&declarations = resolved.declarations](std::ofstream &f) {
    std::set<std::string> includes;
    for (const auto &d : declarations) {
      if (is_header_file(d.declaration_file))
        includes.emplace(d.declaration_file);
    }

    for (const auto &i : includes)
      // todo: relative path
      f << "#include <" << i << ">" << std::endl;
  }(f);

  f << std::endl << std::endl;

  for (const auto &d : resolved.declarations) {
    print_func(d, f);
    f << std::endl << std::endl;
  }

  f << std::endl;

  return 0;
}
