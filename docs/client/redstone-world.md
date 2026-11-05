# Redstone World rendering

World render type 5 now enters the actual `CV.g(Block,III)` renderer through
the shared RenderBlocks dispatcher. It reads metadata, invokes the Block's
metadata texture virtual before considering the override, captures brightness,
and installs the power color. Connection checks retain their original West,
East, North and South order and short circuits.

The floor chooses its cross or straight texture geometry from those connection
results. A fresh overhead query then controls four independent upright strips.
Each strip rereads its support, upper Block ID and live Redstone singleton;
capability alone does not qualify an upright strip. Each floor or upright
surface emits a power-colored quad followed by its gray overlay. A successful
call appends 8 to 40 source vertices, expanded by NW to 12 to 60 triangle
vertices, or 96 to 480 words. It preserves Block bounds and unrelated CV state
and leaves the enclosing batch open.

Coordinates preserve the original mixture of Java integer wrapping, float
narrowing and double operations. UV edges use float arithmetic before widening;
overlay V offsets use double arithmetic. The signed-coordinate cases exercise
the resulting differences at four float precision boundaries with genuine
far-coordinate World Chunks and EW caches.

The original packed Chunk keys alias two of those far requests to already
loaded base Chunks. An explicit original owner receipt records twelve requests
and ten unique returned owners. Native preparation follows the same request
slots and aliases, and retires each owner once. The earlier fixture's coordinate
uniqueness assumption failed before rendering; no World coordinate or cache key
is changed to satisfy that test.

## Connectivity and receivers

The actual `SM.e(XP,IIII)` helper first reads the Block ID and then the live
Redstone singleton. Other non-air Blocks resolve through the captured Block
array and their actual power-capability virtual. Both repeater singletons and
the actual JJ direction table participate in the original order.

Each helper captures its XP argument once. Brightness also forwards its
captured XP through nested Stairs calls and both Fluid reads. A callback that
changes `CV.c` during Registry initialization cannot retarget that already
entered child. The next direct renderer query reads the changed field afresh.
Registry active use records its source PC before resource callbacks can run.

Solid-support queries use the admitted implementation's actual operation:
`EW.h` uses `Material.c`, while `World.h` uses `Material.h`. Typed failures retain
the lower query, source PC, Registry cause and native policy. Leaves and null
material controls distinguish these routes without replacing either owner.

## Evidence and limits

The [reference packet](../../tests/reference/client/redstone_world/README.md)
contains 100 direct source invocations in five separate fresh cohorts. Each has
two ordinary VMs and one read-only JDI VM, with equal full stdout and unchanged
source/helper inputs. The trace records 17,496 events, 3,415 EW queries, 2,113
static reads and six reached null failures. Every backing word outside the
observed append extent is checked against the same target's prior memory.

The native runner compares the literal input, effect, backing and completion
records, with separate corruption controls and preserved executable products.
Its native-only mode checks fresh null/foreign rejection and owner retirement.
A constructed internal stress pairs a ready World Registry's Block with a
separate cold code Registry, using that Registry's resource callbacks to change
CV.c during brightness. This split-context setup tests argument retention;
ordinary CV calls require matching defining owners. It establishes no original
Java cross-loader or ordinary cold-call behavior.

Windows MSVC Release and Linux Clang Debug with ASan/UBSan pass all seven focused
tests. Each host matches all 100 source calls, 310 literal input/effect/backing
and completion records, plus the separate signed-owner receipt. The five replay
controllers reject 371 corruptions; the callback controller rejects another 14.
The packet has 26 integrity and semantic test methods. The seven CTests take
9.89 seconds on Windows and 12.24 seconds on Linux; these test durations are not
gameplay or FPS measurements. Nine neighboring renderer regressions also pass
on each host.

The native dispatcher control runs after the 56-call floor cohort in the same
open batch. It reauthors the first declared scene, enters the actual type 5
dispatcher, checks return PC 146 and compares 96 appended words with the first
direct row. Seventy assertions cover this join and its retained state. It adds
no original whole-dispatcher observation.

The previous linked runtime stops at the type 5 dispatcher dependency before
entering `g`. That retained baseline is a dispatcher frontier, not an original
renderer result. The callback regression also retained its earlier failure:
Registry initialization exposed parent PC 51 instead of active-use PC 5. The
fixed helper records PC 5 before entering initialization.

Item Redstone use and its can-place join remain separate implementation work.
These CPU comparisons do not establish loaded GL rendering, original pixels,
audio, power-propagation parity, general focus reliability or FPS improvements.
