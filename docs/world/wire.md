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

Development suites pass all 200 MSVC and 202 Clang CTests, including the
447 native wire checks, with no failures or skips. Linux enables ASan, UBSan
and leak checks. All 581 C/header files pass clang-format 18; controller mirrors
and lifecycle interval controls pass on both hosts. Earlier failed attempts
retain their own records; fresh full suites pass after the diagnostic-storage
and fresh-configuration test-registration fixes. These development runs do not
qualify a clean commit or an executed-original World comparison.

The current cases fit one Chunk per World on both sides. They check unchanged
World Random where no drop occurs, expected IDs/metadata, source flags, empty
notification storage after successful drains and balanced calls/leases/snapshots.
These are native contracts. Original executed World-level wire equality,
intermediate notification traces, negative/cross-chunk acquisition, dedicated
World construction, shared-Registry multi-World reentry, wider persistence and
failure schedules remain required. The collection's default identity-source
boundary also remains open. Client display particles and other redstone Blocks
retain their own unfinished behavior.

Run the public native case without Java or original assets:

```sh
ctest --test-dir build --output-on-failure -R '^world[.]wire_lifecycle$'
```

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
