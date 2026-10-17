# Working on minecraft-c

Read `docs/status.md` at the start of a continuation and inspect the current
worktree, GitHub PRs, and check results before relying on its resume information.

The target is the supplied, verified Minecraft Beta 1.7.3 client and compatible
dedicated server. Account authentication is the only approved gameplay exception.
Project-owned runtime code uses C17. Java is permitted only for local reference
probes and must not be required by the finished executables.

Organize source by system and subsystem: `src/<system>/<subsystem>/`, with
matching public headers under `include/mc/<system>/<subsystem>/` and tests under
`tests/<system>/<subsystem>/`. Group tools under `scripts/<system>/...`.
Add new subsystem directories with their implementation, rather than collecting
unrelated code into flat source folders. Apply `.clang-format` through
`scripts/development/format_code.py`; CI uses clang-format 18.

Before changing behavior, establish evidence from the original binary or a
controlled original-runtime observation. Track side, JAR hash, class, descriptor,
and bytecode offsets in `docs/reference/`; distinguish inference from observation.
For each implemented behavior update `docs/parity.md` with its issue, evidence,
verification command, result, and remaining limits.

Keep original JARs, extracted assets, decompiled Java, launcher files, credentials,
and personal worlds inside ignored local paths. Copy worlds before writable
reference tests. Inspect staged contents and packaging before publication.

Use the installed humanizer, git-human-workflow, and git-commit-author skills for
prose and Git operations. Follow their composition rules. Git operations belong
to the primary agent; parallel agents own explicitly assigned files only.

Use bounded parallel agents for independent research, implementation, or review.
Validate their results in the integrated build. Public tests must work without
Minecraft files; reference tests must report missing inputs as unavailable.

Keep broader parity issues open when a scoped implementation increment merges.
Record exact tested commits, CI results, blockers, and next actions in
`docs/status.md`. Full completion requires the original feature inventory,
native platform validation, save round trips, and the four-way interoperability
matrix, rather than a playable subset.
