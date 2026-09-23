#include <omnirefl/serialization/ryml.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

struct endpoint {
  std::string host;
  unsigned port;
};

struct service {
  std::string name;
  endpoint upstream;
  std::vector<std::string> tags;
};

int main() {
  auto result =
    omni::ryml::deserialize(omni::type_t<service>{}, R"({
      "name": "gateway",
      "upstream": {
        "host": "api.example.com",
        "port": 443
      },
      "tags": ["public", "https"]
    })") //
      .transform_error(omni::ryml::render_diagnostics);

  if (!result) {
    std::cerr << result.error() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << omni::ryml::as_json(*result);
}
