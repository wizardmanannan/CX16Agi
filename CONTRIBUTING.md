# Contributing

This port is under construction and supports AGI 1 and AGI 2 games only.

- **Before filing:** read the pinned [Start here](https://github.com/wizardmanannan/CX16Agi/issues/37)
  issue. It lists the supported builds, the controls, every open issue, and what a report needs.
- **Bug reports:** use the Bug report form. A report without a game version, a port revision, and a
  route to the scene cannot be reproduced.
- **Fixes:** one topic per pull request. Use `Refs #N` for partial coverage and `Fixes #N` after
  checking all of an issue's acceptance cases. Include the tested commit, game release, runtime,
  rerun command and result. Verify the result again on GitHub main after integration.
- **Reference behaviour:** the AGI specification, Sierra's recovered interpreter source, AGILE, and
  ScummVM are pinned in the Start here issue. They show what a compatible engine does; they do not
  replace evidence from this code.

## Issue body format

The forms collect the player report in this order. When preparing or expanding an issue,
use the body below. For a feature, lead with the goal and current gap, and replace
the reproduction with an example or measurable workload when one fits. Leave optional
technical sections out until there is evidence to add. A player can file a report without
diagnosing it.

`GAME AREA: symptom` is the title for a bug. Use `AREA: proposal` for a feature.

```markdown
Describe the scene and symptom in a sentence a player can follow.

| Quick facts | |
|---|---|
| Game / build | |
| Scene | |
| Blocks play? | |
| Difficulty | |
| Status | |
| Workaround | |

### Reproduce it

1. Start from a reachable point. Include prerequisites and the approximate time.
2. Give the exact keys, commands or actions.

**Expected:** Describe what the player should see or be able to do.
**Actual:** Describe what happened.

### Evidence

| Confidence | Finding | Source |
|---|---|---|
| Confirmed / Suspected / To verify | | |

<details><summary>Technical trace and game-side evidence</summary>

Include the relevant logic, captures, logs or upstream code links.

</details>

### Reference implementation

Link a relevant AGI specification, interpreter or same-release comparison when available.

### Fix direction

### How to verify

### Acceptance

- State an observable result that would pass the reproduction or workload.

### Related

- Link the relevant issue or candidate PR.

<details><summary>Original report</summary>

When rewriting an existing report, preserve the original wording here.

</details>
```

Record issue status using the [evidence fields in the issue map](https://github.com/wizardmanannan/CX16Agi/issues/37#evidence-on-each-issue).
Use a commit permalink for upstream code findings. A code finding and a reproduced symptom
are separate claims. A candidate PR stays a candidate until the result is verified on GitHub main.
