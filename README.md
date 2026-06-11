
todo: write the readme

TODO: document reflected_call output construction. A type mentioned only as a
lambda return type is not reflected; the reflectable type must be passed as an
argument, for example via std::type_identity<T> or an equivalent tag.

TODO(High): document generated-header type support.

Currently supported:
- forward-declarable records and enums
- nested records/enums of a forward-declarable parent
- direct public base fields

Release requirements:
- transitive public base fields
- record templates
- record template specializations

Planned/considered:
- unnamed non-local types addressable from namespace scope via decltype(symbol)
- unnamed non-local types addressable through a function return type

Not planned for release:
- local/block-scope types that cannot be named from generated namespace-scope
  specializations
- non-public fields
- recursive reflection
