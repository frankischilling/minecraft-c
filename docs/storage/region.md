# Read-only region records

`mc_storage` exposes [region.h](../../include/mc/storage/region/region.h). It
parses a borrowed byte span without heap allocation or file I/O. All input and
returned payload spans stay immutable until the caller discards the view.
There is no creation, repair, padding, sector allocation or save write operation.

`mc_region_open` requires the complete 8192-byte header and defaults to a 1 GiB
input limit. It retains all 1024 raw location/timestamp pairs, including
timestamps for missing entries. Locations contain an unsigned 24-bit sector
start and 8-bit sector count; each sector is 4096 bytes. Offsets are calculated
with 64-bit arithmetic. Partial final sectors are reported through
`trailing_bytes`, with no repair or implicit padding.

Missing entries have location zero. Nonzero locations are checked for header
references, empty extents, unavailable sectors and overlap. Both entries in an
overlap are diagnosed, including ranges that also have other issues. Work is
bounded to 1024 ranges; it does not allocate a bitmap based on a stored offset.
Unrelated metadata stays available when another record is invalid.

Record headers are inspected only after location validation. Their signed
length must be positive and `4 + length` must fit the allocated sector extent
and actual input. Length includes one selector byte and the compressed payload.
Selector 1 means gzip; selector 2 means zlib. Unknown selectors have a distinct
unsupported status. Decompression and raw NBT parsing remain separate modules.

| Issue mask | Meaning |
| --- | --- |
| 1 | References a header sector |
| 2 | Nonzero location with an empty extent |
| 4 | Extent is unavailable in the supplied span |
| 8 | Overlaps another present nonempty extent |
| 16 | Invalid signed record length |
| 32 | Declared record exceeds its extent/input |
| 64 | Unsupported compression selector |

Open succeeds with per-entry diagnoses when the header is available. A fatal
argument/header/limit error leaves the caller's view unchanged. Record lookup
distinguishes missing, invalid, unsupported and invalid-coordinate results,
leaving payload output unchanged on failure. Both local coordinates must be
0..31, including metadata lookups. There is no global coordinate, filename or
cache mapping. See [reference evidence](../reference/region.md) for scope.

## Inspection command

The normal build includes `mc_region_inspect`, even with `BUILD_TESTING=OFF`:

```sh
build/mc_region_inspect supplied-region-file
build/mc_region_inspect --all supplied-region-file
build/mc_region_inspect supplied-region-file 0 0
```

On Windows use `build\mc_region_inspect.exe`, or the generator's configuration
directory. Windows uses wide arguments/file paths; Unicode and space-containing
paths are tested. Input files open only in binary read mode. Use `--` before a
filename beginning with a dash. The tool creates no output files.

Default JSON lists present entry metadata; `--all` includes all 1024 entries.
Supplying local X/Z selects one entry and decodes its container and compound.
Selection takes precedence over `--all`. Metadata mode checks record framing
without decoding containers. Selected output reports compressed consumption,
compressed trailing bytes, decompressed size, raw NBT consumption and raw
trailing bytes separately. Decoder errors retain the available region metadata.

NBT summaries show at most 128 UTF-16 units per name, 32 children per collection
and two child levels. Full lengths/counts and truncation flags make omissions
explicit. Summaries do not apply or validate a world/chunk schema; the retained
NBT document itself remains unchanged.

Limits use decimal byte counts: `--max-input-bytes`, `--max-output-bytes`, and
`--max-allocation-bytes`. Defaults are 64 MiB file input, 16 MiB decompressed
output and 64 MiB for each decoder's allocation budget. These budgets are
independent; input loading, compression and NBT are not a single total process
memory budget. The reader checks one extra byte when testing an exact file cap.

| Exit | Result |
| --- | --- |
| 0 | No diagnostics in the requested inspection |
| 1 | Header/record/container/NBT diagnostic, partial sector or extra trailing bytes |
| 2 | Usage, I/O, allocation or resource limit failure |
| 3 | Selected entry is missing |

The CLI is an inspection tool. It does not load a playable world, infer region
filenames, validate embedded coordinates, replace saves or establish writable
compatibility.

## Verification boundary

The live comparison runs untouched client/server region readers on 36 generated
cases and 49 ordered queries per side, with a separate copy for every query and
fresh repeated controls. Original construction may repair its owned copies;
C inspection must preserve all 36 pristine inputs. Complete metadata, original
outcomes, drained bytes and canonical roots remain in the records.

Eight queries compare compatibly between C and each original. Another 41 retain
explicit C diagnostic departures for malformed/repairing files, invalid
coordinates or containers. They validate the C read-only contract without
claiming identical malformed-file behavior. The complete client/server original
records and each side's complete control snapshots must agree. See
[live evidence and reproduction](../reference/region.md#live-original-comparison)
for the exact observation and provenance requirements.

`MC_ENABLE_REGION_REFERENCE_TESTS=ON` enables the opt-in `reference.region`
comparison with both supplied JARs, JDK17+ and a source commit label. The
Java-free `tools.region_live` suite has 17 protocol/geometry/provenance guards.
`tools.region_probe` checks the C probe's UTF-8 stdin paths with Unicode
directory/file names, complete header/data JSON and unchanged inputs; it passes
on MSVC and WSL GCC/Clang. The standalone CLI's wide Windows arguments remain
a separate tested entry point.

A guarded live development run passes the finite corpus and controls. Final
exact-source/platform acceptance and integration remain outstanding for this
worktree. Historical `reference.region_capture` replay remains a distinct
recorded-observation check that executes no Java. Neither route implements
region allocation/writes, world schemas, cache lifecycle or save round trips.
The [test guide](../testing.md) records the wider project validation scope.
