#include <gtest/gtest.h>

namespace {

// TODO(High): turn this file into the executable usage guide referenced by the
// README.
//
// The file should be written as a presentation: every meaningful example is a
// named TEST block, examples compile and run, and comments explain the
// reflection rule being demonstrated without hiding the API behind planned QoL
// helpers.
//
// 1. Start with the instrumentation boundary.
//    - The first example should be useful enough to keep attention:
//      field names, field type names, and read field values for a small record.
//    - Explain that `reflected_call` is the instrumentation boundary.
//    - Explain that the visitor is the reflected scope.
//    - Explain that reflection queries are valid inside the visitor and are
//      tool errors outside it.
//    - Include commented-out lines marked "uncomment for tool error":
//      - reflection query outside `reflected_call`;
//      - reflected query inside a non-template visitor or lambda shape that
//        forces premature instantiation, if still applicable;
//      - nested/recursive `reflected_call` inside a reflected scope, if/when
//        diagnostics are implemented.
//    - The invalid examples should be close to the first successful example so
//      users learn the limitation early, but the first impression stays useful.
//
// 2. Prefer the IDE-friendly C++20 path first.
//    - Use concept-constrained lambda args:
//      - `[](omni::binding auto value) -> result_t { ... }`
//      - `[](omni::meta auto type) -> result_t { ... }`
//    - Use `if constexpr` for type-dependent behavior where it improves
//      comprehension.
//    - Use `omni::compat::apply` directly on the fields tuple.
//    - Do not introduce planned helpers such as `omni::each`, tuple monads, or
//      other QoL wrappers in the basic examples.
//    - Show both meta-only usage and value-binding usage.
//
// 3. Demonstrate bindings explicitly.
//    - `binding_t<const T &>`: read-only referenced object.
//    - `binding_t<T &>`: mutable referenced object.
//    - `binding_t<T>`: owning binding from a moved value, if this is part of the
//      stable public API.
//    - Show why this matters by reading fields from const/ref bindings and
//      writing fields through mutable bindings.
//
// 4. Add a simple write-fields example.
//    - Use a small "foo/bar" visitor:
//      - if a mutable field name contains `foo`, write `8`;
//      - if a mutable field name contains `bar`, write `15`.
//    - Keep it intentionally simpler than map/variant deserialization.
//    - Assert the resulting field values.
//
// 5. Demonstrate enums.
//    - Enumerator names.
//    - Enumerator values.
//    - Scoped and fixed-underlying enum examples if they remain compact.
//    - Keep enum annotations for a later section unless it makes the example
//      clearer.
//
// 6. Demonstrate primary record templates.
//    - Include observed concrete instantiations such as `box<int>` and
//      `box<std::string>`.
//    - Include one allocator/container-shaped example if it stays readable,
//      because that is a practical serialization use case.
//    - Include a CRTP or public-template-base example only if it remains short.
//    - Comment that primary record templates are supported through observed
//      instantiations.
//    - Comment that explicit and partial specializations are not part of this
//      basic guide.
//    - Do not imply that default template arguments are emitted by omnirefl.
//
// 7. Add the C++17 adaptation after the C++20 examples.
//    - Use generic lambdas and `if constexpr`.
//    - Show the smallest change from the C++20 version.
//    - If omitted specialization / class template argument deduction is used for
//      local readability, keep the spelling exact and explain why it is
//      IDE-friendly.
//
// 8. Add the C++11 compatibility version last.
//    - Use struct visitors with templated `operator()`.
//    - Show one meta visitor and one binding visitor.
//    - Make it clear this is the compatibility form for standards without
//      generic lambdas or `if constexpr`.
//
// 9. End with a short diagnostics section.
//    - Keep examples commented out.
//    - Mark them as "uncomment for tool error".
//    - Include only errors users are likely to hit:
//      - reflection query outside reflected scope;
//      - recursive/nested `reflected_call` once a diagnostic exists;
//      - unsupported local/unnamed/indexed-type routes only if still relevant to
//        the public release story.
//
// Keep this file example-first. It should teach how to serialize-like inspect
// POD data with records, enums, fields, templates, and bindings before it dives
// into edge cases.

} // namespace
