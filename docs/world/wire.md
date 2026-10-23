# Synchronous wire lifecycle

Wire addition, removal and neighbor dispatch now run the actual strength-update
closure through the existing GAME World services. The implementation lives in
`src/world/runtime/mutation/wire/`; collection ownership stays in the defining
Registry's actual wire Block.

The recursive update disables the shared wire output flag for its external-power
query, then stores true unconditionally. It preserves horizontal query order,
repeated support reads and fresh center-metadata reads after recursive updates.
Metadata changes use the notifying setter with World.o true, followed by a
separate range observation and an unconditional false store. A failed reached
operation retains earlier mutations and source flags.

Seven fresh position attempts accumulate notifications in the actual shared
[position Set](../reference/wire-position-set.md). Draining first copies a shallow
snapshot, then clears the Set and invokes the original ordered World-neighbor
operations. Nested callbacks can refill or drain the Set while an outer snapshot
still owns its keys. Large child diagnostics use checked heap workspaces and
retain current wire sites alongside the deepest concrete hook cause. Query and
acquisition children use the existing diagnostic fields. Duplicating their full
graph enlarged every ancestor and overflowed two Windows regression stacks; the
small wire sites and independently allocated query workspaces avoid that growth.

## Native verification and remaining evidence

`world.wire_lifecycle` constructs real client and base-server Worlds with loaded
Chunks, registry-created Blocks and authored minimal translation resources.
Its prepared substrate is a test input, not natural terrain. Public notifying
setters exercise power rise/decay, a 25-cell path past attenuation range,
removal/reinsertion, vertical steps, blocked overhead and a loop. Support loss
publishes a real ItemEntity with one redstone item. Separate native controls
check remote guards and exact partial effects at failed power, observer and
collection-budget operations. Controls restore nullable inputs before checking
results; they provide no successful replacement World or observer callbacks.

`world.wire_multichunk` adds 350 checks in 25 cases across four real Worlds:
client, base server, dedicated and a second base server. Three loaded Chunks per
World cover negative coordinates and crossings at x=0 and x=16. The three server
Worlds share their defining Registry. Cases cover the dedicated loading gate,
cache hits/misses, suppression flags, rise/decay, removal/reinsertion and unchanged
payloads after repeated metadata writes. Dedicated loading is an explicit test
input; server startup remains unfinished.

A retained snapshot survives a second World's refill and drain of the shared
Set. The test also checks the intermediate update boundary: the first wire has
strength 15 and seven positions remain pending; draining their actual World
notifications completes propagation. This is sequential shared-owner coverage.
Callback-driven cross-World reentry remains required. Both native cases check
unchanged World Random where no drop occurs, source flags, empty notification
storage after successful drains and balanced calls/leases/snapshots.

Both cases pass on MSVC and sanitized GCC/Clang. Code checkpoint `2150299`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40030317783):
203 MSVC, 205 GCC and 205 Clang public/JDK tests, with an actual Passed line for
every registered test and no failures or skips. Formatting checks all 583
C/header files; controller mirrors and lifecycle interval controls pass. The
synthetic checkout `e7d539dcebc4e4a7a0ce8cde37b25f27ce075ba8` has the same
complete tree as that commit. Earlier failed development attempts retain their
own records. The existing-World observations below retain separate development
attribution and incomplete qualification.

These are native contracts. Original executed World-level wire equality,
intermediate notification traces, wider persistence and failure schedules remain
required. The collection's default identity-source boundary also remains open.
Client display particles and other redstone Blocks retain their own unfinished
behavior.

Run the public native case without Java or original assets:

```sh
ctest --test-dir build --output-on-failure -R '^world[.]wire_(lifecycle|multichunk)$'
```

## Existing-World observation

`mc_wire_world_probe` opens an existing copied save through the production client
or base-server constructor. The bounded seed-23 profile requires spawn (0,64,0)
and populated stored Chunks. Its [input plan](../../tests/reference/world/wire/line.plan)
contains only constructor parameters, operations and coordinates. No expected
array, strength, Random output or collection state is supplied. Construction
replaces the copy's session lock; the plan requests no save or game tick.

The producer emits an initial snapshot, before/result/after records for each
operation and a terminal cleanup record. Observations include complete Chunk
arrays and flags, native address aliases, cache membership/insertion order,
lighting, scheduled entries, wire collection structure and Random states with
eight continuations drawn from value copies. Native addresses require explicit
structural adaptation to Java owners; they are not JVM object identities. The
header records the authored minimal translation resources and finite omissions.
Entity-bearing, tile-bearing, generated and unpopulated Chunks fail admission.

