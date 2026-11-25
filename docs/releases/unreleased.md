# Unreleased

## Breaking changes

- `map_range_t` becomes a template; use `map_range_t<>` for its default type.
- `map_record_t` gains an optional second `ExplicitNulls` template parameter;
  existing `map_record_t<Strategy>` spellings remain valid. It is now `final`
  to prevent inherited `.tdm_` updates from discarding derived members.

## JSON/YAML

- Serialization and tree builders expose `write_config` for empty optional
  fields and array elements. `map_record` and `map_range` can retain nulls with
  `.tdm_explicit_nulls`; false follows the shared policy. The enclosing mapper
  decides omission before creating a node. Defaults retain null fields and omit
  empty array elements. Empty roots remain null.
