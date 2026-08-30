// Expected failure: direct, indirect, and templated virtual bases are not
// supported reflection inputs.
#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_virtual_bases {

struct root {
  int value;
};

struct direct_virtual_base: virtual root {};

// `root` remains a virtual base, but is not a direct base of this record.
struct indirect_virtual_base: direct_virtual_base {};

template <typename T>
struct template_virtual_base: virtual root {
  T payload;
};

struct indirect_template_virtual_base: template_virtual_base<int> {};

void run() {
  direct_virtual_base value;
  indirect_template_virtual_base template_value;

  (void)omni::reflected_call([](auto...) -> void {},
    value,
    omni::type<indirect_virtual_base>,
    omni::type<template_virtual_base<int>>,
    template_value);
}

} // namespace negative_reflected_call_virtual_bases
