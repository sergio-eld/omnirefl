#pragma once

namespace omni {
namespace ryml {

inline std::vector<issue::path_segment> mapping_path::materialize() const {
  std::vector<issue::path_segment> segments(this->depth);
  auto path = this;
  for (std::size_t remaining = this->depth; remaining; --remaining) {
    segments[remaining - 1] = {
      /*field=*/path->field,
      /*index=*/path->has_index
        ? compat::optional<std::size_t>{path->index}
        : compat::nullopt,
    };
    path = path->parent;
  }

  return segments;
}

} // namespace ryml
} // namespace omni
