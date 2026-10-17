# Review series and performance gates

The focused native C suite covers stacked review series and image metric
comparisons:

```sh
make -C tools -f review-series.mk review-series-test
```

It builds under `obj/review-series` and creates its own temporary Git projects
and evidence. Use `review-series-core-test` or `review-metrics-test` to run one
area. The temporary inputs are removed after both passing and failing runs.

## Review series

Build a route from exact Git commits and sealed review evidence with:

```sh
obj/review-series/review_series route --repo REPOSITORY --manifest series.txt --output NEW_DIRECTORY
```

The output directory must not already exist. A `review-series-v1` manifest is
newline-delimited and uses these records:

```text
review-series-v1
range|BASE_SHA|HEAD_SHA
pr|PR_ID|BASE_SHA|HEAD_SHA|EVIDENCE_DIRECTORY|EVIDENCE_SHA256
requires|PR_ID|PREREQUISITE_PR_ID
commit|COMMIT_SHA|PR_ID
fold|COMMIT_SHA|EARLIER_KEPT_COMMIT_SHA|REASON
split|COMMIT_SHA|REPOSITORY_PATH|PART_ID
exclude|COMMIT_SHA|REASON
```

Each `pr` record points to the exact evidence directory and its SHA-256 over
`evidence.v2`. The evidence must verify against that PR's real Git head and
tree. An absolute evidence path is used as written; a relative path resolves
beside the manifest. PR prerequisites form a DAG. A PR without prerequisites
starts at the series base; a stacked PR starts at a declared prerequisite head
and includes all declared prerequisite heads. Every commit in
`BASE_SHA..HEAD_SHA` must be accounted for exactly once and belong to its
assigned PR's real commit range. `HEAD_SHA` must be one of the evidence-bound
PR heads.

Commits without decisions appear in the route as incremental patches. A fold
must point to an earlier kept commit in the same PR. A split assigns every
changed path exactly once across at least two distinct part IDs. An exclusion
requires a reason. These records organize a review route; they do not certify
that a human approved its contents.

The route index and patches establish local identity and integrity for the
supplied evidence. They do not grant approval or claim fresh test verification.
Rerun the required suites at publication.

## Image metric comparison

`review_metrics gate` runs one pinned native runner against baseline and
candidate images with the same workload input. Each image manifest is:

```text
review-image-v1
kind|release
commit|COMMIT_SHA
tree|TREE_SHA
image_sha|IMAGE_SHA256
```

`kind` is `release` or `test`. The requested image kind selects which budget
category applies: release-kind comparisons reject test images, while test-kind
comparisons can use test images. These labels do not qualify an artifact as a
release. The gate verifies the image digest and that the declared commit and
tree are real Git objects, but it does not establish that the image was built
from that commit. Source-to-image derivation requires a reviewed build/capture
that links the source revision to the image, or an independent rebuild. The
candidate commit must descend from the baseline, and the checkout must be clean
at the candidate commit.

Run it with:

```sh
obj/review-series/review_metrics gate \
  --repo REPOSITORY --input WORKLOAD_INPUT --workload graphics-clear \
  --baseline-image BASELINE_IMAGE --baseline-manifest BASELINE_MANIFEST \
  --candidate-image CANDIDATE_IMAGE --candidate-manifest CANDIDATE_MANIFEST \
  --baseline-runner RUNNER --candidate-runner RUNNER --proof-kind release \
  --max-bytes N --max-cycles N --max-latency-ns N --scratch-dir obj
```

The runner executable receives `--input FILE --image FILE --output FILE
--workload ID`. It must produce one newline-terminated record:

```text
review-metric-v2|WORKLOAD|INPUT_SHA256|TOOLCHAIN_SHA256|CYCLES|LATENCY_NS|emulator-cycle-counter
```

The gate compares the runner and toolchain identities, the input digest, the
cycle source, and the bytes written to `--output`. It checks that baseline and
candidate outputs match. It also rechecks the input, executables, images,
manifests, candidate checkout, and output files after execution. Runner time
and combined output are bounded; timeout and output overflow fail the gate.
The result reports `image_kind` and states that manifest commit/tree declarations
do not establish source-to-image derivation. The CLI's `--proof-kind` option
selects this image-kind budget category; it is not a build or approval claim.

For the linked graphics-clear workload, reported latency is nominal emulated
time calculated as `ceil(cycles * 1,000,000,000 / 8,000,000)`, using the
emulated 8 MHz CPU clock. It is not host wall time or a hardware latency
measurement. The runner-reported latency and toolchain hash are checked for
format and agreement across runs, but the gate cannot independently attest
them. The route is a local comparison, not a global optimality or approval
claim. It also does not establish source-to-binary derivation, independent
measurement attestation, repository-owner approval, or fresh verification.

Candidates must remain within byte, cycle, and latency budgets. A local Pareto
improvement needs no additional text. For an in-budget candidate that does not
locally Pareto-improve, an optional committed proposal can document the
tradeoff. Its `--tradeoff` path is repository-relative and the file must be
committed at the candidate commit:

```text
review-metrics-tradeoff-v1
review_reference|PULL_REQUEST_OR_REVIEW_REFERENCE
reason|WHY_THIS_TRADEOFF_IS_PROPOSED
```

The reference and reason are proposal text. A local file cannot establish
repository-owner approval or satisfy branch protection; obtain and verify any
required approval through the repository's publication process.
