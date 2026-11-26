# CI validation

PRs normally run smoke across all package distributions. Add `ci:full` before a
push, or add it and select **Re-run all jobs**, to run full validation. Every
attempt reads current labels. PR run names say `automatic`; selection and the
Summary show the actual suite. Standalone manual runs do not satisfy PR checks.
Manual `full_suite` requests run full; `build_sources` forces package work.

Master runs full after five first-parent commits since the latest green full
validation, otherwise smoke. A missing baseline selects full. Full includes
smoke, so each run executes only its selected suite.

Ordinary master/PR reruns may reuse exact-commit packages. A merged PR's final
synthetic checkout also qualifies when GitHub confirms the final head and its
complete tree equals the merged tree. Master may reuse those checks; smoke does
not reset cadence, while green full coverage does. Explicit full requests still
execute full tests. Pages continues to build and publish from master.

## Nightly and release packages

At 06:00 Europe/Belgrade every day, nightly inspects master. Nightly and release
require packages originally built from that exact commit. A restaged PR build
or an older artifact without build provenance triggers a fresh build and full
suite. An exact-commit smoke build can supply packages for full validation. A
complete exact-commit package/symbol set with green full validation needs no
duplicate work. Missing packages or symbols require a fresh build and full
validation; tree-only full coverage cannot authorize new binaries.

Release preflight waits for existing master CI to succeed. Native selectors
wait for earlier active runs for the commit before choosing work; failures stay
failed. Release ships the bytes that passed full validation, with archive names
and embedded binary IDs naming the tagged commit. The tag is checked again
before creating the draft prerelease.

Package markers explicitly retain the original `build_commit` through every
restaging, even when tests run again. Smoke/full markers bind to their successful
attempt; artifact ZIPs retain SHA-256 verification. Packages and symbols must
predate their validation gate. A failed rerun can preserve earlier green bytes,
but its replacement uploads cannot borrow older success. Full coverage reused
from another tree-equivalent run never validates new uploads.

Before a release exists, move an annotated tag by pushing just its ref:

```sh
git tag -fa v0.0.9 <commit> -m "v0.0.9"
git push origin +refs/tags/v0.0.9
```

This triggers release validation without deleting the remote tag first. For an
unchanged commit, rerun failed workflow jobs instead of moving the tag.
