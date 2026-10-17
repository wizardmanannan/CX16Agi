# Preparing changes for review

For a correctness or performance bug, add a permanent native C case under
`tools/` before changing production behavior. Run it on the baseline and observe
the reported symptom, then confirm it passes on the fixed candidate. Extend
the existing suites:

```sh
make -C tools test
make -C tools production-test
```

The first command runs the host C cases; the second builds a fresh image and
runs compiled production callers on the emulator CPU. Test outputs live under
`obj/`; each test owns and cleans its temporary fixtures. The suites need the
pinned cc65 toolchain and emulator CPU sources, but no ROM or game files. See
[README.md](README.md) for dependencies and asset-based integration commands.

Record the exact baseline failure, fixed candidate result, tested commit SHA,
commands, and execution limits. A negative control that reapplies the bug is
useful when it can be made repeatable. Host tests can isolate parsing or
geometry; use compiled target callers when correctness depends on target
integer widths, bank selection, or the C/assembly calling convention. Exercise
combined lifecycle sequences when behavior depends on order, such as failure,
retry, reset, eviction, or cleanup.

Successful tests with uncommitted changes do not validate the published head.
Keep a change focused, and use a commit subject that states the behavior, such
as `fix: preserve bank ownership during reset`. In the commit body, name the
broken behavior, the regression, and its rerun command.

## Review evidence

The `native-head` job tests the pull request head. `native-integration` runs the
same local suite commands on GitHub's synthetic merge commit and verifies its
parents match the event's PR head and base. The `native-ready` check requires
both jobs to succeed, verifies their recorded head and tree identities, and
fails if current `main` moved from the tested base. Each job publishes logs
and SHA-256 digests. These CPU-core results do not establish audio-device
behavior or hardware timing.

For a publication packet, declare the required suites and baseline witnesses
in the `review-plan-v2` at `tools/review/cx16.plan`, then capture them to a new,
empty evidence path. Export `CC65_HOME` and `X16_EMULATOR_SOURCE` as absolute
paths to the installations used by the suites (see [the review guide](tools/review/README.md)):

```sh
make -C tools review-tools
obj/review-tools capture --plan tools/review/cx16.plan --output obj/review-evidence-new
```

Read the current base from the target repository, then run the publication
gate with the exact commits and PR identity. It verifies
and replays the existing capture before checking live GitHub readiness; it does
not create the capture:

```sh
mkdir -p obj/review-replay-new
make -C tools review-ready \
  REVIEW_PLAN=tools/review/cx16.plan \
  REVIEW_EVIDENCE=obj/review-evidence-new \
  REVIEW_REPLAY_ROOT=obj/review-replay-new \
  REVIEW_REPOSITORY=wizardmanannan/CX16Agi \
  REVIEW_PR=123 \
  REVIEW_HEAD="$(git rev-parse HEAD)" \
  REVIEW_BASE="$(gh api repos/wizardmanannan/CX16Agi/branches/main --jq .commit.sha)"
```

The gate checks live GitHub readiness for the expected PR head and base.
Include series or performance suites in the plan when they are required for the
decision; the verifier reruns every declared suite. The generated packet and
local event ledger record process provenance. They do not prove owner review
or GitHub approval. Readiness requires a current-head approval from a designated
repository owner and matching branch protection on GitHub.

## Contracts and performance

Keep a public interface, its production callers, and its regression together.
Test the real caller contract: arguments, return value, bank identity and
restoration, publication order, failure exit behavior, and recovery state when
those boundaries apply. For performance, retain a repeatable workload and
baseline, use the same inputs and runner, compare output hashes, image bytes,
and emulator cycles, and state what the timing model cannot establish. Report
latency only for comparable workload boundaries.

For a multi-PR series, list dependencies, intended merge order, and rollback
dependencies. Each proposed head must pass its declared suites with only its
listed prerequisites. Give reviewers an ordered route through the commits and
identify the entry point and rollback dependencies for each unit. Keep a
reviewed follow-up easy to assess by naming the SHA it replaces and showing
the incremental diff. Fold exploratory fixups into their owning changes while
preserving attribution.

When rewriting a series, account for every original commit, including folds,
splits, and exclusions. Use `git range-diff BASE..OLD_HEAD BASE..NEW_HEAD` and
compare trees when the rewrite should preserve content; these checks establish
traceability, not semantic correctness. For merge commits, inspect
`git show --remerge-diff MERGE_SHA` and explain each substantive resolution.
Keep the release image, test binaries, and generated fixtures in the documented
output layout; tests must create their own inputs and work from a clean checkout.
