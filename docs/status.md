# Resume note

Current branch: `feature/reference-foundations`, [PR #6](https://github.com/frankischilling/minecraft-c/pull/6),
based on `main` commit `8453dfb`. Before resuming, inspect
the live branch, open PRs, and exact-head CI rather than assuming this note is current.

Completed work in this increment:

- Verified supplied JAR fingerprints; pinned CFR0.152; decompiled both sides
  locally; bytecode anchors and an initial full-scope feature inventory recorded.
- Implemented C17 Java numerical/cast/Random helpers: 146 public checks and
  135768 exact Java/C records pass. Native MSVC and GCC/Clang WSL tests pass.
- Implemented local import with 16 synthetic tests; real import of 409 resources
  with hashes verified. Added system/subsystem folders and `.clang-format`.
- Original loopback server seed17 startup/save/stop passed. Original-client
  joins and the entire four-way multiplayer matrix remain untested.
- MIT approved by owner. Only account authentication is an approved gameplay
  exception. Original materials remain ignored/external.

Exact locally tested implementation commit: `8de9d81d317847c9bf25b58790e6baf261c8a199`.
After relocation and formatting, all three CTests passed with native MSVC and
with GCC/Clang WSL plus ASan/UBSan. Format checks passed with versions 18 and 22.
The supplied-JAR inspection, real 409-resource import, and original fresh-world
loopback save/stop smoke also passed at that commit. Local outputs are under
`.local/reference/`, `.local/assets-validation-8de9d81/` and
`.local/server-smoke-8de9d81/`. No gameplay comparison follows from these results.

Both independent reviews finished. The missing-reference exit77 CTest handling
and a stale relocated test command were corrected before the implementation
commit. Hosted CI is now pending on PR #6; inspect its final head before merge.

Outstanding access limits:

- Ordinary Minecraft Launcher window capture timed out twice; no Play action
  or credential-file access occurred. Retry graphical reference testing when
  the capture tool works; this does not block storage/simulation implementation.
- `git ls-remote https://github.com/frankischilling/minecraft-c.wiki.git` returned
  `Repository not found`. Wiki pages are prepared under `docs/wiki/`; publication
  has not succeeded. The hosted wiki may need its first page created through
  GitHub before its Git repository is available.

Next actions: inspect exact-head native CI on PR #6, resolve any failures,
merge only after scoped acceptance, verify default-branch CI, and
close only completed scoped issues. Keep issue4 and all unimplemented parity
areas open. Then select a reference-backed NBT/text/storage increment under M2.

Unanswered behavior questions: canonical external audio provenance, unavailable
legacy skin/resource services, and dormant Sky-provider reachability require
research. Ask the owner for an exception only when unavailability is established
and it affects completion. No other owner decision currently blocks coding.
