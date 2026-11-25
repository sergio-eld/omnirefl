# Unreleased

## JSON/YAML

- Reduce compiler memory use for wide records by replacing intermediate field
  tuples with flat storage and direct field-pack processing. Record mapping,
  customization, validation, and diagnostic policies retain their behavior.
