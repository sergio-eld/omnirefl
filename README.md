# Release Scope

## Minimal Release

### Types

- (+) named globally accessible records
- (+) named globally accessible enums
- (+) nested named records/enums of supported globally accessible parents
- (+) primary record templates, including type, value, and template-template
  parameters
- (+) observed concrete instantiations of supported primary record templates

### Records

- (+) public data fields
- (+) read public fields
- (+) write public non-const fields
- (+) inherited public fields
- (+) transitive public base fields
- (+) public CRTP base fields through supported primary templates
- (+) multiple public base fields
- (+) public base fields from template records
- (+) private/protected base fields are not reflected through derived records
- (+) non-public fields are not reflected
- (+) field names
- (+) field types
- (+) canonical field type names
- (+) only canonical field type names are collected; alias spelling such as
  `std::uint16_t` is not preserved
- (+) field count / iteration
- (+) field index is local to the declaring record, not the flattened inherited
  field tuple
- (+) record `type_name()` without namespaces, including enclosing records
- (+) record `qualified_type_name()` with namespaces and enclosing records

### Enums

- (+) enumerator names
- (+) enumerator values
- (+) scoped enums
- (+) fixed-underlying enums
- (+) enum `type_name()` without namespaces
- (+) enum `qualified_type_name()` with namespaces
- (?) plain unscoped enum field dependencies

### Dependency Routes

- (+) public field type dependencies
- (+) public base type dependencies
- (+) transitive public base dependencies
- (+) standard-library public bases are ignored as reflection dependencies
- (+) template record field dependencies
- (+) CRTP base dependencies
- (+) supported member alias dependencies: `error_type`, `key_type`, `type`,
  `value`, `value_type`
- (+) supported template-pack dependencies for template names exactly `tuple`
  and `variant`
- (+) recursive dependency walk through supported routes

### Serialization Completeness

- (+) reflect record field names
- (+) reflect record field types
- (+) reflect canonical record field type names
- (+) reflect record and field annotations
- (+) reflect documentation comment annotations from `///`, `//!`,
  `/** */`, `/*! */`, `///<`, and `//!<`
- (+) read field values
- (+) write field values
- (+) write inherited public field values
- (+) reflect enum names
- (+) reflect enum values
- (+) reflect enum annotations
- (+) recurse into reflected record fields
- (+) recurse through supported dependency routes

### Frontend/API

- (+) `reflected_call` as the supported reflection instrumentation interface
- (+) `meta_t`, `binding_t`, `field_meta_t`, and `field_binding_t` public
  reflection interfaces
- (+) C++20 `meta`, `binding`, `field_meta`, and `field_binding` concepts

### Build/Release

- (+) generated-header reflection
- (+) CMake integration
- (+) annotations enabled by default
- (+) annotations can be disabled with `--no-annotations` or CMake
  `NO_ANNOTATIONS`
- (+) Linux package/install matrix
- (+) Windows package/install test
- (+) GCC and Clang package/install tests

## Planned After Minimal Release

### Types

- (-) nested records inside record template parents
- (-) explicit record template specializations
- (-) partial record template specializations
- (-) specialization-specific record template metadata
- (-) specialization-specific CRTP base metadata

### Metadata

- (-) OpenAPI-like schema table generation from reflected structs/enums using
  type, field, enum, and annotation metadata
- (-) specialization-aware reflected type names when/if explicit or partial
  specializations are implemented

### Frontend/API

- TODO(High): forbid `reflected_call` inside a reflected scope. With the current
  deferred visitor implementation, reliable detection is not practically
  possible without instantiating visitor bodies or adding a broader semantic
  analysis pass.
- TODO(High): detect reflection query instantiation outside a reflected scope as
  a tool error when possible, and add dedicated negative tool-run tests.
- (-) refine the public interface
- (-) replace `reflected_call`
- (-) document why reflected-scope visitors sometimes need explicit trailing
  return types to avoid premature instantiation during the tool run
- (-) separate const and mutable public-field accessors in the public interface
- (-) recoverable reflection query/fallback branch for non-reflected types
- (-) type-erased field wrappers, likely short `field_t`-style names
- (-) refine the CLI interface
- (-) remove dependency on compilation database
- (-) make the tool callable like a compiler instance with limited support for
  compilation-meaningful flags
- (-) Unix-like invocation: `omnirefl -o <reflection.hpp> -MF <deps.d> -- <cc1 args...>`
- (-) split compiler-driver/compile-db args to cc1 mapping into a separate
  composable tool

### Build/Release

- (-) add Windows MinGW/MSYS2 to the test matrix
- (-) add Linux MinGW cross-compilation for Windows to the test matrix

## Might Be Considered Later

- (-) unnamed non-local types addressable from namespace scope via
  `decltype(symbol)`
- (-) unnamed non-local types addressable through function return type
- (-) other globally addressable unnamed cases

## Out

- (-) source mode
- (-) private/protected fields
- (-) methods
- (-) local/block-scope types
- (-) arbitrary composed `reflected_call` instrumentation
- (-) recursive reflection
