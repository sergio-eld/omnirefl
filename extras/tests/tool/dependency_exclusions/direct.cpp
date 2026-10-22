// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#include <omnirefl/reflected_scope.hpp>

namespace direct_exclusion {

struct record {
  int value;
};

using record_alias = record;

enum class status { ready };

template <typename T>
struct box {
  T value;
};

struct inspect {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    return std::tuple_size<decltype(meta.public_fields())>::value;
  }

  template <typename _M>
  std::size_t operator()(omni::enum_meta_t<_M>) const {
    return 0;
  }
};

} // namespace direct_exclusion

int main() {
  (void)omni::reflected_call(direct_exclusion::inspect{},
    omni::type_t<direct_exclusion::record>{});
  (void)omni::reflected_call(direct_exclusion::inspect{},
    omni::type_t<direct_exclusion::record_alias>{});
  (void)omni::reflected_call(direct_exclusion::inspect{},
    omni::type_t<direct_exclusion::box<double>>{});
  (void)omni::reflected_call(direct_exclusion::inspect{},
    omni::type_t<direct_exclusion::status>{});
#if defined(OMNI_TOOL_RUN)
  omni::tweaks::skip_metadata<direct_exclusion::record>();
  omni::tweaks::skip_metadata<direct_exclusion::box<int>>();
  omni::tweaks::skip_metadata<direct_exclusion::status>();
#endif
}
