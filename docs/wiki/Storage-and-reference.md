# Storage and original references

Storage currently provides raw NBT plus gzip/zlib memory compression. It does
not implement region addressing, world schemas, writable-world compatibility,
or copied-world round trips. Do not use this increment as a world save writer.

The raw NBT codec handles the original tag IDs 0..10 with named roots, lists,
compounds, byte arrays and lossless UTF-16 string units. Decoded documents own
retained data, including application fields the caller does not interpret.
Duplicate compound names retain the last value; resource limits bound memory,
bytes, tags, elements and nesting. See the
[NBT interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/storage/nbt.md).

Compression accepts an explicit gzip/zlib format and borrowed input bytes,
returning an owned output buffer. It validates complete stream checksums and
trailers. Gzip concatenates adjacent recognized members, while zlib stops after
one stream. Nonmagic compressed tails remain unconsumed. Limits bound input,
output, live allocation and member count. See the
[compression interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/storage/compression.md).

The original gzip NBT wrapper can finish its root before reading a missing or
corrupt trailer. The C module rejects malformed containers through full member
validation; reserved flags and recognized invalid later headers are also
documented departures from the selected modern Java readers. Valid container
payloads compare exactly. Compressed blocks need not match across compressors.

Reference comparisons invoke untouched classes from separately supplied,
hash-verified client/server JARs in isolated JVM processes. They record source,
binary, runtime and input fingerprints plus raw records in ignored local
reports. The C runtime has no JVM dependency. Missing prerequisites remain
unavailable results. Rewritten components agreeing with each other alone do not
establish original compatibility.

[Float lookup/floor evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/math.md)
records the complete initialized table and raw-bit scalar calls. These pure
numerical observations need no world or cave setup. They establish the tested
math prerequisite separately from cave geometry and complete chunks.

Reproduction and exact bytecode anchors live in
[NBT evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/nbt.md)
and [compression evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/compression.md).
The [testing guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/testing.md)
and [development status](https://github.com/frankischilling/minecraft-c/blob/main/docs/status.md)
record current validation scope and outstanding work.

[Normal density evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/density.md)
also documents genuine original world construction for numerical observations.
Each run owns fresh ignored directories; normal server construction can create
a session lock and directories. Tests never open an existing personal world.
These density/base comparisons do not establish copied-world save compatibility
or complete generation.

[Surface evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/surface.md)
separates immutable constructor state from live Random and current material
attributes. Genuine server constructor spawn may generate complete chunks
before a fresh measured generator is created; the later observed pass calls
base/surface directly. These observations do not establish complete chunks,
caves, finishing, population, lighting or save compatibility.

[Normal cave evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/caves.md)
separates ten genuine natural contexts from 44 protected room/path controls.
The natural pass starts after actual base/surface generation and retains all
generator, climate, material and Random state. Focused controls construct no
worlds and observe full buffers, interior/shell water, lava at stored Y10,
grass repair and ineligible bytes. Branching is inferred by unique Random
continuation matching, with the inference recorded explicitly. Cave comparisons
do not establish chunk finishing, population, lighting or save compatibility.

[Chunk evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/chunk.md)
covers fresh voxel arrays, packed nibble access, column heights and direct sky.
Original full finishing calls run in separately owned genuine worlds. Complete
local arrays compare independently between C/client, C/server and client/server;
world cache membership and ordered queued-light bounds remain separate records.
The local C component preserves the original Y127 height exclusion, Y0 sky
exclusion and retained sky values across repeated passes. It does not implement
world-height queries, lighting propagation or chunk lifecycle.

Original binaries, assets, raw decompiled source, launcher files, credentials,
recordings and personal worlds remain outside public source and releases. Copy
worlds before any writable reference experiment. Project code uses MIT; zlib
retains its own license, and neither license covers original Minecraft material.
