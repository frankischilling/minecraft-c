# World height queries and lighting storage

`mc/world/runtime/lighting.h` adds concrete GAME operations to `mc_simulation`.
They use the constructed World's current provider, its actual C list and the
class owner's shared A counter. The separate caller-owned numerical interfaces
remain documented under [finishing](finishing.md) and [queue bounds](lighting.md).

## Height queries

`mc_world_runtime_height` follows `fd/dj.d(II)I`. X must be within
`[-32000000,32000000)` and Z within `[-32000000,32000000]`; outside those bounds
the result is zero before provider access. Inside, a nonloading presence check
uses the current provider and signed chunk shifts. Missing cached entries return
zero even when their data exists on disk or an unload request is pending.

Presence and acquisition are separate operations. A present entry reaches the
ordinary getter, then reads its unsigned local height byte. Actual blank chunks
return zero. Packed-key aliases retain the existing cache behavior. The call
does not generate an absent neighbor merely to obtain its height.

The World stays held through the whole operation. Native admission rejects
COMPOSITION and RETIRED owners. RAW and failed GAME owners retain their reached
field behavior: an outside-border call can succeed before an unavailable
provider; an inside-border call fails at source33. Output remains unchanged on
failure. Error frames distinguish presence33/provider6, getter48 and height61.
An OK acquisition field means no getter attempt when the method returned early.

## Shared Light identities and actual C records

Each World class context lazily constructs two immutable Light handles, Sky
with c=15 and Block with c=0. They are shared across its Worlds and distinct
from another class context's handles. This finite interface does not implement
generic Java Enum methods. Type views borrow the live class context.

`mc_world_runtime_light_submit` accepts a same-class handle or a real nullable
type reference. It preserves all six signed bounds, including reversed and
wrapped cases. It reads current WorldProvider.e first. A no-sky Sky request returns
before incrementing A. Other requests increment that same shared counter with
Java wrapping; only equality with 50 skips. Every guarded exit subtracts one
from the current A, including failure paths.

Wrapped midpoint sums and truncating division select the presence query. This
query has no X/Z world-border test. A present midpoint independently reaches the
getter and blank predicate. Only then does the method read actual C storage.
Coalescing examines the newest five records, counting both types within that
window. The first successful same-reference merge retains the record identity.
Nullable types can also match each other. Disabling coalescing always appends
after the ordinary admission checks.

Every append allocates a distinct owned region. Pointer-array growth preserves
region addresses. Both requests count against the World's native direct-byte
budget, including the old and replacement arrays at peak. The initial pointer
capacity is ten and later growth is 1.5 times the preceding capacity, bounded by
the native span and signed source-list range. This is a native storage policy;
it does not reproduce JDK allocation sizes or Java OOM timing.

After the million-and-first real append, successful diagnostic output precedes
clearing that same list. Native output failure preserves the appended records
and reports IO_ERROR at289. Clear frees region owners and retains the C identity
and pointer capacity. Failed record219 or pointer-array237 allocation preserves
existing records; the unpublished record is freed after a failed add.

Passive views expose current C identity/count/capacity and indexed region fields.
Borrowed region addresses survive growth and merging, and expire on clear or
free. Native World retirement keeps C storage available for inspection until
final free; it performs no implicit gameplay clear or propagation. Other
unimplemented World collections retain their emptiness requirement at retirement.

Native admission changes only its returned error. Reached failures retain their
complete storage cause in World.last_error. Success preserves prior diagnostics
except for the getter's own ordinary effects. Callers keep dependencies live,
grant exclusive access and keep arguments, output, error and owner storage
disjoint.

## Evidence and remaining work

The [paired source anchors](../reference/lighting.md#concrete-world-source)
qualify method order on both supplied originals. Independent native contracts
exercise real constructed client Worlds, actual store-owned restored chunks,
natural allocation limits and explicit shared-A scalar controls. Their
qualification is recorded in [testing](../testing.md).

All 33 selected cases pass on MSVC/GCC/Clang, including 177 height and 2,000,382
queue checks, with Linux ASan/UBSan and leak detection. The real overflow
transcript contains the naturally reached achievement and recipe lines before
the single diagnostic. Nonempty-growth tests admit the final retained size
while rejecting its old-plus-new array peak and preserve every preceding
record. Type initialization leaves the actual profile uniquifier unchanged.
Forced queue output failure remains a source-reviewed boundary; the reused
platform writer's closed-handle and broken-pipe controls pass separately.

The [generated Chunk constructor and finalizer](chunk-objects.md) now call these
operations on the same store-owned Chunk. All 38 selected native cases pass on
MSVC/GCC/Clang, including 33,458 new Chunk checks and Linux sanitizers.
Queue draining, light propagation, complete GAME generation, synchronous block
mutations and population remain unfinished. No new executed-original World
comparison or complete startup/persistence acceptance follows from this module.
