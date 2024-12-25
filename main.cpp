
// todo:
// - ast caching (?)
// - run queries on the ast and output the code

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <sstream>
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
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <numeric>
#include <optional>
#include <stack>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
namespace util {
template <typename Container>
constexpr auto indexed(Container &c) noexcept {
  using _iter_type = decltype(std::begin(c));
  using value_type = std::decay_t<decltype(*std::begin(c))>;
  struct _ref {
    std::size_t index;
    const value_type &value;
  };
  struct _indexed_iter {
    _iter_type _begin;
    _iter_type _end;
    _iter_type _iter = _begin;
    std::size_t _index = 0;

    constexpr _indexed_iter begin() const {
      return {_begin, _end};
    }
    constexpr _indexed_iter end() const {
      return {_begin, _end, _end};
    }
    constexpr _indexed_iter operator++() {
      ++_iter;
      ++_index;
      return *this;
    }
    constexpr bool operator==(const _indexed_iter &other) const noexcept {
      return _iter == other._iter;
    }
    constexpr bool operator!=(const _indexed_iter &other) const noexcept {
      return _iter != other._iter;
    }
    constexpr _ref operator*() const {
      return {_index, *_iter};
    }
  };
  return _indexed_iter{std::begin(c), std::end(c)};
}

template <typename T, typename Format>
struct with_fmt {
  const T &value;
  Format format;
};

template <typename T>
struct _with_fmt_rng {
  using _wrapped_value_type = decltype(*std::declval<const T &>().begin());
  using _wrapped_iterator_type = decltype(std::declval<const T &>().begin());

  constexpr static auto begin(const T &t) noexcept {
    return std::begin(t);
  }
  constexpr static auto end(const T &t) noexcept {
    return std::end(t);
  }
};

template <typename T>
struct _with_fmt_rng<std::reference_wrapper<T>> {
  using _wrapped_value_type = decltype(*std::declval<const T &>().begin());
  using _wrapped_iterator_type = decltype(std::declval<const T &>().begin());

  constexpr static auto begin(std::reference_wrapper<T> t) noexcept {
    return std::begin(t.get());
  }
  constexpr static auto end(std::reference_wrapper<T> t) noexcept {
    return std::end(t.get());
  }
};

template <typename T, typename Format>
struct with_fmt_rng {
  T value;
  Format format;

  // fixme: should not force `const` in `std::reference_wrapper<Format>`, but otherwise doesn't
  // compile..
  using value_type =
    with_fmt<typename _with_fmt_rng<T>::_wrapped_value_type, std::reference_wrapper<const Format>>;
  using _wrapped_iterator_type = typename _with_fmt_rng<T>::_wrapped_iterator_type;

  struct iterator_type {
    using value_type = with_fmt_rng::value_type;

    _wrapped_iterator_type i;
    std::reference_wrapper<const Format> format;
    value_type operator*() const {
      return {.value = *i, .format = format};
    }
    _wrapped_iterator_type operator++() {
      return ++i;
    }
    bool operator==(const iterator_type &other) const {
      return i == other.i;
    }
    bool operator!=(const iterator_type &other) const {
      return i != other.i;
    }
  };

  constexpr iterator_type begin() const {
    return iterator_type{.i = _with_fmt_rng<T>::begin(value), .format = std::ref(format)};
  }
  constexpr iterator_type end() const {
    return iterator_type{.i = _with_fmt_rng<T>::end(value), .format = std::ref(format)};
  }
};

template <typename T, typename Format>
with_fmt(const T &, Format) -> with_fmt<T, Format>;
} // namespace util
} // namespace

template <typename T, typename Format, typename Char>
struct fmt::formatter<util::with_fmt<T, Format>, Char> {
  constexpr auto parse(fmt::format_parse_context &ctx) {
    return ctx.begin();
  }
  constexpr static auto format(const util::with_fmt<T, Format> &uf, fmt::format_context &ctx) {
    return uf.format(uf.value, ctx);
  }
};

