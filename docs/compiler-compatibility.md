# Older compilers and draft C++ modes

Omnirefl parses your source with its bundled Clang frontend; your configured
compiler then compiles it with the generated metadata. Both must agree on the
declarations and language rules used by reflection. `omnirefl --version` reports
the bundled LLVM version.

An older compiler's `-std=c++2a` mode implements a particular C++20 draft, not
necessarily the rules used by modern Clang. Even accepting `-std=c++20` and
reporting `__cplusplus == 202002L` does not establish feature parity: Clang 10,
for example, reports that date but lacks `__cpp_consteval`.

**Warning:** differences can cause instrumentation errors or silently produce
metadata for a different set of fields. Successful instrumentation and final
compilation alone do not establish that the metadata matches your program.

## Known differences

- **Aggregate classification:** Clang 6's C++2a mode allows an explicitly
  defaulted constructor in an aggregate. Modern C++20 rules reject some
  initializers accepted by that backend.
- **Parenthesized aggregate initialization:** Clang 10's C++20 mode lacks this
  feature while supporting concepts and `constinit`. Construction traits and
  overload selection can differ during instrumentation.
- **Conditional declarations:** feature macros and queries such as
  `__has_builtin` can differ. The frontend can observe fields absent from the
  backend's record.
- **Coroutines TS:** Clang 10's `-fcoroutines-ts` uses the experimental
  coroutine protocol. Modern Clang requires the standardized protocol;
  renaming the option is insufficient.

For example, Clang 6 accepts this in C++2a mode:

```cpp
struct record {
  record() = default;
  int value;
};

record make_record() { return record{42}; }
```

Under modern C++20 rules, the user-declared constructor makes `record` a
non-aggregate, so that initialization fails. Matching feature macros does not
restore the older aggregate rule.

Conversely, for `struct record { int value; };`,
`std::is_constructible<record, int>::value` is false in Clang 10's C++20 mode
and true in modern Clang. Lowering instrumentation to C++17 changes that result,
but also removes concepts and `constinit` that the same source may require.

A query mismatch can instead change the record silently. With Clang 10:

```cpp
struct record {
  int common;
#if __has_builtin(__builtin_source_location)
  int extra;
#endif
};
```

The backend sees one field; the newer frontend sees two. Globally redefining
`__has_builtin` is unsafe because standard-library headers also use it to
select implementations. An ABI compatibility flag does not select these older
language semantics either.

## Current workarounds

For configured GCC and upstream Clang builds using GNU-style command lines,
including MinGW cross-compilation, the [ad hoc CMake adapter][compiler-adapter]
queries the backend's C++20 feature defaults and supplies instrumentation-only
adjustments:

- `-fno-char8_t` when the backend lacks `__cpp_char8_t`. This also avoids
  activating unavailable `std::u8string` declarations in older dependencies.
- `-U__cpp_consteval` when the backend lacks that macro. This matches
  conditional declarations; it does not emulate older immediate-function
  semantics.

Explicit source flags take precedence. The real compiler's flags are unchanged.
Direct CLI users must supply these adjustments themselves when needed.
The probe excludes AppleClang and MSVC-style command lines (`cl`, `clang-cl`).

The query omits project flags and headers. Fixed arguments in
`CMAKE_CXX_COMPILER_ARG1` must work independently of those options. If a fixed
`-include` needs project include paths, move it to `CMAKE_CXX_FLAGS` or
`target_compile_options`; otherwise the query fails. Retrying with arbitrary
project flags could load unrelated headers or overwrite build outputs.

Omnirefl also discards the retired `-fexperimental-new-pass-manager` and
`-fno-experimental-new-pass-manager` switches during instrumentation, including
their `-Xclang` forms. Identically named header operands are preserved, and
unrelated unknown options still produce errors.

These repairs cover specific mismatches. They do not emulate arbitrary older
drafts, reconcile every feature-macro revision or builtin query, or restore the
coroutines TS protocol.

## Choosing a configuration

Use a newer backend where practical, or an earlier language mode if your source
does not need the affected draft features. Changing a standard flag or copying
feature macros cannot reproduce an arbitrary mixture of old and new rules.

For compiler-dependent reflected declarations, check the resulting metadata
against the record compiled by the backend. When reporting a mismatch, include
both compiler versions, the exact compile command, and a minimal source that
shows the differing declarations or semantics; see
[bug reports](../README.md#troubleshooting-and-bug-reports).

[compiler-adapter]: ../cmake/_ad_hoc_compiler_compatibility.cmake
