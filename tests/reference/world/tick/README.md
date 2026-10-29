# World scheduler observations

`world.tick` checks eleven captured observations from the unchanged client JAR.
The native contracts also exercise callback reentry, active-call retirement,
live WorldAccess list changes, retained diagnostics, and queue ownership.

The capture uses Temurin17.0.17 with `-Xint -Xverify:all`. `source-pins.json`
pins the complete bytes of seventeen original classes. The replay checks their
actual code origins, the JAR and library hashes, the matching Java/Javac
version, and input/helper hashes before and after execution. It saves bounded
stdout/stderr and a JSON receipt under the requested work directory.

The observations cover these source paths:

- `fd.l()` with an empty Player list: weather, the sleeping predicate, skipped
  spawning, one provider maintenance pass, sky subtraction, time update,
  scheduled drain, and empty random Chunk work.
- Empty-list sleeping with time `-24001`: the next-day calculation stores 0
  before the ordinary increment stores 1. The wake step resets rain and thunder
  between those stores.
- A zero autosave period: the long remainder throws before the time store.
- `fd.a(Z)Z`: future/forced records, the 1000-record cap, size mismatch, and
  removal from E then F before a reached null-provider failure.
- `cq.a(World,ZZ)`: one RAW concrete Player at X=-0.25/Z=32 produces 289
  candidate Chunks; its first HashSet element is (-3,-3). The Java fixture
  installs an authored `xv` subclass that throws at the first biome lookup.
  The original `cq` method performs the candidate construction, category gate,
  Entity count, and iterator selection.
- A second spawn fixture reaches the selected Pig constructor. Seed 529 selects
  weight ticket 18 of 40, initial point (-40,64,-36), and first trial (-39,64,-37).
  The ten bounded World Random calls leave state `98aeeda4389e`. This includes
  both `nextInt(1)` calls. The Java fixture uses authored flat World predicates,
  lets the unchanged `cq` invoke the actual Pig constructor, records World
  Random state immediately before the yaw draw at 808, and throws at insertion.
  That constructor performs no World Random calls. Entity construction effects
  and the later yaw draw are outside the compared prefix.

The native constructor fixture reads an authored, populated grass-layer Chunk
through the actual Store/provider path. It checks one cache publication, no
Entity insertion, no leaked Chunk lease, and unchanged region-file bytes after
the reached constructor frontier at 773. The source biome and weighted Lists
remain real; the fixture does not replace them with empty Lists or a no-spawn
result.

The C scheduler reports success only when the reached source path returns.
Concrete Mob initialization/construction, sleeping Player wake transitions, and
nonempty random Chunk work remain explicit dependencies. The reusable
scheduled drain dispatches through the existing concrete Block tick kernel;
unimplemented Block descendants still fail at their reached entry. These
observations do not qualify a complete loaded-player World tick. Spawn-list
selection, all candidate/group loops, support/material checks, nearest-player
lookup, and the float spawn-distance test execute in source order before that
frontier. Iterator reentry checks the captured wrapping modification count.

The candidate sets reuse the captured JDK17 HashSet ordering for distinct
`yy.hashCode()` values. A distinct coordinate pair with an already-used hash
reports a native limit before insertion. The set also has a 65536-entry native
ceiling. These boundaries never truncate a successful source invocation.

Run the native contracts:

```text
python -B tests/reference/world/tick/run.py --native <mc_test_world_tick> --work-dir <work>
```

Add `--client-jar <b1.7.3.jar> --library-dir <libraries> --java <JDK17/bin/java>`
to rebuild the authored helper and replay the unchanged original methods.
`observations.json` contains the resulting rows; every native row reads the
current World or owning queues.