namespace {
namespace fs = std::filesystem;
namespace matchers = clang::ast_matchers;

using clang::ASTUnit;

namespace util {

template <typename>
constexpr const std::tuple<> list_types = {};

template <template <typename...> class Template, typename... Types>
constexpr const auto list_types<Template<Types...>> = std::tuple{std::type_identity<Types>{}...};

template <typename T, typename Variant>
constexpr const size_t type_index = std::apply(
  []<typename... Ts>(std::type_identity<Ts>...) {
    constexpr const std::array _table{std::is_same_v<T, Ts>...};
    for (size_t i = 0; i < _table.size(); ++i)
      if (_table[i])
        return i;

    return size_t(-1);
  },
  list_types<Variant>);

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
struct fold_ast_to_matches {
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
  // these are used to determine violated limitations when using the tool
  enum type_definition_flags {
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

  enum ref_type_t {
    ref_none,
    ref_lval,
    ref_rval,
  };

  struct type_definition {
    // refactorme: name is redundant. store it either only here or only in `definitions` map
    // fully namespace-qualified type name
    std::string name;
    fs::path source_file;
    type_definition_flags definition_flags = none;
  };

  struct field_data {
    std::string name;

    // fully namespace-qualified type
    std::string nm_qual_type;

    // todo: I don't know how this can be useful, since there's a simple workaround for accessing
    // bit fields without a member pointer
    // bool is_bitfield;
  };

  struct func_arg {
    // todo: better store qualifiers and the typename separatelly
    // fully cv and namespace-qualified type
    std::string cvr_qualified_type;
    std::string nm_qual_type;
    bool is_const : 1;
    ref_type_t ref_type;
  };

  struct func_signature {
    std::vector<func_arg> args;
  };

  // todo: profile and optimize
  // definitions for reflected types and implementations
  std::unordered_map<std::string /*namespace_qualified_type*/, type_definition> definitions;

  // todo: is it possible to have unresolved reflected types between ASTs? as of this writing - no,
  // because we assume that forward declarations are not allowed. however, if intermediate forward
  // declarations are allowed (defined in different AST), we should add a container to track them
  std::unordered_map<std::string /*namespace_qualified_type*/, std::vector<field_data>>
    reflected_types;

  std::set<std::string /*namespace_qualified_type*/> reflected_implementations;
  std::vector<func_signature> reflected_calls;

  // todo: profile and optimize with flat_set
  // unique list of std headers. used for reflecting std containers and aggregated types like
  // `std::tuple`, `std::variant`, `std::optional` and any other standard type that wraps a
  // reflected type and defines `type` or `value_type` trait
  std::set<std::string> std_includes;
};

// because stupid dap-ui doesn't show unordered_maps
void print_debug(const context &ctx) {
  fmt::println("debug: reflected_implementations: {}", ctx.reflected_implementations.size());
  for (const auto &i : ctx.reflected_implementations)
    fmt::println("  {}", i);
  fmt::println("debug: reflected_types: {}", ctx.reflected_types.size());
  for (const auto &i : ctx.reflected_types)
    fmt::println("  {}", i.first);
}

struct fold_matches_to_context {
  using result_type = tl::expected<context, std::string>;

  const ASTUnit &ast;

