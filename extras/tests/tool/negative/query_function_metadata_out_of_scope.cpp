// Expected failure: function metadata and bindings are queried outside a
// reflected scope.
#include <omnirefl/reflected_scope.hpp>

namespace negative_query_function_metadata_out_of_scope {

struct record {};

struct value_meta {
  using type = int;
};

struct function_meta {
  using ret_t = value_meta;
  using params_t = std::tuple<>;

  static int (*pointer())();
};

constexpr bool forced_function_meta =
  0 < sizeof(omni::function_meta_t<function_meta>);
constexpr bool forced_function_binding =
  0 < sizeof(omni::function_binding_t<record, function_meta>);
constexpr bool forced_function_param_meta =
  0 < sizeof(omni::function_param_meta_t<value_meta>);
constexpr bool forced_function_return_meta =
  0 < sizeof(omni::function_return_meta_t<value_meta>);

} // namespace negative_query_function_metadata_out_of_scope
