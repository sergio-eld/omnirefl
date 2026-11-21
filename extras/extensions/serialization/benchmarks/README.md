Runtime schema mapping versus handwritten JSON mapping
=====================================================

`manual_mapping.cpp` compares `map_by_schema` with handwritten readers that
look up each field in the document. It also compares reflected and handwritten
record-to-JSON writing. The comparison runs in the C++20 and newer variants of
the existing serialization benchmark executable.

| Record | Shape |
| --- | --- |
| `flat` | Six fields: string, Boolean, signed/unsigned integers, and real |
| `nested` | Three fields, including a six-field record; two record layers |
| `deep` | Two nested records and a revision; three record layers |
| `sequence` | Vector of four records; three record layers plus a sequence |

Each reader processes the same valid JSON and invalid variants: a quoted
integer, null, integer overflow, a missing field, an unknown field, a duplicate
field, and an array in place of the root object. Deep and sequence workloads
also fail in their last nested record to measure work before a late error.
The default configuration rejects unknown/duplicate fields and stops at the
first error. Structural errors precede missing fields, then values are decoded
in declaration order.

The manual implementation contains no reflection or customization lookup.
Its record readers explicitly name and look up each field. Presence flags
detect duplicates and missing fields without repeated document searches.
The sequence reader reserves its input size and uses `emplace_back`.
The manual readers share a structural validator and scalar conversions.
Both sides use the public node checks and RapidYAML/C4 conversion routines,
including checked integer overflow.
The comparison therefore measures record traversal, lookup, construction, and
diagnostic propagation rather than different numeric parsers.

Before timing, every reader pair checks decoded values, expected diagnostic
text, and structured issues including paths and node IDs. Every writer pair
checks identical JSON output and a value-preserving round trip. A mismatch
reports a benchmark error instead of a timing result.

Timed boundaries:

- **Read:** mapping an already parsed tree into a fresh value and structured
  diagnostics, including result destruction. Parsing and diagnostic rendering
  are excluded from both sides.
- **Write:** building a fresh tree and emitting JSON, including allocation,
  numeric formatting, and destruction. Both sides borrow source string values,
  copy keys, and use the same emitter and output-reserve estimate. The library
  side uses `as_ref_tree`, as the default `as_json` implementation does.
  Inputs contain no raw ASCII control characters, so neither side needs the
  default serializer's backend escape-repair pass.

Build the existing benchmark target in a configured build with benchmarks
enabled:

```sh
cmake --build <build> --target cpp_20.benchmark.serialization
```

Run only these comparisons, with repetitions and randomized ordering to reduce
drift between the paired implementations:

```sh
<build>/benchmarks/extensions/serialization/cpp_20.benchmark.serialization \
  --benchmark_filter='^comparison/' \
  --benchmark_min_time=0.1s \
  --benchmark_repetitions=5 \
  --benchmark_enable_random_interleaving=true \
  --benchmark_report_aggregates_only=true \
  --benchmark_out=comparison.json \
  --benchmark_out_format=json
```

Names end in `/schema` or `/manual`. Compare paired median `cpu_time` values:
`schema / manual` is the runtime ratio; `(schema / manual - 1) * 100` is the
additional time in percent. These results compare these concrete handwritten
implementations, not a theoretical minimum. They exclude instrumentation and
compilation costs.
