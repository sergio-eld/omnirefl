#include <omnirefl/reflected_scope.hpp>

#include <cstddef>
#include <tuple>

namespace {

struct count_fields {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    return std::tuple_size<decltype(meta.accessible_data_members())>::value;
  }
};

struct invoke {
  template <typename Model>
  std::size_t operator()(omni::type_t<Model>) const {
    return omni::reflected_call(count_fields{}, omni::type_t<Model>{});
  }
};

struct model;

std::size_t before_definition() {
  return std::apply(invoke{}, std::make_tuple(omni::type_t<model>{}));
}

struct model {
  int value;
};

// A valid later call through the same specialization must not hide the first.
std::size_t after_definition() {
  return std::apply(invoke{}, std::make_tuple(omni::type_t<model>{}));
}

} // namespace
