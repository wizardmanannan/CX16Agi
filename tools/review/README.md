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

The plan takes absolute dependency roots from `CC65_HOME` and
`X16_EMULATOR_SOURCE`. Export the same installed roots used by the native
suites before capture and verification; CI supplies its pinned installations.

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
