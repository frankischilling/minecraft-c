# World height, light and sky queries

The concrete GAME queries in [`queries.h`](../../include/mc/world/runtime/queries.h)
use the installed World provider and its actual Chunk identities. These methods
support normal population and plant survival. They have different bounds,
acquisition and class-initialization behavior; one bounded block lookup cannot
replace them.

| Query | Bounds and acquisition | Normal Chunk result | Blank result |
| --- | --- | --- | --- |
| `mc_world_runtime_top_solid_liquid` | No World border gate; acquire once before masking X/Z | Scan Y127 through Y1; return the first solid or liquid material's Y+1, otherwise -1 | Scan air through the same reached Material operations; return -1 |
| `mc_world_runtime_saved_light` | Clamp Y to 0..127; outside the asymmetric World border read supplied Light.c; inside, check presence before acquiring | Compare actual shared Sky then Block identities and read the selected Nibble | Zero, without Light active use |
| `mc_world_runtime_plant_light` | Negative Y returns zero before acquisition; high Y clamps to 127; no X/Z border gate | Read sky, set shared Chunk.a when positive, then read block light and return their maximum | Zero |
| `mc_world_runtime_sky_visible` | Acquire without Y or World-border rejection | Compare unchanged Y against the actual unsigned height-map byte | False |

World.c invokes the provider's `b` getter. A cached hit preserves pending work;
a permitted miss can load or generate. The dedicated cache's denied-load branch
returns its actual blank fallback. These queries do not force the separate `c`
entry, whose pending-removal behavior differs.

The downward query keeps the acquired Chunk throughout the scan. Each nonair
cell performs a reached live Block.m lookup and then reads that Block's material.
Air uses shared Material.a without activating Block. Material.c is tested first;
Material.d is invoked only when c is false. Y0 is never inspected. Cached height,
the separate upward top-block method and support/opacity predicates do not
determine this result.

Saved-light presence misses return zero without initializing Light. A present
normal Chunk initializes Light at its actual static comparison, including when
the supplied reference is NULL; a NULL ordinarily matches neither identity and
returns zero. Outside the border the same NULL fails when Light.c is read.
Dispatch compares live static references, not a detached light-kind number.
Normal plant-light and sky-visible queries do not initialize Light.

Chunk.a belongs to the shared class owner, above individual Worlds and Chunks.
It starts false once and remains true after positive sky light, including when
the later block-light read fails. World/Chunk construction and retirement do not
reset it. `mc_world_runtime_chunk_light_seen` reads this same cell passively.
Renderer reset behavior is separate and remains unfinished.

Each call holds its World. The captured normal Chunk receives an exact Store
lease across Registry initialization and synchronous reentry. These holds do
not suppress nested acquisition or replace source locals with later cache
lookups. Native failure preserves the ordinary output and all earlier effects.
The error frame distinguishes the outer World invocation, provider/Chunk PCs,
storage cause and complete Registry cause. Native admission leaves prior World
diagnostics intact. RAW, constructing and failed GAME prefixes remain usable at
their actually reached fields; retired Worlds reject.

The [paired source map](../reference/world-queries.md) records the authority and
method locations. Native query, material-array, [plant predicate](plants.md),
[plant features](population/plants.md), trees and geology contracts pass 6,110 checks per host.
All 142 configured public cases pass on Windows MSVC and all 144 on each
sanitized Linux GCC/Clang build. Each host retains the same 661 selected inputs,
254 external zlib source pins, 143 products and selected executable pins before
and after execution, including Java/javac.
Tests verify captured-Chunk continuation through Registry reentry, pending-key
preservation, live nullable fields, cold Light, shared class state, packed
aliases and prefix failures. Six complete region baselines containing eight
records remain byte-equal; the complete inventory also includes two exactly
empty provider-capacity regions. The denied-policy test is an explicit native
input control on a base World, not an executed dedicated-World constructor.

Separate untouched-original client/base-server experiments each record 44 calls,
55 operations, 112 snapshots, 170 raw Contexts and 35 complete byte-array images.
They cover cold direct Chunk APIs and genuine normal World queries, with prepared
instance inputs recorded explicitly. Original World Blank was not induced.
These observations have no native replay or full save comparison.

These queries do not establish complete plant callbacks, feature population,
light propagation or save interoperability.
