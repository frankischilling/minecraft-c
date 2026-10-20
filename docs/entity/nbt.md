# Entity classes and Item NBT

The [NBT methods](../../include/mc/entity/nbt/nbt.h) operate on actual constructed
ItemEntities. The [save wrapper and NBT factory](../../include/mc/entity/nbt/factory.h)
add the original class-name and world-only construction calls. These APIs do
not yet replace the unfinished Entity loops in ordinary GAME Chunk loading and
saving. Other concrete Entity constructors, AI, ticking, source removal and
complete save interoperability remain unfinished.

## Class identity and lifetime

`mc_entity_classes` owns 24 stable canonical Class cells. Retrieving a literal
is passive: it does not initialize Entity, consume an ID or draw Random.
Distinct application owners have distinct cells even when their names and
source sides agree. The diagnostic obfuscated names never determine identity.

The [EntityList namespace](../../include/mc/entity/registry/registry.h) initializes
four distinct native Maps in source order: name to Class, Class to name, numeric
ID to Class, and Class to numeric ID. Its 24 registrations use the original
names and numbers. Each completed Map publication and put survives a later
native allocation failure. A failed initializer retains its first complete
cause and refuses later active use without retry. The native linked Maps and
UTF-16 value keys do not reproduce Java HashMap, String boxing or heap layout.

Registry keeps its existing 13 stable Biome logical handles. Explicit binding
joins those handles to the same canonical Entity Class cells through a fixed
typed association. A foreign handle is rejected; an unbound canonical lookup
reports the missing dependency. Both Registry-first and Entity-first binding
orders preserve previously borrowed handles.

WorldClasses retains its own Registry and Entity bindings. Binding the second
dependency also establishes Registry's one independent Entity-owner hold.
An identical Registry/Entity pair is idempotent; a conflicting pair fails
without publishing the new WorldClasses binding. Destroy WorldClasses before
Registry, then destroy Entity classes after all instances and other bindings
have released them. The lower `mc_entity_classes` library owns passive metadata;
concrete constructors and World/NBT operations remain in `mc_simulation`.

## Read and write order

The independent leaf writer emits ten known fields and 22 value tags:

| Fields | Representation |
| --- | --- |
| Pos, Motion | Three Double values each |
| Rotation | Two Float values |
| FallDistance | Float |
| Fire, Air, Health, Age | Short |
| OnGround | Byte |
| Item | Compound containing Short id, Byte Count and Short Damage |

Saved Y is current Y plus the widened `y_size` Float. Health narrows from int
through byte to short. Stack fields come from the actual held owner, including
RAW or failed Stack construction; there is no invented READY or ID-validity
check. A NULL Stack fails only at the reached Item writer call, after the
earlier base and Item values. No unknown Entity or Stack tags are overlaid by
these semantic methods.

Read fetches all three named Lists before storing any motion. All three motion
stores precede their clamps: ordered magnitudes greater than ten become positive
zero; NaN, exactly positive/negative ten and negative zero survive. Each position
axis updates current, interpolation and previous fields in source order. Previous
angles retain the input while current angles later use float remainder by 360.
The same existing bounds object is updated before the Item read hook. A later
wrong tag leaves every completed earlier store intact.

Item read applies unsigned Health masking, then signed Age, then constructs a
new Stack. The old Stack remains owned until successful reference replacement.
Missing Item constructs a genuine zero-valued Stack. None of these operations
resets IDs, class phases, Random, World bindings or constructor state.

The Boolean save wrapper resolves the Class name before testing dead. A false
result leaves the caller's tree untouched. A true result adds String `id=Item`,
giving eleven known fields and 23 tags. The NBT factory uses the supplied
application Class owner even with NULL World. It calls the real world-only
constructor, which consumes one shared Math draw, then invokes semantic read
outside the original reflection catch. A later failure preserves consumed IDs
and profile effects. Each successful factory call returns a distinct Entity
and Stack; it does not publish either into a World or Chunk.

Unknown names return NULL after the supported default process stdout diagnostic.
That path currently supports at most 64 ASCII bytes including its fixed prefix.
Known unsupported subclasses stop at their reached constructor frontier.
Wrong-type getters stop at the catch diagnostic frontier. Java reflection,
Throwable stack printing, arbitrary Unicode output, stream replacement and
PrintStream error-state queries remain outside this finite implementation.

## Native contracts and verification

Independent trees own their bytes. The Boolean wrapper charges its intermediate
tree while constructing the final tree; it excludes separately owned live
Entity/Stack graphs and bounded C-stack views. Output slots remain unchanged on
failure. Error values retain independent save/name/factory/base/Item/Stack PCs,
the whole namespace first failure, constructor cause and codec cause. These
resource and output contracts do not claim Java allocation/OOME timing.

The native contracts cover both sides and real immutable allocation ceilings:
`entity.nbt` passes 1,084 checks, `entity.classes` passes 7,759,
`entity.factory` passes 364 and `world.entity_batch` passes 353 on MSVC and
sanitized Linux GCC/Clang. All 149 configured MSVC cases and 151 cases on each
Linux compiler pass, with zero skips and local JDK comparisons enabled. They check
signed narrowing, ordered partial reads, numeric edge bits, Stack replacement,
all 24 namespace mappings, cached partial initialization, Class binding and
whole-batch ownership. These are constructed native contracts.

Separately, twenty prepared cases per side match actual calls into both pinned
original archives on all three native hosts. The comparison checks 52 selected
fields, exact numeric bits, partial failure state, owner relationships and
semantic saved tags. `entity.nbt_observation` replays the complete captured
original stdout through the native probe without requiring either archive or
a JDK. These finite checks do not establish ticking, complete World startup or
ordinary disk save/reload. See [paired NBT source anchors and capture
procedure](../reference/entity-nbt.md),
[construction evidence](../reference/entity-construction.md) and
[World publication](../world/entities.md).
