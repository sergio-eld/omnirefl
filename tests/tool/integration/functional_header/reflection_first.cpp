#include <omnirefl/reflection.hpp>
#include <omnirefl/functional.hpp>

#include <type_traits>

static_assert(std::is_same<int, omni::type_t<int>::type>::value,
  "reflection-first inclusion must preserve the type tag");
