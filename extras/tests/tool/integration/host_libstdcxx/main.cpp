/** Code accepted by the project's compiler must also pass instrumentation.
 * On 64-bit GNU/Linux, libstdc++ uses int64_t (long) for seconds::rep, so the
 * std::max call below has matching argument types. Bundled libc++ instead
 * uses long long: substituting its headers makes template deduction fail.
 * The reflected target must build using the host libstdc++ headers; running
 * it also checks the calculation's result. C++11 suffices for this case. */
#include <algorithm>
#include <chrono>
#include <cstdint>

std::int64_t minutes(std::chrono::seconds period) {
  return std::max(std::int64_t{1}, period.count() / 60);
}

int main() {
  return 2 == minutes(std::chrono::seconds{120}) ? 0 : 1;
}
