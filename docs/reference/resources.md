# Local client resource inventory

This note records archive inspection, client bytecode, and a local import on
2026-10-17.
It does not establish rendered or audible parity.

## Reference identity and observations

The client source is the supplied `b1.7.3.jar`, 1,465,375 bytes, SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
These findings apply to that client only. They make no dedicated-server claim.
The JAR and the external resource files remain in ignored or external local paths.

| Class and method | Descriptor | Bytecode offsets | Observation |
| --- | --- | --- | --- |
| `net/minecraft/client/Minecraft.a` | `()V` | 241 through 259 | Constructs `sj` with `/font/default.png`. |
| `sj.<init>` | `(Lkv;Ljava/lang/String;Lji;)V` | 28 through 34 | Loads that font image through `Class.getResourceAsStream` and `ImageIO.read`. |
| `fp.a` | `()Ljava/lang/String;` | 3 through 24 | Opens `/font.txt` with an explicit UTF-8 reader. |
| `cz.<init>` | `(Ljava/io/File;Lnet/minecraft/client/Minecraft;)V` | 25 through 36 | Uses a `resources/` directory under the original working directory. |
| `cz.a` | `(Ljava/io/File;Ljava/lang/String;)V` | 0 through 99 | Recursively visits local resource files and calls the client's resource-registration method with relative paths. |
| `net/minecraft/client/Minecraft.a` | `(Ljava/lang/String;Ljava/io/File;)V` | 0 through 22 | Splits the first path component from a resource path. |
| Same method | Same descriptor | 23 through 64 | Registers `sound` and `newsound` with `yo.a`. |
| Same method | Same descriptor | 67 through 86 | Registers `streaming` with `yo.b`. |
| Same method | Same descriptor | 89 through 127 | Registers `music` and `newmusic` with `yo.c`. |
| Same method | Same descriptor | 118 through 130 | Returns after the recognized groups; no branch recognizes `sound3`, `icons`, or `pe`. |

Reproduce the bytecode observations locally with:

```powershell
javap -classpath b1.7.3.jar -c -p -s net.minecraft.client.Minecraft
javap -classpath b1.7.3.jar -c -p -s sj fp cz
```

The resource routing is a direct bytecode observation. Calling these pools sound
effects, streamed records, and background music is an interpretation of the
resource names and downstream use. Audio decoding and playback are not tested by
the import tool.

## Inside the client JAR

The archive contains 83 non-class resource files totaling 511,342 bytes, excluding
the zero-byte entry named `null` and `META-INF` metadata. There are 77 PNGs and six
text resources. The exact filename allowlist is `JAR_RESOURCES` in
[`scripts/resources/import_assets.py`](../../scripts/resources/import_assets.py).

| Resource area | Files |
| --- | ---: |
| Root (`font.txt`, `pack.png`, `pack.txt`, `particles.png`, `terrain.png`) | 5 |
| `achievement/` | 3 |
| `armor/` | 11 |
| `art/` | 1 |
| `environment/` | 3 |
| `font/` | 1 |
| `gui/` | 13 |
| `item/` | 5 |
| `lang/` | 2 |
| `misc/` | 11 |
| `mob/` | 22 |
| `terrain/` | 2 |
| `title/` | 4 |

PNG header inspection found `terrain.png` and `gui/items.png` at 256 by 256
pixels and `font/default.png` at 128 by 128 pixels. `font.txt` contains 252 bytes.
All 83 required archive resources were present. Their presence does not show that
the C client can display them.

## External resources

The local `%APPDATA%/.minecraft/resources` directory contains these groups:

| Group | Files | Bytes | Import decision |
| --- | ---: | ---: | --- |
| `sound` | 16 | 110,323 | Import; client recognizes this group. |
| `newsound` | 283 | 3,472,952 | Import; client recognizes this group. |
| `streaming` | 15 | 21,071,230 | Import; client recognizes this group. |
| `music` | 3 | 6,740,194 | Import; client recognizes this group. |
| `newmusic` | 9 | 12,616,640 | Import; client recognizes this group. |
| `sound3` | 417 | 5,334,905 | Exclude; this client does not register this group. |
| `icons` | 3 | 123,813 | Exclude; this client does not register this group. |
| `pe` | 1 | 35,003 | Exclude; this client does not register this group. |

The five recognized groups contain 326 files totaling 44,011,339 bytes. All 313
`.ogg` files in those groups had `OggS` headers. The remaining 13 files use `.mus`
under `streaming/`; the importer copies their bytes without decoding them.
`streaming/13.ogg` and `streaming/cat.ogg` are also present.

No required import path was missing in this local inspection. The importer checks
an availability floor of six files: `sound/step/stone1.ogg`,
`newsound/random/click.ogg`, `music/calm1.ogg`, `newmusic/hal1.ogg`,
`streaming/13.ogg`, and `streaming/cat.ogg`, along with nonempty recognized
directories. These six files are local tool preconditions, not a reconstructed
canonical Beta audio manifest. No trusted external-resource manifest or original
audio hashes have been established. Extra tracks in the local directory do not
prove that Beta 1.7.3 used them. Canonical audio completeness and file provenance
remain unverified.

## Local import and validation

The importer requires Python 3.10 or later during development. It takes explicit
local inputs and makes no network requests:

```powershell
python scripts/resources/import_assets.py --jar b1.7.3.jar --resources "$env:APPDATA/.minecraft/resources" --output .local/assets-b1.7.3
```

The observed import succeeded with 409 files and 44,522,681 resource bytes, plus
`manifest.json`. JAR resources are under `jar/`; external resources are under
`resources/`. The manifest records each imported file's relative path, byte count,
and SHA-256. It omits absolute input paths. The output is local and ignored.

The destination must be fresh. A repeated command with that destination reports
that it already exists; use a new local output directory for another import.
Inside the checkout, outputs must be under `.local/`, `assets-local/`, or
`research-local/`. The importer rejects unsupported client hashes, missing required
paths, duplicate ZIP entries, portable-path escapes, links and Windows reparse
points, unsupported external extensions, and inputs above size or entry limits.
It checks the complete input plan before copying, copies only approved paths into
a temporary directory, and publishes the result after validation succeeds. A
failed import does not overwrite existing resource files.

The current bounds are 16 MiB per JAR or resource file, 256 MiB of imported
resource data, 8,192 archive entries, 8,192 visited external entries, and 64 KiB
copy chunks. Ogg headers are only a format check. The tool does not validate PNG
decoding, Vorbis streams, `.mus` decoding, asset semantics, or actively racing
changes by another local process.

Public verification needs no original files:

```powershell
python -m unittest discover -s tests/tools/resources -p test_import_assets.py -v
```

Result on Windows: 16 tests passed, including synthetic successful imports,
missing inputs, rejected client hashes, path traversal, duplicate/archive links,
external and destination links, reparse attributes, size limits, and cleanup after
invalid Ogg data. Tests patch the module's known hash only around generated
fixtures; the production command has no hash-bypass option.

There is no runnable C asset consumer yet. The intended native consumer will use
the imported directory without a JAR or Java runtime. That behavior remains to be
implemented and verified, as do rendering, audio, resource reloads, and parity
against controlled original-runtime observations. These requirements extend
beyond this local import increment.
