#include "tool/tool_template.hpp"

#include "tool/util.hpp"

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
