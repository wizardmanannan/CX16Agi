# Preparing changes for review

Prepare the change so a reviewer can establish its purpose, correctness, and
dependencies without reconstructing the development session. The PR template
asks for the short version; keep detailed evidence in the relevant test and
commit body.

## Establish the regression

For a correctness or performance change, first add a permanent C case under
`tools/` and observe the reported failure through its named suite command.
Exercise the production path responsible for the symptom. A host test with
modeled dependencies can isolate parsing or geometry, but cannot establish
target integer widths, bank selection, or the C/assembly calling convention.
Use compiled target callers for those contracts. If the necessary seam is
missing, identify that gap rather than substituting a passing shallow test.

Extend the existing suite and helpers. Tests generate indispensable inputs in
unique owned fixture directories, clean them on success and failure, and build
under `obj/`. Do not require files left by earlier runs. A deliberately failing
reproducer commit should name the expected failure; do not describe that
checkpoint as green.

## Run and report the relevant suites

```sh
make -C tools test
make -C tools production-test
```

The first command runs focused native C cases. The second builds a fresh image
and exercises compiled production callers on the emulator CPU core; it needs
neither a ROM nor game files. See [README.md](README.md) for dependency setup,
overrides, and full-emulator integration commands. Use your original game data
and a compatible ROM for asset-dependent validation; do not commit those inputs.

Report the tested commit SHA, commands, results, and execution boundaries.
Successful tests with uncommitted changes do not validate the published head.
The [native CI workflow](.github/workflows/native-regressions.yml) records its
checkout SHA and dependency pins and preserves both suite results and logs.
Its CPU-core checks do not establish audio-device behavior or hardware timing.
For diagnostic output, pass `KEEP_TEST_OUTPUT=1` as a Make argument; fixtures
must still be cleaned.

## Build a series around review decisions

Use commit subjects such as `fix: preserve bank ownership during reset` or
`perf: reduce bounded sprite queue cycles`. Explain the broken behavior and
result, then name the regression and rerun command. Reserve performance claims
for measured workloads, with the baseline, result, and code-size tradeoff.

Keep a public interface, all its callers, and its regression together. Separate
unrelated behaviors and mechanical renames or formatting. Fold exploratory
fixups into their owning changes before review, while preserving attribution.
Commit count and line count are not substitutes for coherent scope.

For a large series, identify dependencies before choosing PR boundaries.
Each proposed PR head must build and run its stated suites with only its listed
prerequisites. Test infrastructure must not silently require production code
from a later PR. Provide a short ordered review route, an entry point for each
unit, and its rollback dependencies. A dependency table is useful; an agent
transcript or a chronological work diary is not.

When rewriting a long series, account for every original commit, including
folds, splits, and deliberate exclusions. Compare the final trees and inspect
`git range-diff BASE..OLD_HEAD BASE..NEW_HEAD` for a linear series. Explain
content changes separately from history-only changes. Range-diff and tree
equality establish traceability, not semantic correctness.

## Check contracts across boundaries

| Changed boundary | Evidence to include when relevant |
| --- | --- |
| C/assembly interface | Compiled caller; arguments, stack consumption, return registers, and immediately consumed condition flags |
| Banked memory | Address plus bank identity, identical-address aliases, bank restoration, and reads at the bank edge |
| IRQ publication | Interruption at publication boundaries, nested ownership, final-write-before-completion, and combined workload latency |
| Allocation and caches | Partial failure positions, retained owners, repeated reset, eviction, retry, and stale caller metadata |
| Text and input | Source and destination limits, physical placement, hide/show, redraw invalidation, and display-to-cleanup sequences |
| Resources | Truncated reads, invalid lengths/offsets, publication on failure, and the actual allocator's failure contract |
| Sound | Stop/restart, sparse channels, simultaneous endings, end flags, refill bounds, and exact PSG addressing |
| Performance | Equivalent output and boundaries, repeatable maximum workload, retained baseline, and the limits of the timing model |

## Update a reviewed series predictably

After review starts, keep follow-up changes easy to identify. State which
reviewed revision they replace and show the incremental diff. Avoid unrelated
rewrites that invalidate review anchors. Resolve conflicts by checking both
contracts and their callers, and explain nontrivial resolutions. Rerun the
relevant suites on the resulting head; green results from either parent do not
validate the integration.

For a merge commit, inspect `git show --remerge-diff MERGE_HEAD` to distinguish
the resolutions and edits made during integration from incoming upstream work.
This view includes conflict-marker removal, so line counts are not a measure of
new behavior. Give each substantive resolution its rationale and regression;
keep newly discovered independent fixes identifiable rather than hiding them in
an unexplained merge.
