// Expected failure: reflected-scope-only record and enum aliases are queried
// without a reflected_call scope.
#include <omnirefl/reflection.hpp>

namespace negative_query_aliases_out_of_scope {

struct record {
  int value;
};

enum class enumeration {
  value,
};

constexpr bool forced_record_meta = 0 < sizeof(omni::record_meta_t<record>);
constexpr bool forced_enum_meta = 0 < sizeof(omni::enum_meta_t<enumeration>);
constexpr bool forced_record_binding =
  0 < sizeof(omni::record_binding_t<record &>);
constexpr bool forced_enum_binding =
  0 < sizeof(omni::enum_binding_t<enumeration &>);

} // namespace negative_query_aliases_out_of_scope
