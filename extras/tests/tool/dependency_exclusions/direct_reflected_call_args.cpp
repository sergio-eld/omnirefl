// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

namespace direct_reflected_call_args {

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

} // namespace direct_reflected_call_args

// CTest verifies four instrumentation errors; this case must not compile.
TEST(dependency_exclusions, rejects_skipped_immediate_arguments) {
  (void)omni::reflected_call(direct_reflected_call_args::inspect{},
    omni::type_t<direct_reflected_call_args::record>{});
  (void)omni::reflected_call(direct_reflected_call_args::inspect{},
    omni::type_t<direct_reflected_call_args::record_alias>{});
  (void)omni::reflected_call(direct_reflected_call_args::inspect{},
    omni::type_t<direct_reflected_call_args::box<double>>{});
  (void)omni::reflected_call(direct_reflected_call_args::inspect{},
    omni::type_t<direct_reflected_call_args::status>{});
}

#if defined(OMNI_TOOL_RUN)
OMNI_SKIP_METADATA(direct_reflected_call_args::record);
OMNI_SKIP_METADATA(direct_reflected_call_args::box<int>);
OMNI_SKIP_METADATA(direct_reflected_call_args::status);
#endif
