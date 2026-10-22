# Native review readiness

## Executable evidence

`review-plan-v2` declares the candidate commit, toolchain, inputs, required
suite commands, and baseline regression witnesses. A witness runs the same
declared test overlay against a baseline that must show its expected symptom
and a candidate that must show the pass marker. `review-tools capture` records
the commits, trees, tools, inputs, results, logs, and digests in a review
packet. `review-tools verify` checks the packet and replays its declared suites
and witnesses. The canonical plan format and witness are exercised by
`tools/test_review_evidence.c`.

The KQ1 startup palette witness builds the real test image and compiled caller
harness on baseline `079d92d91f7dc415ba1770b8adc2f38f64cf8f81` and the
candidate. It overlays the permanent `--text-cleanup` C case and its target
callers onto that pre-KQ1 baseline. The baseline must report the exact
`palette=$10 expected=$40` symptom: the old white palette-1 attribute remains
visible where the managed black-background palette should be. The candidate
must complete the same case and print its success marker.

The plan takes absolute dependency roots from `CC65_HOME` and
`X16_EMULATOR_SOURCE`. Export the same installed roots used by the native
suites before capture and verification; CI supplies its pinned installations.
External directory input hashes omit only the root `.git` administrative entry;
the packet records this policy. Runtime files remain hashed, and sockets or
other unsupported entries outside `.git` fail capture. Artifact hashes do not
apply this exclusion.

```sh
export CC65_HOME=/absolute/path/to/cc65
export X16_EMULATOR_SOURCE=/absolute/path/to/x16-emulator
make -C tools review-tools
obj/review-tools capture --plan tools/review/cx16.plan --output obj/review-evidence-new
mkdir -p obj/review-replay-new
obj/review-tools verify --plan tools/review/cx16.plan \
  --evidence-dir obj/review-evidence-new --replay-root obj/review-replay-new
```

Capture creates its new evidence directory; create an empty replay root before
verification. Use separate paths for each run. The plan declares
which suites and baseline witnesses the publication decision requires.
Suite and witness fixtures use unique owned directories under the caller's
absolute `TMPDIR` (default `/tmp`), outside the checkout. Generated binaries,
logs, and retained artifacts stay in the evidence output tree. Both capture
and fresh verification clean their temporary fixture directories on failure.

Declare retained outputs after all `suite` records and before the witnesses:

```text
artifact|production-native|cx16agi-production-test.*/image/agi.cx16
artifact|production-native|cx16agi-production-test.*/image/final.lbl
artifact|production-native|cx16agi-production-test.*/image/final.map
```

Patterns are relative to that suite's artifact directory. Only `*` wildcards
are supported, and they do not cross directory separators. Each rule must
match at least one nonempty regular file; matching directories and symlinks
fail, and traversal never follows symlinks. Capture and fresh replay enforce
these rules. The packet lists sorted matched paths and individual file hashes,
alongside the digest of the complete artifact tree. The project plan requests
`KEEP_TEST_OUTPUT=1` for the compiled production suite so its TEST image, labels,
and map survive fixture cleanup. These artifacts describe the test build;
they do not establish a release-image size or release qualification.

The permanent review cases have these focused rerun commands. The aggregate
`make -C tools test` also runs them; the plan separately runs
`make -C tools production-test` for compiled production callers.

| Area | Permanent C case | Focused command after `make -C tools` |
| --- | --- | --- |
| KQ1 startup white-attribute regression | `test_banked_strings_text_cases.h:run_kq1_startup_display_case` | `production-test` |
| Capture, replay, artifacts, and identity | `test_review_evidence.c` | `-f review-enforcement-test.mk test` |
| Current GitHub reviews, checks, and protection | `test_review_policy.c` | `-f review-policy.mk review-policy-test` |
| Real workflow readiness script | `test_review_workflow.c` | `review-workflow-test` |
| Dependency order and commit accounting | `test_review_series.c` | `-f review-series.mk review-series-core-test` |
| Equivalent outputs and image budgets | `test_review_metrics.c` | `-f review-series.mk review-metrics-test` |
| Linked graphics assembly and VRAM oracle | `review_graphics_metrics.c`, `test_graphics_clear.c` | `-f review-graphics-metrics.mk review-graphics-metrics-test` |
| Task/run/revision event transitions | `test_review_events.c` | `-f review-series.mk review-events-test` |

The packet establishes executable results for named revisions. It cannot
establish when they ran, who approved them, or whether GitHub permits a merge.
The local `review-events` ledger records versioned task and process events for
provenance; it is not signed approval and does not grant review authority.

