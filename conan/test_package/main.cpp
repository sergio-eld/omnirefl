#include <omnirefl/reflected_scope.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

struct observation {
  unsigned sequence;
  double scale;

  protected:
  unsigned internal_id;
};

struct field_names {
  template <typename _M>
  std::vector<std::string> operator()(omni::record_meta_t<_M> record) const {
    return collect(record.public_fields());
  }

  private:
  template <typename... Fields>
  static std::vector<std::string> collect(const std::tuple<Fields...> &) {
    return {Fields::name()...};
  }
};

int main() {
  const auto names = omni::reflected_call(field_names{},
    omni::type_t<observation>{});
  for (const auto &name : names)
    std::cout << name << '\n';
  return std::vector<std::string>{"sequence", "scale"} == names
    ? EXIT_SUCCESS
    : EXIT_FAILURE;
}
