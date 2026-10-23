---
deferred_work_file: '{implementation_artifacts}/deferred-work.md'
specLoopIteration: 1
---

# Step 4: Review

## RULES

- YOU MUST ALWAYS SPEAK OUTPUT in your Agent communication style with the config `{communication_language}`
- Review subagents get NO conversation context.
- Select review models under the current user and runtime policy.

## INSTRUCTIONS

Change `{spec_file}` status to `in-review` in the frontmatter before continuing.

### Construct Diff

Read `baseline_commit` and the `baseline_worktree` record from `{spec_file}`. Construct `{diff_output}` from this task's changes relative to that pre-edit state. Separate pre-existing changes and later unrelated edits; do not exclude an entire previously dirty file if this task also changed it. Compare the relevant hunks against the saved contents. If the baseline is missing or ambiguous, report that review limitation and do not claim all HEAD differences or untracked files as this task's work. Never reconstruct a baseline by resetting the working tree.

Do NOT `git add` anything — this is read-only inspection.

### Review

Use independent reviewers only when delegation is authorized and available. Otherwise perform the three review lenses sequentially, disclose the lack of independent review, and continue. Follow any separately required cross-review policy.

- **Blind hunter** — receives `{diff_output}` only. No spec, no context docs, no project access. Read the installed `bmad-review/references/lens-adversarial.md` review lens.
- **Edge case hunter** — receives `{diff_output}` and read access to the project. Read the installed `bmad-review/references/lens-edge-case-hunter.md` review lens.
- **Acceptance auditor** — receives `{diff_output}`, `{spec_file}`, and read access to the project. Must also read the docs listed in `{spec_file}` frontmatter `context`. Checks for violations of acceptance criteria, rules, and principles from the spec and context docs.

### Classify

1. Deduplicate all review findings.
2. Classify each finding. The first three categories are **this story's problem** — caused or exposed by the current change. The last two are **not this story's problem**.
   - **intent_gap** — caused by the change; cannot be resolved from the spec because the captured intent is incomplete. Do not infer intent unless there is exactly one possible reading.
   - **bad_spec** — caused by the change, including direct deviations from spec. The spec should have been clear enough to prevent it. When in doubt between bad_spec and patch, prefer bad_spec — a spec-level fix is more likely to produce coherent code.
   - **patch** — caused by the change; trivially fixable without human input. Just part of the diff.
   - **defer** — pre-existing issue not caused by this story, surfaced incidentally by the review. Collect for later focused attention.
   - **reject** — noise. Drop silently. When unsure between defer and reject, prefer reject — only defer findings you are confident are real.
3. Process findings in cascading order. If intent_gap or bad_spec findings exist, they trigger a loopback — lower findings are moot since code will be re-derived. If neither exists, process patch and defer normally. Increment `{specLoopIteration}` on each loopback. If it exceeds 5, HALT and escalate to the human.
   - **intent_gap** — Root cause is inside `<frozen-after-approval>`. Correct only the affected changes from this task against the captured pre-edit state. Never reset or discard unrelated work. Loop back to the human to resolve. Once resolved, read fully and follow `./step-02-plan.md` to re-run steps 2–4.
   - **bad_spec** — Root cause is outside `<frozen-after-approval>`. Before reverting code: extract KEEP instructions for positive preservation (what worked well and must survive re-derivation). Correct only the affected changes from this task against the captured pre-edit state. Never reset or discard unrelated work. Read the `## Spec Change Log` in `{spec_file}` and strictly respect all logged constraints when amending the non-frozen sections that contain the root cause. Append a new change-log entry recording: the triggering finding, what was amended, the known-bad state avoided, and the KEEP instructions. Read fully and follow `./step-03-implement.md` to re-derive the code, then this step will run again.
   - **patch** — Auto-fix. These are the only findings that survive loopbacks.
   - **defer** — Append to `{deferred_work_file}`.
   - **reject** — Drop silently.

## NEXT

Read fully and follow `./step-05-present.md`
