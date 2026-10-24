# World construction and environment

`mc_simulation` retains one stable `mc_world_runtime` identity from native RAW
allocation through construction, failure and retirement. The existing
`mc_world_runtime_create` composition remains a separate entry for explicitly
provided storage and callbacks. `mc_world_runtime_allocate_world` and
`mc_world_runtime_construct_world` model the ordered base and dedicated-World
constructors.

The GAME owner has all 43 case-sensitive base fields. Ten list owners are
distinct, E and F use the real scheduled collections, and P has its own empty
identity. Collection construction does not implement every later list/set
operation. The dedicated entry also owns its five separate subclass fields and
constructs the dedicated provider during the base virtual call. Network-client
subclass fields and its provider override remain unfinished.

## Ordered state and ownership

The common prefix creates the temporary default Random and draws g, reads the
actual platform millisecond clock into I, constructs the retained Random r,
then draws Q through that same r. Both default constructors use the application
profile. Ordinary operation uses the real clock; no chosen g/Q value is supplied.
A private, bounded observation harness can supply independently measured clock
inputs at that same sampling boundary, after the actual uniquifier advance.
Its [input-binding contract](../reference/random-profile.md#measured-constructor-inputs)
does not replace Random output or shared state.
The base class owner shares its actual static lighting-depth cell across
instances and borrows the application's profile and initialized trig table.
An optional [shared registry binding](registry.md) is installed before the
first World allocation and retained for the entire class lifetime. Binding
performs no game initialization.

Load/create constructs SavedData before typed Info loading. Direct metadata
and copy use their original different schedules. Copy first executes the full
fresh prefix, then overwrites I from the parent, shares its store, copies Info
with independent scalar cells and retains the original name/Player identities.
The runtime's time/spawn contexts point into its actual held Info.

Typed loading constructs Info from the selected NBT Data compound before
forming the older convenience view. Missing Data uses defaults. Present Data
of the wrong type fails and may select the backup. Native allocation/limit
failures remain fatal. Loaded Info owns its data independently of the store's
retained document.

WorldProvider binding publishes the same World before constructing the current
biome manager and filling the WorldProvider's actual light array. Noise-backed biome
sampling reads the live World seed before each of its three channels. Rebinding
publishes a new manager; an independently held preceding manager stays alive.
With a class Registry binding, construction uses the actual
[noise-backed/fixed WorldChunkManagers](biome-registry.md#worldprovider-and-worldchunkmanager).
Each owns a Registry borrow independent of the World class. Fixed setup loads
actual Hell/Sky references before replacing the manager, and any failure keeps
the full Registry cause at the enclosing World bind call300,315 or398.

The concrete native Overworld ChunkProvider owns its generator, pending history,
cache/list storage and blank payload. Its actual cave Random is default
constructed before explicit terrain/noise initialization and survives that
initialization. Cave reseeding preserves stored Gaussian bits while clearing
their validity. The public numerical generator remains entropy-free and keeps
its distinct fresh-seed behavior. Provider-held biome ownership survives a
later WorldProvider rebind. Cache and reference hooks use the runtime's same
embedded blank identity.

Direct/copy and already-loaded Overworld base-World paths can reach native READY
after provider construction, initial light and weather. Missing metadata
enters the real acquiring spawn predicate and its source-ordered rejection
loop. Missing chunks enter [concrete generation and known-empty activation](chunk-activation.md),
retaining reached fields, y, provider and generation effects on failure.
Registry resources are required; reached population gates enter the
[actual Overworld controller](population/overworld.md). Unsupported descendants retain
their explicit failure prefixes. Nether/Sky full providers, dedicated/network-client
startup and complete gameplay startup qualification remain
unfinished. Native READY is constructor completion for the represented normal
path, not a whole-game or original-World observation claim.

## Dedicated construction

`MC_WORLD_RUNTIME_SERVER_DEDICATED` evaluates the dimension factory before the
base instance prefix. It uses `dimension_id`; the separate base-constructor
`dimension` argument does not select this route. During the base virtual provider
call, it constructs the dedicated cache and publishes subclass C before base v
receives that same owner. After the base returns, it stores D=false, constructs
the typed Entity index G, then retains and stores the nullable Server F. E keeps
its default false value. Subclass E is the live saving-disable cell; base E is
the scheduled ordered collection.

The [Server default constructor](../server/construction.md) is separate from
startup. A dedicated World can retain a real default-constructed Server without
opening a listener, loading properties or starting a tick loop. The cache policy
is selected by this constructor and stays dedicated for its lifetime.

The Entity-added hook first runs the base WorldAccess loop, reads the Entity's current
ID, then enters G. Batch publication adds the complete source batch to base b
before invoking per-Entity hooks. The index preserves the original duplicate-key
behavior: it updates every matching node and still prepends another node.
Deferred Chunk detachment appends to base D without removing an ID from G.
Later source removal/tick processing remains unfinished. See the
[bytecode map](../reference/dedicated-world-construction.md) for the exact scope.

## Light, weather and block reads

The World angle delegate captures its actual WorldProvider, reads current Info
time, then dispatches the WorldProvider angle. Overworld celestial smoothing uses
binary64 `cos`; subsequent World light calculation uses the distinct Minecraft
float lookup. Rain and thunder retain their separate float operations, fresh
rain read inside thunder and both double attenuation stages with intervening
float narrowing. Initial light conditionally assigns World f before the
private weather helper reads Info flags. False raining skips the thunder read
and preserves existing weather strengths.

Block reads use the installed acquisition coordinator, signed chunk shifts
and actual X/Z/Y payload indexing. Out-of-range coordinates return air before
acquisition. The positive x boundary is exclusive; positive z is inclusive.
Top-block lookup starts at y63, scans upward while the block above is nonair,
then reads the resulting y. It uses no cached height or downward search.
Unsupported generation/population descendants fail explicitly after any reached
load/acquisition effects. Known-empty entity/TileEntity records retain the existing
storage activation boundary.

The separate [block access contract](blocks.md) covers the implemented quiet
metadata getter/setter and their selected native validation. [Block-ID mutation
and notifying wrappers](mutations.md) preserve their separate schedules, with
concrete synchronous descendants still unfinished.

## Native limits and retirement

Default World direct storage is bounded to 16 MiB and 65,536 scheduled entries.
Provider storage has a separate 8 MiB direct bound and 512 cache/list/pending
slots. Its Integer HashSet history retains the existing separate 8 MiB retained
and 16 MiB peak bounds, with its entry limit clipped to the provider bound.
WorldProvider direct storage defaults to 4 MiB, with separately bounded
noise-backed WorldChunkManager buffers. Info, SavedData and store payloads retain their own limits.
The dedicated Entity index independently defaults to 65,536 nodes, 8 MiB retained
storage and a 16 MiB peak request bound. These native allocation policies do not
reproduce Java object/array OOM timing.

A reached construction failure latches FAILED_CONSTRUCTION, preserving actual
World fields and shared profile effects. The same World cannot retry. An
unreturned provider child can be explicitly freed without undoing those effects.
Inspection borrows live references; the exposed Random pointer has the World
lifetime and does not add arbitrary public Random replacement/alias ownership.

Native retirement requires no entered call/local lease and empty unimplemented
collections/cache/list/pending state. Typed [lighting C records](runtime-lighting.md)
and [scheduled E/F records](scheduling.md) remain owned and inspectable until final
native free, including an F-only partial insertion prefix. Retirement does not
propagate, repair queues or implicitly clear these records. It unregisters actual contexts and clears
only this WorldProvider's binding to this World, without invoking biome setup.
It never clears the aggregate persistent-binding count. External Entity/TileEntity
bindings keep free BUSY until their owners release them. No implicit save,
file rollback, profile reset or retry occurs. Terminal retirement releases the
dedicated Entity-index holds and sets G to NULL without running a source
clear/removal hook. The World keeps its Server hold until final free.

Store-owned [generated Chunks](chunk-objects.md) also keep their actual World d
allocated. The free guard checks those exact references before teardown. World
retirement can precede Chunk retirement, while final free remains BUSY until all
such owners have retired. Keep the store live through both lifetimes.

The shared-registry addition passes 150 constructed checks within 22 selected
cases on MSVC/GCC/Clang, including the unchanged 4,397-check GAME case and
recorded Dimension replay. Linux runs use ASan/UBSan and leak detection. These
checks do not qualify original World construction or the complete fresh-spawn
loop; see the [registry contract](registry.md) for exact scope.

See [Dimension reference evidence](../reference/dimension.md),
[Info ownership](info.md), [SavedData](saved-data.md) and
[runtime storage](storage.md) for their separate qualification boundaries.
