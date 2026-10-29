# World scheduler observations

`world.tick` checks ten captured observations from the unchanged client JAR.
The native contracts also exercise callback reentry, active-call retirement,
live WorldAccess list changes, retained diagnostics, and queue ownership.

The capture uses Temurin17.0.17 with `-Xint -Xverify:all`. `source-pins.json`
pins the complete bytes of twelve original classes. The replay checks their
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
  Entity count, and iterator selection. The native path executes the real
  biome sampler and then reports the missing spawn-list selection at PC290.

The C scheduler reports success only when the reached source path returns.
Mob spawn-list selection/construction, sleeping Player wake transitions, and
nonempty random Chunk work remain explicit dependencies. The reusable
scheduled drain dispatches through the existing concrete Block tick kernel;
unimplemented Block descendants still fail at their reached entry. These
observations do not qualify a complete loaded-player World tick.

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