  result_type operator()(
    std::vector<std::variant<matched_reflection, matched_reflected_call>> matches) const noexcept {
    result_type r{tl::in_place};
    for (const auto &m : matches) {
      // todo: better type matching api
      using var_type = std::decay_t<decltype(m)>;
      switch (m.index()) {
      case util::type_index<matched_reflection, var_type>: {
        const clang::ClassTemplateSpecializationDecl &template_decl =
          *std::get<util::type_index<matched_reflection, var_type>>(m).node;
        const std::string_view detail_struct_name = template_decl.getName();
        const auto &template_args_list = template_decl.getTemplateArgs();

        // todo: error if template_args_list is empty
        const clang::TemplateArgument &first_arg = template_args_list.get(0);
        if (clang::TemplateArgument::ArgKind::Type != first_arg.getKind()) {
          return tl::unexpected(
            fmt::format("non-type template argument `0` of {}", detail_struct_name));
        }
        const clang::Type &first_arg_type = *first_arg.getAsType().getTypePtr();
        // todo: handle checks for `clang::Type::is...`
        // todo: `hasDefinition` - at this point (as of this writing) forward declarations are not
        //       allowed
        if (!first_arg_type.isStructureOrClassType()) {
          return tl::unexpected(fmt::format("unsupported type {} in {}",
            // fixme: `getTypeClassName` doesn't do what I thought it does
            first_arg_type.getTypeClassName(),
            detail_struct_name));
        }

        // omni::detail::_reflected_impl<T>
        if ("_reflected_impl" == detail_struct_name) {
          // type declaration of <T>
          const clang::CXXRecordDecl &rdecl = *first_arg_type.getAsCXXRecordDecl();
          // as of this writing there's a "fixme" to remove `std::string`, but it is not clear
          // whether a new type will be used or another function should be called
          const std::string nm_qual_type = rdecl.getQualifiedNameAsString();
          // fixme: do not use definitions, because reflected types also use them, but field info
          // should be collected only for reflected types
          if (r->definitions.contains(nm_qual_type))
            continue;

          r->definitions[nm_qual_type] =
            resolve_definition(nm_qual_type, rdecl, ast.getSourceManager());
          r->reflected_implementations.emplace(nm_qual_type);
          // todo: should we allow wrapped types (using value_type trait)?
          continue;
        }

        // omni::detail::_reflected_type<T>
        if ("_reflected_type" == detail_struct_name) {
          // type declaration of <T>
          const clang::CXXRecordDecl &first_arg_decl = *first_arg_type.getAsCXXRecordDecl();
          const std::string first_arg_nm_qual_type = first_arg_decl.getQualifiedNameAsString();
          if (r->reflected_types.contains(first_arg_nm_qual_type))
            continue;

          // recursivelly collected types (including self)
          const std::vector<const clang::RecordDecl *> types =
            dfs_fold_reflected_types(first_arg_decl, r->reflected_types);

          for (const clang::RecordDecl *_rdecl : types) {
            const auto &rdecl = *_rdecl;
            const std::string nm_qual_type = rdecl.getQualifiedNameAsString();

            if (rdecl.isInStdNamespace()) {
              // todo: std include path should not be absolute
              r->std_includes.emplace(
                util::get_declaration_source_file(rdecl, ast.getSourceManager()));
            } else {
              assert(!r->definitions.contains(nm_qual_type));
              r->definitions[nm_qual_type] =
                resolve_definition(nm_qual_type, rdecl, ast.getSourceManager());
            }

            // refactorme: this code is duplicated when collecting types, but whatever
            r->reflected_types.emplace(nm_qual_type,
              [](const auto &_fields) -> std::vector<context::field_data> {
                std::vector<context::field_data> r;
                for (const clang::FieldDecl *fd : _fields) {
                  // todo: logging for skipped fields, since we are not reporting them as errors
                  // todo: checks that would prevent the field from being reflected (uniouns,
                  // bitfields, what else?)
                  if (clang::AccessSpecifier::AS_public != fd->getAccess())
                    continue;
                  r.push_back({
                    .name = fd->getNameAsString(),
                    .nm_qual_type = fd->getQualifiedNameAsString(),
                  });
                }
                return r;
              }(rdecl.fields()));
          }

          continue;
        }

        continue;
      }
      case util::type_index<matched_reflected_call, var_type>: {
        const clang::CXXMethodDecl &refl_call_decl =
          *std::get<util::type_index<matched_reflected_call, var_type>>(m).node;
        context::func_signature &refl_call = r->reflected_calls.emplace_back();
        refl_call.args.reserve(refl_call_decl.parameters().size());

        for (const clang::ParmVarDecl *parm_decl : refl_call_decl.parameters()) {
          const auto &[type, is_const, ref_type] = //
                                                   // refactorme: it can return `context::func_arg`
            [](const clang::QualType &q) {
              struct _r {
                const clang::Type &type;
                bool is_const;
                context::ref_type_t ref_type;
              };
              if (q->isLValueReferenceType()) {
                return _r{
                  .type = *q->getPointeeType().getTypePtr(),
                  .is_const = q->getPointeeType().isConstQualified(),
                  .ref_type = context::ref_type_t::ref_lval,
                };
              }
              if (q->isRValueReferenceType()) {
                return _r{
                  .type = *q->getPointeeType().getTypePtr(),
                  .is_const = q->getPointeeType().isConstQualified(),
                  .ref_type = context::ref_type_t::ref_rval,
                };
              }
              return _r{
                .type = *q,
                .is_const = q.isConstQualified(),
                .ref_type = context::ref_type_t::ref_none,
              };
            }(parm_decl->getType());
          // todo: validate that the type is not a forward declaration, since they are not supported
          // at this point

          const static auto printing_policy = [] {
            clang::PrintingPolicy p{{}};
            p.SuppressTagKeyword = true;
            p.SuppressScope = false;
            p.PrintCanonicalTypes = true;

            return p;
          }();
          refl_call.args.push_back({
            // todo: remove
            .cvr_qualified_type = parm_decl->getType().getAsString(),
            // fixme: it still prints `struct`
            // refactorme: use `type`
            .nm_qual_type = clang::QualType::getAsString(
              [](clang::SplitQualType q) {
                q.Quals.removeCVRQualifiers();
                return q;
              }(parm_decl->getType().getNonReferenceType().split()),
              printing_policy),
            .is_const = is_const,
            .ref_type = ref_type,
          });
          const auto &nm_qual_type = refl_call.args.back().nm_qual_type;

          // fixme: what about unions, built-in arrays?
          if (!type.isStructureOrClassType())
            continue;
          const clang::RecordDecl &rd = *type.getAsRecordDecl();

          if (rd.isInStdNamespace()) {
            // todo: add std include
          }
          if (!r->definitions.contains(nm_qual_type))
            r->definitions[nm_qual_type] =
              resolve_definition(nm_qual_type, rd, ast.getSourceManager());
        }
        continue;
      }

      default:
        continue;
      }
    }
    return r;
  };

