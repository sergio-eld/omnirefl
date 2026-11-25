#include <cstddef>
#include <iostream>

namespace shared_source {
std::size_t count_fields();
}

int main() {
  const auto count = shared_source::count_fields();
  if (EXPECTED_FIELDS != count) {
    std::cerr << "Expected " << EXPECTED_FIELDS << " fields, got " << count
      << '\n';
    return 1;
  }
  return 0;
}
