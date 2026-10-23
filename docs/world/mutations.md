# Synchronous block mutation

The GAME World entry points in `mc/world/runtime/mutations.h` acquire the actual
Chunk before calling its ID or ID-plus-metadata setter. Quiet ID setters still
run the selected removal/addition hooks, incremental height repair and lighting
submissions. The notifying variants then visit observers and six neighbors when
the quiet call returns true. The notifying metadata operation has a void game
result; native status reports failures separately.

Reached concrete handlers without an implemented body fail explicitly after
earlier effects. [Flowing and still fluids](fluids.md) now have concrete update,
addition and neighbor bodies. [Wooden pressure plates](pressure-plates.md) have
their actual added, support-neighbor, removal and shared contact/tick bodies.
[Plant callbacks](plants.md) implement survival, drops, growth and farmland
through the same World services. Population, complete fluid/falling closure,
tile/entity effects and general observer dispatch remain unfinished.

## Source order

Coordinates use the same asymmetric bounds as [block access](blocks.md). Outside
requests return false before acquisition. Blank Chunk ID overrides return true
without changing their arrays. An equal normal ID write returns false; the
ID-plus-metadata overload also compares the full metadata argument.

Normal setters read unsigned old height and block bytes before comparison. They
write the new low eight bits before removing the old Block. ID-only removal has
no remote gate; its addition does. ID-plus-metadata removal is remote gated;
its addition is not. That second overload writes metadata twice and gates its
height/Sky sequence on the current Dimension. These differences are preserved.
Opacity indexes use the requested ID's low eight bits; addition uses its full
integer. A late invalid index retains earlier bytes, metadata and height changes.

Incremental height repair is distinct from [full finishing](finishing.md).
Height can become 128. A height-change observer call receives local x/z before
the height/minimum writes. Minimum rescanning starts at127. Opacity captures
precede the current block reads, with Java integer wrapping and no upper light
clamp before nibble masking. Retained height bytes above 128 retain the original
additive scan and OR-indexed getter/Nibble behavior; reached array failures are
reported without an early numerical-coordinate rejection.

Both setters reuse the same four ordered height-neighbor operations as full
finishing. World [height and light submission](runtime-lighting.md) retain
nonloading presence, actual channel identities, queue effects and no-sky gates.
Mutation does not implicitly drain the queue.

## Notifications and lifetimes

Observers precede neighbors. Each neighbor independently reads current World
suppression flags, captures Block.m, acquires the current block, indexes that
captured array, and invokes the selected concrete handler. Order is -X, +X, -Y,
+Y, -Z, +Z; the forwarded source ID remains the original full integer.

Notifying metadata first performs the quiet write, then acquires the current ID
again and reads actual Block.t at its low-eight-bit index. That flag chooses
observer-plus-neighbors or neighbors alone. Equal nibble writes still reach it.
The current finite observer owner supports genuine empty lists. Nonempty
observer invocation remains an explicit reached dependency.

Native calls hold the initiating World and lease the exact selected Chunk
through synchronous handlers. The Chunk's actual World is held separately.
Failure preserves the ordinary output and completed effects, including effects
on another owner. Error frames retain the reached World, Chunk, height,
observer, neighbor and hook calls and complete returned Registry/light/storage
causes. Native admission and unsupported descendants are not Java Throwable or
allocation-failure equivalence claims.

## Reference basis

The paired supplied binaries are the unchanged client SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`
and server `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.

| Operation | Client | Server |
| --- | --- | --- |
| Quiet ID | `fd.c(IIII)Z` | `dj.b(IIII)Z` |
| Quiet ID and metadata | `fd.a(IIIII)Z` | `dj.a(IIIII)Z` |
| Notifying ID | `fd.f(IIII)Z` | `dj.e(IIII)Z` |
| Notifying ID and metadata | `fd.b(IIIII)Z` | `dj.b(IIIII)Z` |
| Notifying metadata | `fd.d(IIII)V` | `dj.c(IIII)V` |
| Concrete Chunk setters | `lm.a(IIII)Z`, `lm.a(IIIII)Z` | `hi.a(IIII)Z`, `hi.a(IIIII)Z` |
| Incremental height | `lm.g(III)V` | `hi.g(III)V` |

These source mappings establish ordering; they do not qualify whole population,
published-cache mutation parity, arbitrary callbacks or complete persistence.

## Native validation

World setters and plant notifying writes retain independent checked diagnostic
workspaces across synchronous descendants. Native allocation failure preserves
earlier effects, including drops completed before a plant's write. The
[full-height regression](../reference/plant-lifecycle.md#native-contracts)
exercises recursive removal on the default Windows stack.

At the earlier mutation checkpoint, all 52 selected cases passed on Windows
MSVC and WSL GCC/Clang, including 218 new mutation checks. Linux enables ASan/UBSan and leak detection. All 618 selected
code/tool/fixture inputs match before/after and retained host copies; each host
retains 44 build products. Formatting passes 335 C/header files.

The native tests use real constructed Worlds and store-owned Chunks, with
explicit prepared array inputs. They cover both overloads, full-int equality
and late registry failures, height 128/minimum 127, retained-height aliasing and
failure, restored negative opacity, cold resource failure with actual leases,
cached public notifications, a natural capacity limit after an earlier error,
and real maintenance/save/unload/reload. Synthetic nonowning m/t null views are
separate guards. Unload retains dirty; the later loaded record has its decoder
state. Two corrected test assumptions and a strict GCC conversion diagnostic
are retained with their first failed runs.

These are finite native contracts and existing fixture replays. They do not
establish new original/native mutation parity or the full synchronous closure.

## Original direct-Chunk experiments

`tests/reference/world/MutationObservation.java` runs actual setters on real
unpublished normal Chunks in genuine original Worlds. Four fresh Temurin17
captures cover both binaries, baseline 23 calls and two negative-opacity calls
per side. All five target arrays and scalars are retained before/after each
operation, with actual Boolean or exact Throwable class. Both originals agree
on these local projections. The helper never computes expected mutation data.

The measured neighboring entries are genuinely absent and observers empty.
The helper preserves the existing setup cache and lighting queue, checks exact
cached arrays and identity graphs, and reads actual Random state without draws.
The controlled actual q element is restored in `finally`. Controls compare
each World before and after calls; setup clocks, RNG and queue state vary between
processes. This direct feature-API experiment has no native replay fixture yet
and does not qualify ordinary cached population, arbitrary observers, dedicated
routes, instrumentation equivalence or original save interoperability.

Compile the helper with JDK17 and run its CLI with a pinned local JAR, side
`client` or `server`, a new owned world directory, and scenario `baseline` or
`negative-q`. The launcher requires
`--add-opens java.base/java.util=ALL-UNNAMED` for read-only Random inspection.
Stdout is the bounded binary transcript; original console and actual Throwable
traces go to stderr. Exit 77 means setup was unavailable, never a passing result.