  private:
  static context::type_definition resolve_definition(
    // namespace-qualified typename is needed before the call to this function to check whether
    // the definition has been already resolved
    std::string nm_qual_type,
    const clang::RecordDecl &rd,
    const clang::SourceManager &sm) noexcept {
    using td_flags = context::type_definition_flags;
    std::string source_file = util::get_declaration_source_file(rd, sm);
    const td_flags td_unnamed = td_flags::none; // todo:
    const td_flags td_local = td_flags::none; // todo:
    const td_flags td_in_cpp =
      util::is_header_file(source_file) ? td_flags::none : td_flags::in_cpp;

    return {
      .name = std::move(nm_qual_type),
      .source_file = std::move(source_file),
      .definition_flags = td_flags(td_unnamed | td_local | td_in_cpp),
    };
  };

  // types of interest for recursive reflection:
  //   - value_type: std containers, std wrappers, std::optional
  //   - key_type: std containers
  //   - type: std::reference_wrapper
  struct _member_typedef_decl {
    std::string_view name;
    clang::QualType qual_type;
  };

  static std::vector<_member_typedef_decl> member_typedefs(const clang::RecordDecl &rd) noexcept {
    // todo: use `filter`
    std::vector<_member_typedef_decl> r;
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

  /// fold AST from root to a vector of unique declarations that are not already present in
  /// `reflected_types_map`
  static std::vector<const clang::RecordDecl *>
    dfs_fold_reflected_types(const clang::CXXRecordDecl &root, const auto &reflected_types_map) {
    std::set<const clang::RecordDecl *> visited;
    std::stack<const clang::RecordDecl *> stack;
    stack.push(&root);

    while (!stack.empty()) {
      const clang::RecordDecl *cur = stack.top();
      stack.pop();
      const std::string _debug_nm_qual_type = cur->getQualifiedNameAsString();
      if (visited.contains(cur) || reflected_types_map.contains(cur->getQualifiedNameAsString()))
        continue;
      // not generating reflection info for std types, at least for now, at least here...
      if (!cur->isInStdNamespace())
        visited.emplace(cur);

      // we allow recursive reflection via certain member typedefs
      for (const _member_typedef_decl &md : util::filtered(
             [](const _member_typedef_decl &m) -> bool {
               const static std::set<std::string_view> aliases{{
                 "key_type",
                 "value_type",
                 "value",
               }};
               return !aliases.contains(m.name);
             },
             member_typedefs(*cur))) {
        if (md.qual_type->isStructureOrClassType())
          stack.push(md.qual_type->getAsRecordDecl());
      }
      // fixme: what about unions, built-in arrays?

      // ad hoc solution. in c++ code template trait types can be used, but I don't know if
      // it is possible to get from parsed ast
      // I could, however, capture such types in `omni::detail`...
      if (clang::Decl::ClassTemplateSpecialization == cur->getKind() &&
          [](std::string_view name) -> bool { //
            return "tuple" == name || "variant" == name;
          }(cur->getName())) {
        const auto arg_list = clang::cast<clang::ClassTemplateSpecializationDecl>(cur)
                                ->getTemplateInstantiationArgs()
                                .asArray();
        if (1 != arg_list.size() || clang::TemplateArgument::Pack != arg_list.front().getKind()) {
          // todo: log? this should not happen
        } else {
          for (const clang::TemplateArgument &t_arg : arg_list.front().getPackAsArray()) {
            // not a type pack
            if (clang::TemplateArgument::Type != t_arg.getKind())
              break;

            const clang::QualType qt = t_arg.getAsType();
            if (qt->isRecordType())
              stack.push(qt->getAsRecordDecl());
          }
        }
      }

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
        stack.push(clang::cast<clang::RecordDecl>(qt->getAsRecordDecl()));
      }
    }

    return {visited.cbegin(), visited.cend()};
  };
};

tl::expected<context, std::string> update(context _current, context delta) noexcept {
  tl::expected<context, std::string> r{std::move(_current)};
  fmt::println("current:");
  print_debug(*r);
  fmt::println("delta:");
  print_debug(delta);

  // in-place utility
  const auto merge_with_conflicts_check =
    []<typename K, typename T>(std::unordered_map<K, T> first,
      std::unordered_map<K, T> second,
      // (const K&, const T&, const T&) -> std::optional<std::string> error
      auto cmp) -> tl::expected<std::unordered_map<K, T>, std::string> {
    for (auto node = second.begin(); node != second.end();) {
      auto &&[k, v] = *node;
      const auto found = first.find(k);
      if (found == first.cend()) {
        first.insert(second.extract(node++));
        continue;
      }
      if (auto err = cmp(k, v, found->second))
        return tl::unexpected(std::move(err).value());
      ++node;
    }
    return {std::move(first)};
  };

  if (auto merged = merge_with_conflicts_check(std::move(r->definitions),
        std::move(delta.definitions),
        [](std::string_view qual_typename,
          const context::type_definition &lhs,
          const context::type_definition &rhs) -> std::optional<std::string> {
          if (lhs.source_file != rhs.source_file) {
            return fmt::format("found different locations for type {} definition: {}, {}",
              qual_typename,
              lhs.source_file.string(),
              rhs.source_file.string());
          }
          if (lhs.definition_flags != rhs.definition_flags) {
            return fmt::format("found different definition flags for type {} definition: {}, {}",
              qual_typename,
              // todo: stringify
              int(lhs.definition_flags),
              int(rhs.definition_flags));
          }
          return std::nullopt;
        })) {
    r->definitions = std::move(merged).value();
  } else
    return tl::unexpected(std::move(merged).error());

  if (auto merged = merge_with_conflicts_check(std::move(r->reflected_types),
        std::move(delta.reflected_types),
        [](std::string_view qual_typename,
          const std::vector<context::field_data> &lhs,
          const std::vector<context::field_data> &rhs) -> std::optional<std::string> {
          if (!std::equal(lhs.cbegin(),
                lhs.cend(),
                rhs.cbegin(),
                [](const context::field_data &lhs, const context::field_data &rhs) -> bool {
                  return lhs.nm_qual_type == rhs.nm_qual_type;
                })) {
            // todo: print detailed info on fields that differ
            return fmt::format("found different data member types for type {}", qual_typename);
          }
          return std::nullopt;
        })) {
    r->reflected_types = std::move(merged).value();
  } else
    return tl::unexpected(std::move(merged).error());

  r->reflected_implementations.merge(std::move(delta.reflected_implementations));

  r->reflected_calls.reserve(r->reflected_calls.size() + delta.reflected_calls.size());
  r->reflected_calls.insert(r->reflected_calls.end(),
    std::make_move_iterator(delta.reflected_calls.begin()),
    std::make_move_iterator(delta.reflected_calls.end()));

  r->std_includes.merge(std::move(delta.std_includes));

  fmt::println("current updated:");
  print_debug(*r);
  return r;
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

    // todo: profile and optimize (std::set -> std::vector)
    // list of unique header paths (non-reflection)
    std::set<std::string> includes;

    // list of unique header paths or reflected types' headers
    std::set<fs::path> refl_includes;

    // list of unique header paths or reflected implementations' headers
    std::set<std::string> refl_impl_includes;

    // list of unique reflected types
    std::set<reflected_type,
      decltype([](const reflected_type &lhs, const reflected_type &rhs) -> bool {
        // types are unique
        return lhs.name < rhs.name;
      })>
      reflected_types;

    // list of unique reflected call function signatures
    std::vector<context::func_signature> reflected_calls;
  };