## CI and publication gate

The native workflow publishes three checks. `native-head` runs
`make -C tools test` and `make -C tools production-test` on the exact PR head
(or the exact main revision for a push). `native-integration` runs those
commands on GitHub's synthetic merge commit and verifies its parents are the
event's PR base and head; on a main push it tests that exact main revision.
`native-ready` runs
even when a dependency is skipped or cancelled; it succeeds only when both
jobs succeed, their head identities agree, the integration SHA is the event
check SHA, and `main` still matches the tested base. All jobs record exact SHA
and tree identities and upload logs with SHA-256 digests. `native-ready` is
the only required check. The workflow has read-only `contents` permission,
does not use `pull_request_target` or repository secrets, and does not persist
checkout credentials while running PR code.

CI captures this plan and freshly replays it on both the head and integration
revision; each job uploads the generated packet and its logs.

First capture the declared plan, then use the publication gate for the exact
local candidate and open PR. Use a new, empty evidence path for each capture:

```sh
make -C tools review-tools
obj/review-tools capture --plan tools/review/cx16.plan --output obj/review-evidence-new
```

Read the base from the target upstream repository. A fork's `origin/main` may
refer to a different revision.

```sh
base_sha=$(gh api repos/wizardmanannan/CX16Agi/branches/main --jq .commit.sha)
mkdir -p obj/review-replay-new
make -C tools review-ready \
  REVIEW_PLAN=tools/review/cx16.plan \
  REVIEW_EVIDENCE=obj/review-evidence-new \
  REVIEW_REPLAY_ROOT=obj/review-replay-new \
  REVIEW_REPOSITORY=wizardmanannan/CX16Agi \
  REVIEW_PR=123 \
  REVIEW_HEAD="$(git rev-parse HEAD)" \
  REVIEW_BASE="$base_sha"
```

This command verifies and replays the existing capture, checks that the
candidate remains at the recorded head, and reads current GitHub reviews,
checks, and branch protection. It needs authenticated `gh` read access. Declare
series or performance suites in the plan when they are part of the review
decision; the verifier reruns every declared suite. Focused offline C cases
have dedicated makefiles and also run through `make -C tools test`.

The live check requires a current-head approval from a designated repository
owner and matching server-side branch protection. Local packets and event
ledgers cannot satisfy either condition.

## Delegation and review boundaries

The implementer supplies the diff, permanent case, caller path, and measured
results. The parent reviewer inspects those sources and reruns the relevant
suite before accepting an exact revision. Further edits require review again;
an agent summary is supporting material, not an acceptance decision.

For larger stacks, use the [series and measurement gates](series.md) to produce
the review route and compare equivalent workloads. Fold, split, and exclusion
records account for history without pretending that a local manifest is an
owner approval.

The event ledger can deduplicate orchestration notifications and reject stale
run or revision updates:

```sh
make -C tools review-events
obj/review-events record --ledger obj/review-events.log \
  --event launch-1 --task implementation-1 --run run-1 \
  --revision "$(git rev-parse HEAD)" --expected-version 0 \
  --task-state running --process-state running
obj/review-events get --ledger obj/review-events.log --task implementation-1
```

Use the returned version for the next update; retry a notification with its
original event ID. A timeout leaves process ownership unresolved until an exit
is observed. The ledger does not control or terminate arbitrary processes.
It is editable local provenance, so it cannot establish test-first chronology
or trusted parent approval. Those require an external orchestration authority;
this repository's enforceable approval boundary is live GitHub review.

## Activate branch protection

`branch-protection.json` describes the desired settings; it does not apply
them. The read-only upstream check on 2026-10-18 found no Actions workflows and
no protection on `main` (the API returned 404), so owner activation is pending.
After the workflow is present on `main`, wait for a successful `native-ready`
run there. Then resolve the check's GitHub App identity and prepare the payload:

```sh
make -C tools review-policy
mkdir -p obj
obj/review-policy protection-payload \
  --repo wizardmanannan/CX16Agi \
  --policy tools/review/branch-protection.json \
  > obj/branch-protection.json
```

The repository owner reviews that payload and applies it with an admin-capable
token, then reads the settings back:

```sh
gh api --method PUT \
  repos/wizardmanannan/CX16Agi/branches/main/protection \
  --input obj/branch-protection.json
gh api repos/wizardmanannan/CX16Agi/branches/main/protection
```

Inspect the readback and run `make -C tools review-ready` against an open PR.
Readiness is active only after GitHub reports the exact required check and
protection settings; the local JSON and passing tests do not activate them.
