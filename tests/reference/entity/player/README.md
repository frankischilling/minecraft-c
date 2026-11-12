# Local Player constructor reference

This opt-in harness runs the supplied unchanged client JAR on the CPU, first
normally and then under a read-only JDI observer. It compares the observations
with `mc_player_construction_probe`. No original classes, assets or captured
answers are distributed here. Java is reference tooling; the runtime remains C17.

Run from the repository root with an explicit matching JDK 17 `java` and `javac`:

```text
python -B tests/reference/entity/player/compare_construction.py --client-jar /private/b1.7.3.jar --native /build/mc_player_construction_probe --java /jdk17/bin/java --javac /jdk17/bin/javac --work-dir /ignored/player-construction-reference
```

On Windows, supply the actual `.exe` paths, quoting paths containing spaces.
The same arguments work on Linux. `--source-root` selects an explicit matching
native source tree; otherwise the runner finds the repository ancestor. Source
pins must match that tree. Do not use another tree's executable after an Entity
ABI change. The report records the actual native executable and JDK hashes.
Unset `JAVA_TOOL_OPTIONS`, `_JAVA_OPTIONS` and `JDK_JAVA_OPTIONS`; nonempty ambient
options are rejected. `CLASSPATH` is removed and every classpath is explicit.
Python optimization is rejected because the authored JDI validator uses assertions.

Every invocation creates a fresh unique child of `--work-dir`. Helpers, private
resources, Worlds, raw stdout/stderr, class-init logs, receiver projections and
reports remain there. Existing runs are never reused or overwritten. The three
allowlisted resources extracted for the native fixture are `lang/en_US.lang`,
`lang/stats_US.lang` and `achievement/map.txt`; sizes and hashes are pinned.
Keep these generated directories ignored. No generated output belongs in Git.

`source-pins.json` binds the entire supplied JAR digest, 187 actual classfiles
and 1,862 concrete Code bodies. `provenance.json` pins all execution sources,
the three compiled authored helper classes and the 15 native increment files.
Only the three authored helpers can occupy the fresh helper classpath; the
target's other entry is the validated JAR. Both target VMs additionally report
each selected class's actual resource digest and runtime code-source origin.
Both class-init logs must contain the exact 187-class original closure and must
exclude Minecraft bootstrap, the selected device classes, LWJGL and Paulscode.
The JDI observer independently verifies the four methods' live JVM bytecodes.

The target constructs real `fd(new wy(), ..., xa.a(0), 1234L)`, producing the
normal `rh` dimension and actual original provider. Sessions are genuine `gr`
objects. Spawn uses the original public operation; remote state and constructor
arguments are explicit case inputs. Null Minecraft is stored and remains unread
on this source path. No fake Player/World, replacement provider, Unsafe, class
rewrite or target-method invocation by JDI is used. JDI reads the actual JDK 17
String backing bytes and coder to preserve embedded NUL and a lone surrogate.
Suspension can affect timing; deterministic state is checked against a separate
ordinary VM run.

The eight authored inputs preserve six completed `dc -> gs -> ls -> sn`
constructors, null Session at `dc` PC103 and null World at `gs` PC68. JDI records
71 checkpoints, including both real unpublished failed receivers, the watcher
hook before Living initialization, four Math poststore boundaries, completed
Living state and actual container construction. The comparison checks 912
deterministic successful fields, 36 bounds fields, 1,680 Slot cells, 18 aliases,
303 failed-prefix fields and 12 failed-prefix bounds fields.

The target's real shared Math holder is never seeded or replaced. Its observed
internal state is cloned independently to recover four actual `nextDouble`
returns, checked by Python LCG replay and JDI boundaries. These 32 recovered
values enter only the native probe's separately labelled scalar formula output;
24 successful-case formula values are compared. The production constructor has
its independent real application Math stream and receives no tape. Absolute
Entity IDs, default instance Random seeds and equality of uncontrolled Math
streams are excluded. Ordinary public constructor tests separately check its
four-draw path. The native fixture uses its real Store/World owners; matching
original SaveHandler persistence or World entropy is outside this field scope.

Original stdout is preserved: `16 achievements`, `151 recipes`, then eight CASE
lines. The native fixture's actual banner order is `151 recipes`, then
`16 achievements`. Only `splitlines()` handles platform line terminators; banner
order and content are asserted independently. Compiler, ordinary target, JDI
controller/target and native comparison must exit 0 with empty stderr.

The authored-only protocol test requires no JAR:

```text
python -B tests/tools/entity/test_player_construction_probe.py --executable /build/mc_player_construction_probe --work-dir /ignored/player-construction-protocol
```

It creates authored language/achievement resources and tests six valid tapes
and 26 malformed or missing tapes. The native CLI remains
`resources worlddir drawtape outputfile`. A tape is at most 4,096 bytes and has
exactly 32 tokens of 16 ASCII hex digits, separated only by space/tab/CR/LF.
Raw bits must be below `0x3ff0000000000000`: positive zero and finite values
below one are valid; negative zero, negative values, NaN and infinity fail.
Invalid/missing tape must exit exactly 2, emit only
`Player probe input: invalid draw tape` on stderr, emit no stdout and create
neither the output file nor World. A crash, sanitizer report or other diagnostic
cannot count as successful rejection. Valid runs also check all eight constructor
rows, deterministic state across tapes and all 32 scalar formula outputs.

This finite client constructor evidence does not qualify gameplay, ticks,
movement, World insertion, rendering, input, audio, networking or server Player
construction. Raw original observations are regenerated locally and are never
published as expected-output fixtures. `validation.json` retains the initial
staging measurements. Later integrated Windows and sanitized Linux results are
recorded in [Player construction](../../../../docs/client/player-construction.md).