  static tl::expected<reflection_data, std::string> prepare_input(context ctx) noexcept {
    tl::expected<reflection_data, std::string> r{tl::in_place};
    // todo: validate input

    r->includes = {std::make_move_iterator(ctx.std_includes.begin()),
      std::make_move_iterator(ctx.std_includes.end())};

    for (const auto &[nm_qual_type, fields] : ctx.reflected_types) {
      const auto definition = ctx.definitions.find(nm_qual_type);
      if (definition == ctx.definitions.cend())
        return tl::unexpected(fmt::format("no definition for reflected type {}", nm_qual_type));

      r->refl_includes.emplace(definition->second.source_file);
      r->reflected_types.insert({
        .name = nm_qual_type,
        .field_names =
          [](const std::vector<context::field_data> &fields) -> std::vector<std::string> {
          std::vector<std::string> r;
          r.reserve(fields.size());
          std::transform(fields.cbegin(),
            fields.cend(),
            std::back_inserter(r),
            [](const context::field_data &fd) { return fd.name; });
          return r;
        }(fields),
      });
    }

    for (const auto &nm_qual_type : ctx.reflected_implementations) {
      const auto definition = ctx.definitions.find(nm_qual_type);
      if (definition == ctx.definitions.cend())
        return tl::unexpected(
          fmt::format("no definition for reflected implementation type {}", nm_qual_type));
      const auto &sf = definition->second.source_file;
      if (!r->refl_includes.contains(sf))
        r->refl_impl_includes.emplace(sf);
    }

    std::set unique_func_signatures{std::make_move_iterator(ctx.reflected_calls.begin()),
      std::make_move_iterator(ctx.reflected_calls.end()),
      [](const context::func_signature &lhs, const context::func_signature &rhs) -> bool {
        // because c++ std is UGLEEEEEEE
        return std::lexicographical_compare(lhs.args.cbegin(),
          lhs.args.cend(),
          rhs.args.cbegin(),
          rhs.args.cend(),
          [](const context::func_arg &lhs, const context::func_arg &rhs) -> bool {
            return lhs.cvr_qualified_type < rhs.cvr_qualified_type;
          });
      }};

    r->reflected_calls = {std::make_move_iterator(unique_func_signatures.begin()),
      std::make_move_iterator(unique_func_signatures.end())};

    return r;
  }

