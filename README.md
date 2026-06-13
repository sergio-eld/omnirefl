todo: write the readme

# Release Scope

## Minimal Release

### Types

- (+) named globally accessible records
- (+) named globally accessible enums
- (+) nested named records/enums of supported globally accessible parents
- (+) primary record templates, including type, value, and template-template
  parameters
- (+) observed concrete instantiations of supported primary record templates
- (-) explicit record template specializations
- (-) partial record template specializations

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
- (+) field count / iteration
- (+) unqualified record name
- (?) namespace-qualified record name

### Enums

- (+) enumerator names
- (+) enumerator values
- (+) scoped enums
- (+) fixed-underlying enums
- (+) enum type name
- (?) namespace-qualified enum type name
- (?) plain unscoped enum field dependencies

### Dependency Routes

- (+) public field type dependencies
- (+) public base type dependencies
- (+) transitive public base dependencies
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
- (+) read field values
- (+) write field values
- (+) write inherited public field values
- (+) reflect enum names
- (+) reflect enum values
- (+) recurse into reflected record fields
- (+) recurse through supported dependency routes

### Build/Release

- (+) generated-header reflection
- (+) CMake integration
- (+) Linux package/install matrix
- (+) Windows package/install test
- (+) GCC and Clang package/install tests

## Planned After Minimal Release

### Types

- (-) nested records inside record template parents
- (-) explicit specializations
- (-) partial specializations
- (-) specialization-specific record template metadata
- (-) specialization-specific CRTP base metadata

### Metadata

- (-) comment annotations
- (-) namespace-qualified reflected type names
- (-) specialization-aware reflected type names when/if explicit or partial
  specializations are implemented

### Frontend/API

- (-) refine the public interface
- (-) replace `reflected_call`
- (-) refine the CLI interface
- (-) remove dependency on compilation database
- (-) make the tool callable like a compiler instance with limited support for
  compilation-meaningful flags

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
