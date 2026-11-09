# JSON customization

## 1. Aliases mapping

Accept legacy field and enum names while keeping the current C++ model.
Normalize the input tree before mapping rather than adding alias parameters to
the mapper itself.

Provide small QoL compositors identifying an existing member and its accepted
aliases, conceptually:

```text
&Person::email <- "mailing_address", "mailbox"
```

Unusual migrations, including non-1:1 mappings and captured external state, can
use the same tree transformation instrument. Defer dedicated mechanisms until
recurring use cases justify them.

## 2. Types mapping

Customize type-to-node and node-to-type mapping, including representations that
differ from the reflected record structure.

Properties to identify and infer through traits, reflection, or protocols:

- JSON representation: value category; numeric range and integral constraints;
  member names/types, element types, nesting and fixed sizes; enum choices.
- Mapping capabilities: construction, assignment, reading, iteration and
  insertion; unique/repeated keys and ordering; absence; owned/borrowed storage.

Observable capabilities do not determine intended representation or policy.

Consider `json_scalar`, `json_array`, and `json_mapping` as optional building
blocks providing default node-shape checks, rather than mandatory customization
interfaces. For example, a scalar building block could check that the input node
is scalar. This remains a design option, not part of the current prototype.

Keep type-based selection in mind: an entry keyed by the type or a unary
template callable, with custom mapping preferred over built-in mapping at each
recursive step. Derive the customization contract from the mapping algorithm's
actual inputs, results, diagnostics and recovery requirements.

____

## General interface instruments

Prioritize the interface demonstrated in the guide. The lower-level `core::`
interface may evolve as customization develops; preserving its signatures is
not currently a compatibility requirement.

### Global and local customization

- Global: SFINAE specialization associated with the type.
- Local: a higher-order callable binding; possibly a builder-like interface.

Proposed precedence: local override > global specialization > built-in mapping.

Prefer readable `static_assert` diagnostics at the public call or registration
boundary, before instantiating implementation details. Avoid eager constraints
or return-type expressions failing deep inside `detail` before that validation.

### `with_transform_tree(f)`

Returns a reusable `config -> config` callable. Apply it through
`deserialize.transform_config(with_transform_tree(f))` or directly to a config
value. The binding stores only config. The registered callback runs after
successful parsing and before mapping.

```cpp
deserialize.transform_config(with_transform_tree(rename_legacy_fields))(type, source);
```

Both overloads configure the callback stored in `config.transform_tree`.
Template-parameter SFINAE constrains both overloads in every language mode.
C++14 and later return the concrete configuration lambda; C++11 returns
`std::function<config(config)>`:

- `Tree -> Tree`: replaces the previous transform.
- `(Tree, previous) -> Tree`: may invoke or discard the previous transform;
  `previous` is identity when none was bound.

The factories support constant evaluation from C++17; applying the returned
configuration callable remains a runtime operation.

Null function pointers reset the transformation to identity in either form.
The decorator form takes precedence for callables supporting both signatures.

Callbacks must be copy constructible and return a tree (including cv/ref-qualified
results).
Run only after successful parsing; mapping and retained diagnostics use the
transformed tree.

### Typed access chains

Identify a member occurrence for contextual overrides of mappings, aliases,
documentation, etc., while retaining the nested type's global defaults elsewhere.

```cpp
struct nested { int a; };

struct outer {
  nested n;
  std::vector<nested> v;
};

access(&outer::n, &nested::a)
```

Validate each access step at compile time. Keep simple member chains useful
without requiring a complete schema-path language. Sequence-element traversal
such as `outer.v[].a` needs a separate step contract; its spelling is TBD.

Potentially reusable for JSON/OpenAPI schema rendering and other extensions.
An ordinary C++ access expression does not itself expose its steps for deduction.
