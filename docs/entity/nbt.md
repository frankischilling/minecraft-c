# Entity classes and subtype NBT

The [NBT methods](../../include/mc/entity/nbt/nbt.h) operate on actual Item,
FallingSand and Minecart owners. The
[save wrapper and NBT factory](../../include/mc/entity/nbt/factory.h) add the
original class-name and World-only construction calls. GAME Chunk loading and
saving dispatch through the actual concrete subtype. Minecart has a separate
[inventory, NBT and storage contract](minecart-inventory.md), with a
[local native validation record](../reference/minecart-inventory-nbt.md#native-validation).
Other concrete Entity behavior, ticking, source
removal and complete save interoperability remain unfinished.

## Class identity and lifetime

`mc_entity_classes` owns 26 stable canonical Class cells: the 24 registered
classes plus query-only Entity and Player. Retrieving a literal
is passive: it does not initialize Entity, consume an ID or draw Random.
Distinct application owners have distinct cells even when their names and
source sides agree. The diagnostic obfuscated names never determine identity.
Passive [assignability](../world/class-entities.md) follows the audited
superclass graph within the same owner without changing class state.

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

The Item leaf writer emits ten known fields and 22 value tags:

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

For Item, the Boolean save wrapper resolves the Class name before testing dead. A false
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
A wrong-type factory id getter stops at the catch diagnostic frontier. Java reflection,
Throwable stack printing, arbitrary Unicode output, stream replacement and
PrintStream error-state queries remain outside this finite implementation.

## FallingSand

The [falling subsystem](../world/falling.md) uses the same base read/write
order followed by the genuine FallingSand hook. Its leaf has eight fields and
17 tags: the seven common fields plus Byte `Tile`. The named wrapper has nine
fields and 18 tags, including `id=FallingSand`. Saving narrows the carried int
to Byte; reading masks that Byte with 255. Age is not persisted. No Health,
Age, Item Compound or Stack is required by this subtype.

The factory invokes the genuine world-only FallingSand constructor before
semantic read. That constructor consumes the base private Random allocation
and ID, with no Item Math draw. Its independent provenance tree preserves
unknown Entity tags using the same native overlay policy. Replacement and
save budgets include all overlapping live trees, generated trees and pointer
scratch. Item provenance still belongs to the actual current Stack.

## Minecart

Client `yl` and server `pr` use the same base read before their own Type getter.
Type2 reads PushX, PushZ and Short Fuel in order. Type1 captures Items before
allocating and publishing a new 27-entry array. Missing Items still replaces h;
a wrong List tag fails before replacement. Byte Slot is masked with 255; in-range
records construct and install real Stacks in list order, so later duplicates
replace earlier entries. A later failure retains the new array and earlier
stores. Detached old h and its occurrence holds protect borrowed inputs until
all reads finish on either outcome.

The writer emits base fields plus Type and, for Type1, Items from every actual
h slot; Type2 emits PushX/PushZ/Fuel instead. Its 8/9/11 known-field results become
9/10/12 when the Boolean wrapper adds String `id=Minecart`. Actual Stack fields
are accepted in every live phase without a READY-only provenance builder. The
retained own-field tree stays charged while the final independent result is
copied. Name lookup still precedes the dead decision; normal false changes only
`serialized`.

The named factory calls the real World-only Minecart constructor, then semantic
read outside the constructor reflection catch. It returns the actual owner
without World/Chunk publication. The [detailed contract](minecart-inventory.md)
records actual 36 versus reported 27 slots, Stack hold transfers, signed split
prefixes and native storage occurrence policy. It does not widen Minecart
movement, collision, Player interaction or ticks.

Local B passed all 544 CTest executions across MSVC/GCC/Clang with zero failures,
errors or skips. The [validation record](../reference/minecart-inventory-nbt.md#native-validation)
separates those full runs from the later one-test GCC CMake D portability probe.
Original Minecart inventory/NBT/storage equality has not been established.

## Native contracts and verification

Independent trees own their bytes. The Boolean wrapper charges its intermediate
tree while constructing the final tree; it excludes separately owned live
Entity/Stack graphs and bounded C-stack views. Output slots remain unchanged on
failure. Error values retain independent save/name/factory/base and subtype
sites, the selected complete Stack cause, namespace first failure, constructor
cause and codec cause. Minecart carries explicit read/write and compact inventory sites;
its Items-list index remains separate from the decoded slot. These resource
and output contracts do not claim Java allocation/OOME timing.

The following counts record the earlier Item/FallingSand increment, not the
new Minecart inventory/NBT/storage runs.

The native contracts cover both sides and real immutable allocation ceilings:
`entity.nbt` passes 1,084 checks, `entity.classes` passes 7,759,
`entity.factory` passes 364 and `world.entity_batch` passes 353 on MSVC and
sanitized Linux GCC/Clang. All 151 configured MSVC cases and 153 cases on each
Linux compiler pass, with zero skips and local JDK comparisons enabled. They check
signed narrowing, ordered partial reads, numeric edge bits, Stack replacement,
all 24 namespace mappings, cached partial initialization, Class binding and
whole-batch ownership. These are constructed native contracts.

The original observations below retain that same earlier scope.

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

## Storage provenance and ordinary GAME coordination

[`nbt/storage.h`](../../include/mc/entity/nbt/storage.h) adds native storage
operations beside the semantic reader/writer. Preservation requires a successful
semantic read of that same input; it does not perform the read. After direct
Chunk adoption, ItemEntity receives a complete independent Entity source and its
current successfully read Stack receives a separate Item source copy.
FallingSand owns only its Entity source. Minecart uses the per-occurrence
generation policy described below. For ItemEntity, replacement commits both
attachments only after every fallible copy succeeds. The final Entity hold frees
its own source. The direct Class allocation ledger keeps its prior meaning; a checked storage inspector adds actual source/Stack
allocations without multiplying them by hold count.

Storage saving calls the actual Boolean wrapper first. A normal false allocates
no overlay even with a zero residual ceiling. For ItemEntity, a true result
overlays generated fields over its Entity source and the generated Item over
only its current Stack's source. Replacing that Stack cannot resurrect the prior
Item's unknown fields. This uses a phase-neutral borrowed Compound overlay, so RAW/failed Stack
fields accepted by the semantic writer remain accepted. The final tree is an
independent copy; source owners and pointer scratch remain live until it finishes.
Preserved unknown fields/order/raw bits are an intermediate native policy, not
original fresh-writer behavior or encoded-byte/order equivalence.

Minecart preservation prepares a copied root generation. For Type1, it also
prepares each current slot's last accepted in-range record before committing
attachments. Other Types replace the root while keeping older h metadata.
Aliased Stacks can retain different metadata per occurrence. Set or whole removal invalidates only
that occurrence; partial split retains it. Root and h may keep different live
generations, and detached h protects any source borrowed by a semantic read.
Storage save emits only current generated records, overlays their retained
winner metadata and removes inactive Items/PushX/PushZ/Fuel. It never falls back
to Stack.source or resurrects skipped/overwritten archive records. The budget
counts distinct live Stacks/generations once, sidecars, overlapping semantic trees,
overlay scratch and final copy/validation. Pure byte inspection selects the
residual ceiling; its error is deferred until true assembly, so semantic name,
dead and normal false behavior retains priority.

The following GAME results also belong to the earlier Item/FallingSand record;
they are not validation of the new Minecart generation/occurrence policy.

The ordinary GAME loader now calls the actual factory and direct Chunk insertion;
the saver traverses every actual slice/List occurrence. Their disk/failure
contracts pass 468 native checks per host; the borrowed overlay passes 255.
Separate original whole-Chunk calls cover ten prepared cases per side, twice.
Known two-Item tag lists match all three native outputs; unknown-field retention
remains the native policy above. See [GAME persistence](../world/saving.md),
[staged loading](../world/storage.md#staged-game-loading) and
[paired source/observation boundaries](../reference/entity-game-storage.md).
