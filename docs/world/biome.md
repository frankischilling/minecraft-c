# Climate and biome module

`mc_world` contains the C17 module at `src/world/biome/biome.c`, with its
interface in `include/mc/world/biome/biome.h`. It combines the original three
simplex/octave channels, climate processing and biome lookup. It initializes no
world, files, display, assets or Java. [Reference evidence](../reference/biome.md)
distinguishes the explicit-seed numerical fixture from original world startup.

The [shared Biome registry and reference sampler](biome-registry.md) provide
the actual class-owned objects, spawn lists and pointer table. They have
separate initialization, lifetime and partial-effect contracts. The numerical
interfaces on this page retain their existing finite-input and preflight
behavior.

## Classification and seeded state

`mc_biome_classifier_init` builds all 4096 entries using binary32 index/63
division and the original float selector. `mc_biome_classify_float` exposes that
selector directly. `mc_biome_lookup` quantizes each double input by truncating
its product with 63, then indexes the initialized table. These operations can
produce different results for the same decimal inputs. Both selectors accept
finite temperature/rainfall values in [0,1]; failures preserve their result.

The thirteen `mc_biome_id` values encode the original static object identities
`a` through `m`. They are project encodings, not a game registry or wire format.
`mc_biome_name` and `mc_biome_field` expose the original labels. `MC_BIOME_NONE`
represents an unset array element. Ice Desert, Hell and Sky exist as identities
but are not selected by this climate table. Live surface application, spawning,
feature placement and World/Dimension integration are separate from this
numerical projection.

A caller-owned `mc_biome_manager` embeds its classifier and ten simplex layers.
Initialization uses three independent wrapped seed products, 9871/39811/543321,
and 4/4/2 layers. Sampling preserves promoted float scales, octave controls,
double arithmetic grouping and clamp order. Sampling consumes no Random state.

## Mutable buffers and calls

The manager exposes four reusable module-owned buffers: `a` is processed
temperature, `b` processed rainfall, `c` raw auxiliary noise, and `d` retained
biomes. These arrays are recomputed on each call. Sharing a mutable manager
between threads requires synchronization.

| Function | Outputs and retained state |
| --- | --- |
| `mc_biome_temperatures` | Processes a rectangle into a separate double buffer and updates `c`; leaves disjoint `a`, `b`, `d` unchanged |
| `mc_biome_sample_square` | Updates `a`/`b`/`c` and a separate biome buffer; leaves `d` unchanged |
| `mc_biome_cached_square` | Updates `a`/`b`/`c` and retained biome buffer `d` |
| `mc_biome_point` | Performs a retained 1×1 lookup and returns its first biome |
| `mc_biome_chunk` | Wraps each signed chunk coordinate shifted left by four, then performs the point lookup |
| `mc_biome_client_raw_temperature` | Original client-only 1×1 raw temperature-channel path with frequency factor 0.5; updates `a` only, without climate processing |

Null or short arrays grow to the exact requested product. Longer arrays retain
their length and identity. Double sampling clears the whole retained array,
including its unused suffix, before processing the requested prefix. Biome
sampling replaces the requested prefix and preserves a longer biome suffix.
Zero dimensions are accepted: null outputs become allocated empty arrays,
and retained double arrays are still cleared. Temperature layout is X outer,
Z inner; biome grids use the same layout with equal dimensions.

The square restriction preserves a known scope limit: the original biome
method passes width twice to its noise calls before consuming width×height.
Temperature-only calls forward both dimensions. No rectangular biome behavior
or original array failures are claimed by the current C interface.

## Ownership, limits and failures

Zero-initialize memory owners, managers and buffer descriptors before first
use. Initialize one caller-owned `mc_biome_memory` with an explicit byte limit;
`MC_BIOME_DEFAULT_ALLOCATION_BYTES` supplies a 64 MiB default. The manager and
its external output buffers share this owner. The limit counts live allocation
headers and payloads, including old and new storage during growth; it excludes
system allocator overhead and caller-owned manager storage.

The module allocates and grows buffers. The public resize helpers create exact
zeroed replacements when the length differs, retain an existing exact-length
array, and distinguish allocated empty arrays from null descriptors. Callers
may inspect or prefill elements, but descriptor fields remain read-only.
Owning descriptors and managers must not be copied. Growth invalidates old
views. External outputs must be disjoint from manager state and its buffers.

All dimensions, products and buffer lengths must fit `INT32_MAX` and their byte
span. Invalid arguments, limits and allocation failures have separate statuses.
Sampling stages every needed replacement before changing any buffer; failures
preserve prior pointers, lengths, values, scalar results and live accounting.
These are bounded C call contracts, not approved changes to valid gameplay.

Free separate output buffers explicitly. `mc_biome_manager_free` releases only
its four retained buffers and resets the manager; the memory owner must outlive
every allocation. Reinitialization requires freeing the previous manager first.
The [test guide](../testing.md) records public and opt-in original comparisons.
