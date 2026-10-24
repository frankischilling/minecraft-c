# World power queries

The [power interface](../../include/mc/world/runtime/power/power.h) exposes
`mc_world_runtime_is_powered`. It returns native status separately from the
ordinary power boolean. The [World bodies](../../src/world/runtime/power/world.c)
and [Block outputs](../../src/world/runtime/power/blocks.c) follow the paired
[source map](../reference/power.md). This implements output queries over the
actual World; it does not advance redstone state.

## World traversal

The outer query asks below, above, Z-minus, Z-plus, X-minus and X-plus, using
side arguments zero through five. It returns at the first true result. Every
coordinate increment or decrement wraps as a Java integer; no future neighbor
is validated before its call is reached.

A directional weak query first runs actual solid support. A supporting block
asks its own six strong neighbors. Otherwise the query acquires the current
block ID, returns false for air, captures the current Block.m array, indexes
that captured array with the full signed ID and invokes the selected weak
output. A directional strong query similarly acquires its ID before capturing
Block.m and invoking the selected strong output. Neither output is inferred
from opacity, an ID table or the capability predicate.

The support subcall has a different order: capture Block.m, acquire the ID,
index the earlier array, then inspect the selected Material and concrete
support predicate. It can therefore observe a different array identity from
the later power lookup after synchronous acquisition. A null selected Block
completes false in support, but fails at the actual output invocation in a
direct power query. A null array fails when indexed. Bounds and blank behavior
come from each reached real getter; there is no additional global coordinate
or side-range guard.

## Concrete outputs

The complete admitted inheritance resolves seven output owners and the
constant-false base inherited by the other sixty-two native kinds. The
capability predicate remains a distinct operation.

| Owner | Weak output | Strong output | Capability |
| --- | --- | --- | --- |
| Base uu/na and remaining inherited kinds | False without World use | False without World use | False |
| Pressure bv/bc | Metadata greater than zero | Metadata nonzero and side1 | True |
| Detector ph/jn | Metadata bit8 | Metadata bit8 and side1 | True |
| Button oi/iz, native BUTTON | Metadata bit8 | Powered orientation selects one side | True |
| Lever xr/pd, native LEVER | Metadata bit8 | Powered orientation selects one side, including orientation6 | True |
| Redstone torch db/by, native REDSTONE_TORCH | Live receiver flag, then raw metadata attachment exclusions | Side0 delegates to the same receiver's weak output; other sides return false first | True |
| Repeater wo/oi | Live receiver flag, then masked orientation selects one horizontal side | Delegates to weak output | False |
| Redstone sm/lo | Live receiver flag, metadata and complete local connectivity geometry | Live receiver flag, then delegates to weak output | Live receiver flag |

Metadata reads remain where each body places them. Pressure, detector, button
and lever read metadata before their later side tests. An inactive torch,
repeater or redstone can return before using its nullable World argument. Torch
strong output also returns before World use when side is not zero. Instance
invocation adds no constructor, READY or class-initialization gate.

Redstone first checks its live flag and metadata. Zero metadata returns false;
nonzero metadata returns true immediately for side1. Other sides evaluate
X-minus, X-plus, Z-minus and Z-plus connectivity, eligible connections one
block below, then the repeated support and one-above connections when the
block above the redstone does not support. Already-connected directions do not
suppress later source support checks. The final horizontal result depends on
the four actual connection booleans; an isolated powered redstone segment outputs to each
horizontal side, while a connected line requires the selected direction and
absence of perpendicular connections.

The connectivity helper reads the actual neighboring ID before live redstone and
repeater statics. It distinguishes redstone identity, air, the selected Block's
capability and the two repeater identities. A repeater connection reads its
metadata and the real jj/ge horizontal-direction class. That class has its own
initializer, arrays and failure state, implemented in
[horizontal directions](../../src/block/registry/horizontal_directions.c).
Its b array is captured before the selected element is read. The complete
initializer also constructs a and c in source order; a constant direction map
would omit those allocations, publications and possible failures. The
six-direction wj/od class is a separate owner.

