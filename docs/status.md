# Resume note

Current branch: `feature/reference-foundations`, based on `main` commit
`8453dfb`. The first implementation PR is being prepared. Before resuming, inspect
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

The source relocation/formatting is being revalidated before the first tested
implementation commit is recorded here. CI is defined but has not yet run on
the implementation head. This note does not identify the initial bootstrap
commit as a tested implementation.

Outstanding access limits:

- Ordinary Minecraft Launcher window capture timed out twice; no Play action
  or credential-file access occurred. Retry graphical reference testing when
  the capture tool works; this does not block storage/simulation implementation.
- `git ls-remote https://github.com/frankischilling/minecraft-c.wiki.git` returned
  `Repository not found`. Wiki pages are prepared under `docs/wiki/`; publication
  has not succeeded. The hosted wiki may need its first page created through
  GitHub before its Git repository is available.

Next actions: finish independent review, rerun relocated/formatted tests, inspect
staged material exclusions, commit/open PR for issues1/2/3/5, inspect exact-head
native CI, merge only after scoped acceptance, verify default-branch CI, and
close only completed scoped issues. Keep issue4 and all unimplemented parity
areas open. Then select a reference-backed NBT/text/storage increment under M2.

Unanswered behavior questions: canonical external audio provenance, unavailable
legacy skin/resource services, and dormant Sky-provider reachability require
research. Ask the owner for an exception only when unavailability is established
and it affects completion. No other owner decision currently blocks coding.
