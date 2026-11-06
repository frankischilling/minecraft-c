# Chunk voxel data

This module covers
fresh chunk arrays, heightmaps and direct skylight. The public interfaces are
`include/mc/world/chunk/data.h` and `include/mc/world/chunk/nibbles.h`;
their implementations belong to `mc_world`.

The [generated Chunk owner](chunk-objects.md) adds actual World/child identities,
store ownership and a full GAME finalizer above these numerical data functions.

## Storage and ownership

The normal chunk shape is 16 by 128 by 16 blocks. A caller-owned
`mc_chunk_data` borrows a mutable block buffer and retains its capacity.
It embeds separate metadata, skylight and emitted block-light arrays, the
heightmap, and the minimum column height.

| Data | Logical size | Order |
| --- | ---: | --- |
| Blocks | 32768 bytes | X, Z, Y: `x*2048 + z*128 + y` |
| Metadata | 16384 packed bytes | Two four-bit values per byte |
| Skylight | 16384 packed bytes | Two four-bit values per byte |
| Block light | 16384 packed bytes | Two four-bit values per byte |
| Heightmap | 256 bytes | `z*16 + x` |

Even Y uses the low nibble; odd Y uses the high nibble. Extra block capacity
is retained caller storage, not a larger logical chunk. Keep the data object,
blocks and opacity table valid and disjoint for their stated lifetimes. Mutation
requires exclusive access. Different data owners can share an immutable opacity
table. These functions allocate no memory and initialize no platform services.

`mc_chunk_data_init` rejects null data, null blocks or block capacity below
32768 before writing anything. Success retains the supplied pointer/capacity,
clears the three packed arrays and heightmap, and sets minimum height to zero.
Every supplied block byte, including a capacity suffix, remains unchanged.
Reinitialization clears the embedded arrays again.

## Packed access

`mc_chunk_nibbles_get` and `mc_chunk_nibbles_set` accept local X/Z coordinates
0 through 15 and Y coordinates 0 through 127. They require at least 16384
packed bytes. A setter accepts every `int32_t` value and stores its low four
bits while preserving the other nibble. Successful access touches only the
logical array prefix.

Null pointers, short capacities and invalid coordinates return false before
mutation. A failed getter retains its output byte. The getter output must be
valid storage disjoint from the packed input. Caller bounds are explicit;
invalid C coordinates are never passed to the original reference classes.

## Heights and direct skylight

`mc_chunk_data_rebuild_height_skylight` takes initialized data, an opacity
table and an explicit skylight-enabled flag. It validates the block storage
and the first 256 table entries before writing output. Every entry in that
prefix must be a nonnegative `int32_t`; extra entries are ignored. Callers
remain responsible for selecting the table from the current block registry.

The height search begins at 127 and examines physical Y126 down to Y0.
It therefore excludes the top Y127 block from this height calculation.
Columns are visited in X/Z order and stored in Z/X heightmap order; the
minimum starts at 127 and retains the lowest result.

When enabled, direct sky begins with light 15 at Y127, subtracts that block's
actual opacity, and writes a nibble only while the remaining light is positive.
It adds no minimum attenuation for transparent blocks. The pass stops above Y0
and preserves sky values at every skipped position. Repeated passes can retain
earlier sky values. Disabling sky preserves the entire sky array while still
rebuilding heights and minimum height.

Success changes only heights, minimum height and the selected sky nibbles.
Blocks, metadata, block light, opacity, pointer/capacity and capacity suffixes
remain unchanged. Nonnegative opacity values through `INT32_MAX` have defined
C arithmetic; original comparisons use the actual observed vanilla table.

## Scope

This object represents local voxel data. The complete original chunk also
owns world relationships, entities, container state, flags and lifecycle data.
Its finishing method queries cached neighboring heights, changes lighting
queues and marks the chunk dirty. Those effects require world-level code;
this component neither emulates nor silently invokes them.

Original-reference observations and the [parity matrix](../parity.md) track
local array verification separately from complete chunk finishing, light
propagation, population, persistence and playable client/server behavior.