Development observations executed the same 15 operations on both supplied
originals and on MSVC, GCC and Clang. On each compiler, a declared boundary
projection matches 600 complete arrays across 62 boundaries, Chunk flags, cache
membership/insertion order, ordered lighting regions and drained wire Set
size/capacity/threshold/modification counts. World and cave Random states differ
at 124 owner boundaries per compiler; those absolute differences are retained.
Each run's Random owners remain unchanged through this no-drop plan. These
finite deterministic matches do not establish full World-state equivalence.
Six Windows admission controls reject malformed/oversized plans, integer and
command-count overflow, missing prepared metadata and an existing output file.
Each preserves its input arena; existing output bytes remain unchanged. Two
comparator controls reject disabled validation assertions and a wrong-side
original capture. Expected nonzero process receipts remain recorded as such.

Passive original traces record 2,087 selected calls per side across all 15
operations and all eleven selected wire/World roles, with balanced returns and
no observed exceptions. Each JDI method's Code bytes match its supplied archive.
Bytecode-proved local-slot reads retain 151 actual ArrayLists and 605 observations
per side through copy, clear, iteration and return. Synthetic debugger-local
slot descriptors read the actual values without claiming an original locals
table. List, backing-array and key aliases remain stable through nested drains.
The debugger makes no target method calls, field writes, class redefinitions or
collection pins. Target and observer exit naturally under bounded ownership.

A separate Linux Debug GDB capture matches all 2,087 ordered engine calls per
side, including nesting, typed arguments, semantic returns, source flags and raw
Set structure across 4,204 paired states. Global correspondence retains 76 node
and 76 key identities and two table allocation epochs per side. Native addresses
can be reused after retirement; actual node/key serials distinguish those
lifetimes. Plain native nodes have an auxiliary backward bucket link absent
from JDK plain nodes. That native invariant is checked against bucket order and
then excluded from cross-runtime fields; tree backward links remain exact.
Native Boolean/status returns are checked separately from source Boolean,
integer or void results.

The GDB capture uses a separate nonsanitized Debug build; earlier GCC/Clang
sanitizer validation remains separate. Kernel PIDFD exit observations and child
adoption account for both debugger and inferior, with no intervention on the
successful captures. Earlier attempts failed in debugger function-address
conversion and retain failed receipts. Native snapshot correspondence, complete
owner adaptation, constructor-clock pairing, default identity generation and
wider persistence/failure profiles remain open. These observations retain
development-source attribution and incomplete qualification status.

Build and invoke the native producer with a fresh output file:

```sh
cmake --build build --target mc_wire_world_probe
build/mc_wire_world_probe client PREPARED_ROOT_HEX tests/reference/world/wire/line.plan output.jsonl
```

`PREPARED_ROOT_HEX` encodes the UTF-8 path whose `world/level.dat` already exists.
Use a separate copied root for each execution. `server` selects the base-server
World. This observer does not launch a client or server application.

## Source map

The supplied archive pins are recorded with the [position collection](../reference/wire-position-set.md).
The selected class entries are:

| Side and class | Bytes | SHA-256 |
| --- | --- | --- |
| Client `sm.class` | 5,273 | `8aab533178604f8388735bbb00b74bf2afaf2eeb49c98edd6822293e2dc0e903` |
| Server `lo.class` | 4,739 | `c504e329ffe08f731cf3299831d06f1a44ef65b7abb58fa5060857758ec68461` |

 Retained disassembly spans cover the
complete methods below; bytecode PCs are method-local. The runtime bodies use
the defining Block's side and real World/Registry services.

| Client / server body | Descriptor (client / server World type) | Selected calls |
| --- | --- | --- |
| `sm.a` / `lo.a` support | `(Lfd;III)Z` / `(Ldj;III)Z` | Below support7 |
| `sm.h` / `lo.g` drain | `(Lfd;III)V` / `(Ldj;III)V` | Update10, shallow copy21, clear30, neighbors84 |
| `sm.a` / `lo.a` strength recursion | `(Lfd;IIIIII)V` / `(Ldj;IIIIII)V` | Power23, metadata285, observer297, recurse433/492, seven Set calls526..664 |
| `sm.i` / `lo.h` surrounding notification | `(Lfd;III)V` / `(Ldj;III)V` | ID5, seven neighbor operations25..109 |
| `sm.c` / `lo.c` addition | `(Lfd;III)V` / `(Ldj;III)V` | Base6, remote10, drain23, above37/below51 |
| `sm.b` / `lo.b` removal | `(Lfd;III)V` / `(Ldj;III)V` | Base6, remote10, above28/below42, drain51 |
| `sm.f` / `lo.f` maximum | `(Lfd;IIII)I` / `(Ldj;IIII)I` | ID5, receiver ID9, metadata23 |
| `sm.b` / `lo.b` neighbor | `(Lfd;IIII)V` / `(Ldj;IIII)V` | Remote1, metadata13, support24, drop42, notifying air51 or drain64, base75 |

The source map and native contracts establish implementation coverage. They do
not substitute for executed-original World-level comparison. The separate
client display/particle body remains outside this synchronous simulation scope.