## Ownership and diagnostics

Public queries admit live exclusive GAME Worlds, including raw, constructing
and failed prefixes, and reject retirement. The public call enters and leaves
the actual World around the whole operation. Internal held entries require
the caller's existing World entry. The World's class binding keeps its actual
Registry and source tables live across acquisition and initialization.

Boolean results own no List, pooled bounds, Entity, persistent World binding
or local lease. Each actual reader leases its selected Chunk only for its
call. Power arithmetic itself needs no floating-environment admission and
draws no Random value. Reached initialization, loading or population can still
consume entropy and change their real owners.

Failure preserves the ordinary output and all earlier effects. The error
retains four separate World sites, concrete capability/strong/weak sites and
the redstone-connectivity site. Each site records whether its body was entered;
this distinguishes a valid PC0 from a null invocation whose body never began.
World sites retain their own coordinates and directional argument. Concrete
Block names identify the actual declaring owner, including inherited base
bodies, rather than a friendly block label.

Fresh children retain the entire nearby-reader error: provider and Chunk PCs,
acquisition, Registry, population, TileEntity, batch, Entity load and activation
details. Detailed support retains its own frame plus that same full reader
child. Caller sites survive nested failures, while a later sibling clears the
callee subtree so it cannot inherit an earlier successful branch. These fixed
sites describe the reached nesting in this operation, not an arbitrary source
stack or a recursion limit. Successful calls return a neutral error and do not
clear older World diagnostics.

The existing public solid-support API keeps its original outer World entry
and nested public block-getter entry. The
[detailed held form](../../src/world/runtime/block_queries_internal.h) shares
the same support body, requires its caller's entry and uses the existing held
reader once. It preserves source read order and effects while deliberately
using different native entry bookkeeping. It does not claim identical
artificial counter-overflow admission to the public form.

## Validation and remaining scope

The [power contracts](../../tests/world/runtime/power/test_power.c) pass 7,593
checks per host using two real GAME Worlds and two loaded records. They cover
all 69 admitted kinds, metadata and side matrices, ordered short circuits,
conduction, redstone geometry, defining namespaces and detailed failure prefixes.
The separate horizontal-direction case passes 359 checks, including all seven
allocation failure cuts. Existing [solid-support contracts](../../tests/world/runtime/test_solid_support.c)
pass 4,359 checks for the preserved public interface.

At native checkpoint `9f9d3a22900ac25c8fc2813ce2f5881fcedb63a2`, full suites
pass 173 MSVC and 175 tests on each sanitized GCC/Clang build, with zero skips.
Controller mirrors, lifecycle checks and formatting for all 499 C files and
headers also pass. Linux uses an exact 961-file native source copy.
[Testing](../testing.md) records the configuration and retained evidence scope.

A separate executed comparison at that checkpoint covers all 5,365 literal
rows on each original side against the native MSVC observer. Each side has
75,110 equal before/after snapshot-field comparisons, 5,365 equal completion
classifications and 5,164 equal normally returned booleans. The 201 target
failures per side remain recorded; their booleans are excluded and their
Throwable/native errors are not equated. Every engine/side completed all rows
and 235 real controls/restorations without infrastructure failures. The
[evidence record](../reference/power.md#validation-boundary) gives the seven
compared fields, exact hashes, preparation recipe and exclusions.

This finite result excludes setup/graph identity, absolute dirty/populated
history and source-PC equality. Redstone propagation, redstone lifecycle and ticks,
torch lifecycle and burnout, button and lever interactions, repeater timing,
scheduled draining and a complete game loop remain separate work. Rail
implementation has its own [native contract](rails.md); an original-runtime
rail comparison remains unverified. Original JARs and Java tooling are optional
reference-test dependencies; the C runtime remains JVM-free.
