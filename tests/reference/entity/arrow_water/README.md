# Arrow water observations

This optional comparison executes the unchanged client archive's actual
`fd.g()V` World dispatcher and `sl.w_()V` Arrow tick. The native side uses
`mc_test_arrow_water_flight`. It needs a supplied original archive, its four
verified Java libraries, JDK 17, and the built native fixture. Java is used only
for reference research. No original archive, asset, class file, or private world
belongs in the repository.

```text
python -B tests/reference/entity/arrow_water/run.py --native build-windows/mc_test_arrow_water_flight.exe --client-jar b1.7.3.jar --library-dir /path/to/.minecraft/libraries --java /path/to/jdk17/bin/java --work-dir .local/arrow-water-reference
```

On Windows, supply the installed JDK's `java.exe`; the runner selects `javac.exe`
from the same directory. Linux uses `java` and `javac`. The installed JDK and OS
remain trusted runtime dependencies. This reference comparison has no default
CTest registration and does not launch a game window.

## Evidence and setup

The runner verifies client SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
`sl.w_()V` has 1,537 Code bytes with SHA-256
`f48dde96f4efaaa629c8b097f793f6ed6a2c561293110f9e2b5bd4e2f4e65f32`.
Its original bytecode calls the stored water flag at 1383, emits four particle
calls at 1459, assigns `.8f` drag at 1468, and applies drag and gravity afterward.
`sn.ag()Z` returns `bx`; `sn.k_()Z` expands the box vertically by
`-0.4000000059604645`, contracts each axis by `0.001`, and calls the actual World
material query. The original particle dispatcher invokes its `pm` receiver at
43. A null receiver throws `NullPointerException` there.

Each fresh process loads 25 genuine stored air Chunks through the normal saved
World constructor. Metadata supplies seed 23 and the constructor receives -99.
The Arrow uses actual position construction, heading `(1, 0, 0)` at `.6f` speed
and zero spread, and normal insertion. Quiet setters place source water at
`x=6..14`, `y=64..68`, `z=6..10`. Observer registration happens after placement
and insertion. The observer implements the original `pm` interface and only
records callback arguments and reads actual state. It throws on an unexpected
callback. No loaded Source field, Random, or geometry pool is repaired or reset.

The five original/native scenarios cover the first wet tick, eight ticks inside
the water volume, dry-to-water entry, water-to-air exit through real setters,
and a null second WorldAccess. The separate native test also covers an adapter
failure on the third flight bubble.

The eight-tick scene preserves a Beta quirk: the Arrow's `.5f` box becomes
vertically inverted by the water query. The Y-cell loop is empty at ticks 5–7,
even inside the water volume. Tick 8 enters water again and emits a new Base
splash. Actual callback counts are `4, 4, 4, 4, 0, 0, 0, 27`.

## Comparison and limits

The helper retains every declared original Entity, Arrow, bounds, and World
instance field, both complete Random triples, and ordered World/Chunk lists.
The runner checks that field inventory against the unchanged archive. The
numerical native comparison covers 13 returned/failed snapshots, each with
26 state/Random fields, and 79 callback records with 16 common columns.

The recorded Windows cohort has 312 exact state cells and 26 differing cells
from the independently constructed Random state and cached Gaussian. Callback
records have 994 exact cells and 270 differences from those Random values and
the Base splash's random offsets, vertical bubble speed, and sound pitch.
Every pair and every difference remains in the report. The finite deterministic
state, bounds, angles, water sequence, and flight-bubble arguments match exactly.
Sound records use explicit zero placeholders for absent velocity arguments;
particle records use zero placeholders for absent volume and pitch arguments.
Those placeholders are record format, not additional original callback operands.

The comparison does not synchronize constructor entropy or claim whole-method,
heap, rendered-bubble, audio-device, sustained-gameplay, pickup, rider, victim
damage, or FPS equivalence. Native callback classification and checked cleanup
have their separate regression assertions. The null-receiver scenario observes
the actual original exception and the native Source classification.

Each compiler/JVM/native process has bounded complete stdout/stderr capture.
Reports retain commands, exit/reap/pipe results, raw observations, input/product
hashes, and complete comparisons. Helpers and pristine inputs stay unchanged.
The original's six initial stored files must remain byte-identical; only its
actual eight-byte session lock may appear. Failed attempts and their payloads
stay available. An uncertain child return defers later guards and launches.
