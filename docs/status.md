# Resume note

Current branch: `chore/project-conventions`, based on `main` commit
`ed67a55d831372c208bcf87c2cc3c6b75a1b8029`. The active scoped task is
[#7](https://github.com/frankischilling/minecraft-c/issues/7): repository text
preferences and current pinned CI actions. Inspect the live branch, PRs and
exact-head CI before relying on this note.

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
commit. [PR #6](https://github.com/frankischilling/minecraft-c/pull/6) is merged.
Its final head `6980164b218e9139f667393950d0f3ed6dc5ff76` passed all four jobs in
[run39021245762](https://github.com/frankischilling/minecraft-c/actions/runs/39021245762).
The resulting `main` commit `ed67a55d831372c208bcf87c2cc3c6b75a1b8029` also passed
all four jobs in [run39021329782](https://github.com/frankischilling/minecraft-c/actions/runs/39021329782).
Native Windows MSVC19.44 and native Linux GCC13.3/Clang18.1.3 each passed all
three CTests, with no reference skips. The default public-only Linux build
also passed both tests without Java discovery. Deprecation annotations in the
first workflow motivated the pinned-action update in issue7.

Outstanding access limits:

- Ordinary Minecraft Launcher window capture timed out twice; no Play action
  or credential-file access occurred. Retry graphical reference testing when
  the capture tool works; this does not block storage/simulation implementation.
- `git ls-remote https://github.com/frankischilling/minecraft-c.wiki.git` returned
  `Repository not found`. Wiki pages are prepared under `docs/wiki/`; publication
  has not succeeded. The hosted wiki may need its first page created through
  GitHub before its Git repository is available.

Next actions: review and validate issue7's workflow/text changes, merge only
after exact-head CI, verify default-branch CI, and close the scoped milestone.
Keep issue4 and all unimplemented parity areas open. Then implement the first
NBT/text/storage increment under M2; the ignored `.local/nbt-next.md` records
both-side bytecode findings for the codec issue, including modified UTF-8,
IDs0..10, empty-list behavior and original-class differential design. That
differential has not yet been executed. Carry publishable findings into the
next reviewed code increment; no original source should be published.

Unanswered behavior questions: canonical external audio provenance, unavailable
legacy skin/resource services, and dormant Sky-provider reachability require
research. Ask the owner for an exception only when unavailability is established
and it affects completion. No other owner decision currently blocks coding.
