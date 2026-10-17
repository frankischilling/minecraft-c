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

Original binaries, assets, raw decompiled source, launcher files, credentials,
recordings and personal worlds remain outside public source and releases. Copy
worlds before any writable reference experiment. Project code uses MIT; zlib
retains its own license, and neither license covers original Minecraft material.
