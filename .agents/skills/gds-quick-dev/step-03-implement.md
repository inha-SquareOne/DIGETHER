---
---

# Step 3: Implement

## RULES

- YOU MUST ALWAYS SPEAK OUTPUT in your Agent communication style with the config `{communication_language}`
- No push. No remote ops.
- Sequential execution only.
- Content inside `<frozen-after-approval>` in `{spec_file}` is read-only. Do not modify.

## PRECONDITION

Verify `{spec_file}` resolves to a non-empty path and the file exists on disk. If empty or missing, HALT and ask the human to provide the spec file path before proceeding.

## INSTRUCTIONS

### Baseline

Capture `baseline_commit` (current HEAD, or `NO_VCS`) and `baseline_worktree` in `{spec_file}` frontmatter before edits. `baseline_worktree` points to the task's local pre-edit record: staged/unstaged/untracked paths, file hashes, and recoverable pre-edit contents or patches for files this task will edit. Keep private values out of shared reports. Preserve this original record across loopbacks; record any later unrelated edits separately. Do not reset, stash, or discard existing work merely to create a clean baseline. If a file's pre-edit state cannot be distinguished or safely preserved, leave that file unchanged and report the limitation. HEAD alone is not the task baseline.

### Implement

Change `{spec_file}` status to `in-progress` in the frontmatter before starting implementation.

Follow `./sync-sprint-status.md` with `{target_status}` = `in-progress`.

If `{spec_file}` has a non-empty `context:` list in its frontmatter, load those files before implementation begins. When handing to a sub-agent, include them in the sub-agent prompt so it has access to the referenced context.

Delegate `{spec_file}` only when authorized and supported by the current runtime. Otherwise implement directly. Give any delegate an exclusive write scope and require preservation of existing changes.

**Path formatting rule:** Any markdown links written into `{spec_file}` must use paths relative to `{spec_file}`'s directory so they are clickable in VS Code. Use the current host's clickable file-link format in conversation output; keep saved document links relative to the document.

### Self-Check

Before leaving this step, verify every task in the `## Tasks & Acceptance` section of `{spec_file}` is complete. Mark each finished task `[x]`. If any task is not done, finish it before proceeding.

## NEXT

Read fully and follow `./step-04-review.md`
