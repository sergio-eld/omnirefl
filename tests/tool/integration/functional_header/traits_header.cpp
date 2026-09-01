#include <omnirefl/traits.hpp>

template <typename>
struct type_template;

static_assert(omni::traits::is<int, int>(),
  "the traits header must compare types when included directly");
static_assert(omni::traits::is<type_template, type_template>(),
  "the traits header must compare templates when included directly");
