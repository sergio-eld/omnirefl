#include <omnirefl/reflection.hpp>

#include <cstddef>

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

  static constexpr const char *annotation() noexcept {
    return "";
  }

  static constexpr std::size_t index() noexcept {
    return 0;
  }

  static int &value(first_record &r) {
    return r.value;
  }

  static void set_value(first_record &r, int v) {
    r.value = v;
  }
};

static_assert(!omni::is_reflected<first_record>::value,
  "out-of-scope is_reflected query should be rejected by the tool");

constexpr bool forced_meta =
  0 < sizeof(omni::meta_t<first_record>);
constexpr bool forced_binding =
  0 < sizeof(omni::binding_t<second_record &>);
constexpr bool forced_field_meta =
  0 < sizeof(omni::field_meta_t<first_record, field_like>);
constexpr bool forced_field_binding =
  0 < sizeof(omni::field_binding_t<first_record, field_like>);

void reflected_type_query() {
  (void)forced_meta;
  (void)forced_binding;
  (void)forced_field_meta;
  (void)forced_field_binding;
  (void)omni::reflected<second_record>();
}

void reflected_value_query() {
  second_record r{1};
  (void)omni::reflected(r);
}
} // namespace negative_query_multiple
