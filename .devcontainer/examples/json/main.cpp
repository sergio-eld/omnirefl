/** JSON/YAML sandbox: serialize, deserialize, and print rejected input.
 * Defaults to C++20; change `CMAKE_CXX_STANDARD` in `CMakeLists.txt` to try
 * C++11. The guards keep both initialization and type tags compatible.
 * For more examples, see `extensions/serialization/guide/main.cpp` in extras.
 */
#include <omnirefl/serialization/ryml.hpp>

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
#if 202002L <= OMNI_CPLUSPLUS
  const service source{
    .name = "gateway",
    .upstream = {.host = "api.example.com", .port = 443},
    .tags = {"public", "https"},
  };
#else
  const service source{
    "gateway", {"api.example.com", 443}, {"public", "https"}};
#endif

  std::cout << "JSON:\n" << omni::ryml::as_json(source)
    << "\n\nYAML:\n" << omni::ryml::as_yaml(source) << '\n';

  const std::string document = R"(
{
  "name": "gateway",
  "upstream": {"host": "localhost", "port": 8080},
  "tags": ["local"]
}
)";

  // C++17 adds CTAD; the older form deduces the result with `auto`.
#if 201703L <= OMNI_CPLUSPLUS
  const omni::ryml::with_diagnostics_t result = omni::ryml::deserialize(
    omni::type<service>, document);
#else
  const auto result = omni::ryml::deserialize(
    omni::type_t<service>{}, document);
#endif

  if (!result.value) {
    std::cerr << result.diagnostics << '\n';
    return 1;
  }

  std::cout << "Deserialized: " << result.value->name << " -> "
    << result.value->upstream.host << ':' << result.value->upstream.port
    << "\n\n";

  // Mapping errors and malformed documents both leave `.value` empty.
  const std::vector<std::string> invalid_documents{
    R"(
{
  "name": "gateway",
  "upstream": {"host": "localhost", "port": "https"},
  "tags": []
}
)",
    R"({"name":)",
  };

  for (const auto &invalid_document : invalid_documents) {
#if 201703L <= OMNI_CPLUSPLUS
    const omni::ryml::with_diagnostics_t rejected = omni::ryml::deserialize(
      omni::type<service>, invalid_document);
#else
    const auto rejected = omni::ryml::deserialize(
      omni::type_t<service>{}, invalid_document);
#endif

    if (rejected.value) {
      std::cerr << "Unexpectedly accepted: " << invalid_document << '\n';
      return 1;
    }

    std::cout << "Rejected input:\n" << invalid_document << "\nDiagnostics:\n"
      << rejected.diagnostics << "\n\n";
  }
}