  tl::expected<void, std::string>
    operator()(options, std::ostream &os, const reflection_data &data) const {
    os << "// This file has been generated by omnirefl tool (todo: timestamp, data, etc)."
          "\n// Do not modify this file manually.\n";

    os << "\n#include <omnirefl/refl.h>"
          "\n"
          "\n#include <tuple>"
          "\n#include <utility>";

    // todo: source paths should not be absolute.
    //    use `options::target_include_paths` to resolve relative paths.
    os << "\n\n// Headers of reflected types";
    for (const fs::path &p : data.refl_includes)
      os << fmt::format("\n#include \"{}\"", p.string());

    os << "\n\n// Headers of reflected implementations";
    for (const auto &p : data.refl_impl_includes)
      os << fmt::format("\n#include \"{}\"", p);

    os << "\n\n// Other headers";
    for (const auto &p : data.includes)
      os << fmt::format("\n#include <{}>", p);

    os << "\n\n// Generated specializations for types' reflection";
    os << "\n#ifndef OMNI_DEFINE_NAME_FUNC"
          "\n#  define OMNI_DEFINE_NAME_FUNC(STR) \\"
          "\nconstexpr static auto name() noexcept -> const char(&)[sizeof(STR)] { return STR; }"
          "\n#endif\n";
    for (const auto &refl_type : data.reflected_types) {
      os << fmt::format(
        "\ntemplate <>"
        "\nstruct omni::reflected_t<{type_name}>{{"
        "\n  using type = {type_name};"
        "\n"
        "\n  // fields meta types declarations"
        "{field_decls}"
        "\n"
        "\n  using fields_t = std::tuple<"
        "\n{field_names}"
        "\n  >;"
        "\n}};\n",
        fmt::arg("type_name", refl_type.name),
        fmt::arg("field_decls",
          fmt::join(
            util::with_fmt_rng{
              .value = std::cref(refl_type.field_names),
              .format =
                [](const auto &field_name, fmt::format_context &ctx) {
                  return fmt::format_to(ctx.out(),
                    "\n  struct {field_name}_t {{"
                    "\n    OMNI_DEFINE_NAME_FUNC(\"{field_name}\")"
                    "\n    constexpr static auto get_value(const type &t) noexcept"
                    "\n    -> const decltype(t.{field_name})& {{"
                    "\n      return t.{field_name};"
                    "\n    }}"
                    "\n"
                    "\n    template <typename V>"
                    "\n    void set_value(type &t, V &&v) {{"
                    "\n      t.{field_name} = std::forward<V>(v);"
                    "\n    }}"
                    "\n  }} constexpr static {field_name}{{}};",
                    fmt::arg("field_name", field_name));
                },
            },
            "\n")),
        fmt::arg("field_names",
          fmt::join(
            util::with_fmt_rng{
              .value = std::cref(refl_type.field_names),
              .format =
                [](const auto &field_name, fmt::format_context &ctx) {
                  return fmt::format_to(ctx.out(),
                    "    {field_name}_t",
                    fmt::arg("field_name", field_name));
                },
            },
            ",\n")));
    }
    os << "\n#undef OMNI_DEFINE_NAME_FUNC\n";

    // todo: write implementation
    os << "\n// Generated implementations for reflected calls";
    for (const context::func_signature &func_sig : data.reflected_calls) {
      // refactorme: this formatted printing doesn't really look cleaner than handwritten loops
      // fixme: otherwise lambda does not compile
      size_t arg_index = 0;
      os << fmt::format(
        "\ntemplate<>"
        "\nvoid omni::reflected_call_t::_call_impl("
        "\n  {params}) {{"
        "\n  {invoke}({args});"
        "\n}}",
        // refactorme: consider using fold to a substring instead
        fmt::arg("params",
          fmt::join(
            util::with_fmt_rng{
              // fixme: .value = util::indexed(fund_sig.args),
              .value = std::cref(func_sig.args),
              .format =
                [&arg_index](const context::func_arg &f_arg, fmt::format_context &ctx) {
                  std::string param_name = 0 == arg_index //
                    ? std::string("impl")
                    : std::to_string(arg_index);
                  ++arg_index;
                  return fmt::format_to(ctx.out(),
                    "{nm_qual_type} {const}{ref}_{param_name}",
                    fmt::arg("nm_qual_type", f_arg.nm_qual_type),
                    fmt::arg("const", f_arg.is_const ? "const" : ""),
                    fmt::arg("ref",
                      [](context::ref_type_t r) -> std::string_view {
                        switch (r) {
                        case context::ref_type_t::ref_lval:
                          return "&";
                        case context::ref_type_t::ref_rval:
                          return "&&";
                        case context::ref_none:
                          return "";
                        }
                        return "/*unresolved_reference_type*/";
                      }(f_arg.ref_type)),
                    fmt::arg("param_name", std::move(param_name)));
                },
            },
            ",\n  ")),
        // refactorme: `util::indexed(util::sliced({1}, func_sig.args))`
        fmt::arg("invoke",
          [](const context::func_arg &arg_impl) -> std::string_view {
            return context::ref_type_t::ref_rval == arg_impl.ref_type //
              ? "std::move(_impl)"
              : "_impl";
          }(func_sig.args.front())),
        // refactorme: this is outrigth ugly
        fmt::arg("args", [](auto begin, auto end) -> std::string {
          if (begin == end)
            return "";

          std::stringstream ss;
          size_t i = 1;
          if (context::ref_type_t::ref_rval == begin->ref_type)
            ss << "std::move(_" << i << ")";
          else
            ss << "_" << i;

          while (++begin != end) {
            ++i;
            if (context::ref_type_t::ref_rval == begin->ref_type)
              ss << ", std::move(_" << i << ")";
            else
              ss << ", _" << i;
          }
          return ss.str();
        }(std::next(func_sig.args.cbegin()), func_sig.args.cend())));
    }

    os << '\n';
    return {};
  }
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
      auto matches = fold_ast_to_matches<matched_reflection, matched_reflected_call>{}(*ast);
      if (!matches) {
        ctx = tl::unexpected(std::move(matches).error());
        return false;
      }

      auto ctx_delta = fold_matches_to_context{.ast = *ast}(std::move(matches).value());
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

std::string resolve_qualified_name(const clang::QualType &qt) {
  auto _split = qt.getNonReferenceType().split();
  _split.Quals.removeCVRQualifiers();
  return clang::QualType::getAsString(_split, printing_policy);
};

} // namespace
