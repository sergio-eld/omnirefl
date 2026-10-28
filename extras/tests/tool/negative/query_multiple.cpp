// Expected failure: reflection metadata and bindings are queried repeatedly
// outside a reflected scope.
#include <omnirefl/reflected_scope.hpp>

namespace negative_query_multiple {
struct first_record {
  int value;
};

struct second_record {
  int value;
};

struct field_like {
  using type = int;

  static constexpr const char *name() noexcept {
    return "value";
  }

  static constexpr const char *type_name() noexcept {
    return "int";
  }

  static constexpr const char *documentation() noexcept {
    return "";
  }

  static int &value(first_record &r) {
    return r.value;
  }

  static void set_value(first_record &r, int v) {
    r.value = v;
  }
};

constexpr bool forced_meta = 0 < sizeof(omni::meta_t<first_record>);
constexpr bool forced_binding = 0 < sizeof(omni::binding_t<second_record &>);
constexpr bool forced_field_meta =
  0 < sizeof(omni::data_member_meta_t<field_like>);
constexpr bool forced_field_binding =
  0 < sizeof(omni::data_member_binding_t<first_record, field_like>);

void use_queries() {
  (void)forced_meta;
  (void)forced_binding;
  (void)forced_field_meta;
  (void)forced_field_binding;
}
} // namespace negative_query_multiple
