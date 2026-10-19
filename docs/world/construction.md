# World construction and environment

`mc_simulation` retains one stable `mc_world_runtime` identity from native RAW
allocation through construction, failure and retirement. The existing
`mc_world_runtime_create` composition remains a separate entry for explicitly
provided storage and callbacks. `mc_world_runtime_allocate_world` and
`mc_world_runtime_construct_world` model the ordered base-World constructors.

The GAME owner has all 43 case-sensitive base fields. Ten list owners are
distinct, E and F use the real scheduled collections, and P has its own empty
identity. Collection construction does not implement every later list/set
operation. Server and network-client subclass fields and their provider
overrides remain separate work.

## Ordered state and ownership

The common prefix creates the temporary default Random and draws g, reads the
actual platform millisecond clock into I, constructs the retained Random r,
then draws Q through that same r. Both default constructors use the application
profile; no constructor clock, entropy stream or chosen g/Q value is supplied.
The base class owner shares its actual static lighting-depth cell across
instances and borrows the application's profile and initialized trig table.

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

Dimension binding publishes the same World before constructing the current
biome manager and filling the Dimension's actual light array. Normal biome
noise reads the live World seed before each of its three channels. Rebinding
publishes a new manager; an independently held preceding manager stays alive.

The concrete native normal provider owns its generator, pending history,
cache/list storage and blank payload. Its actual cave Random is default
constructed before explicit terrain/noise initialization and survives that
initialization. Cave reseeding preserves stored Gaussian bits while clearing
their validity. The public numerical generator remains entropy-free and keeps
its distinct fresh-seed behavior. Provider-held biome ownership survives a
later Dimension rebind. Cache and reference hooks use the runtime's same
embedded blank identity.

Direct/copy and already-loaded normal base-World paths can reach native READY
after provider construction, initial light and weather. Missing metadata
enters the real acquiring first spawn predicate. Generation resources and the
live block registry are unfinished, so that path stops explicitly with its
reached fields, y flag and provider retained. It cannot return a fabricated
spawn or READY prefix. Nether/Sky full providers, dedicated/network-client
subclass startup, registry/class bootstrap and complete gameplay startup remain
unfinished. Native READY is constructor completion for the represented normal
path, not a whole-game or original-World observation claim.

## Light, weather and block reads

The World angle delegate captures its actual Dimension, reads current Info
time, then dispatches the Dimension angle. Normal celestial smoothing uses
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
Missing generation/population capabilities fail explicitly after any reached
load/acquisition effects. Known-empty entity/tile records retain the existing
storage activation boundary.

The separate [block access contract](blocks.md) covers the implemented quiet
metadata getter/setter and their selected native validation. Full block-ID
mutation and notifying wrappers retain their additional unfinished behavior.

## Native limits and retirement

Default World direct storage is bounded to 16 MiB and 65,536 scheduled entries.
Provider storage has a separate 8 MiB direct bound and 512 cache/list/pending
slots. Its Integer HashSet history retains the existing separate 8 MiB retained
and 16 MiB peak bounds, with its entry limit clipped to the provider bound.
Dimension direct storage defaults to 4 MiB, with separately bounded normal
biome buffers. Info, SavedData and store payloads retain their own limits.
These native allocation policies do not reproduce Java object/array OOM timing.

A reached construction failure latches FAILED_CONSTRUCTION, preserving actual
World fields and shared profile effects. The same World cannot retry. An
unreturned provider child can be explicitly freed without undoing those effects.
Inspection borrows live references; the exposed Random pointer has the World
lifetime and does not add arbitrary public Random replacement/alias ownership.

Native retirement requires no entered call/local lease and empty owned
collections/cache/list/pending state. It unregisters actual contexts and clears
only this Dimension's binding to this World, without invoking biome setup.
It never clears the aggregate persistent-binding count. External Entity/Tile
bindings keep free BUSY until their owners release them. No implicit save,
file rollback, profile reset or retry occurs.

See [Dimension reference evidence](../reference/dimension.md),
[Info ownership](info.md), [SavedData](saved-data.md) and
[runtime storage](storage.md) for their separate qualification boundaries.
