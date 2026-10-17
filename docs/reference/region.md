# Original region evidence

The verified client class `qj` and server class `jx` implement the original
region record storage. Their [JAR fingerprints](binaries.md) remain pinned.
This increment implements a read-only byte-span reader and CLI under
[#14](https://github.com/frankischilling/minecraft-c/issues/14). Filename parsing,
global chunk coordinates, cache lifetime and world schemas remain outside this
evidence. Original classes accept a `File`; their local indexing alone does not
establish those caller behaviors.

The recorded investigation used 36 synthetic fixture definitions and 49 queries
per side/runtime on Temurin 17 and Oracle 21, producing 196 query records. Each
query used a fresh generated copy. No user world or original region writer was
used. Raw source, binaries, copies and complete local runtime reports stay
outside public source. This historical investigation does not count as a fresh
live original-region comparison for the current C revision.

## Valid format anchors

Method-relative offsets below were checked in both supplied binaries with:

```text
javap -classpath b1.7.3.jar -c -s -p qj
javap -classpath b1.7.3_server.jar -c -s -p jx
```

| Interface | Evidence on both sides |
| --- | --- |
| Constructor `(Ljava/io/File;)V` | Allocates two 1024-int arrays; location table read at 297..393 and timestamps at 396..425. Locations are at `4*i`, timestamps at `4096+4*i`. |
| Private `d(II)Z` | Local coordinate bounds 0..17 require both coordinates in 0..31. |
| Private `e(II)I` | Local index `x+32*z` at 4..10. |
| Read `a(II)Ljava/io/DataInputStream;` | Bounds check at 3, missing location check at 28..33, packed start/count at 34..45, seek at 74..85, signed length at 92, selector at 152. |
| Compression selection | Selector 1 constructs gzip at 185/198; selector 2 constructs the default Java inflater stream at 237/250. |
| Timestamp storage | Raw big-endian int bits are retained. The inspected writer derives seconds from current time and narrows to int at 423..430. No timestamp writes were invoked by the observations. |

Recorded valid local mappings include `(0,0)`, `(31,0)` and `(0,1)`, plus raw
timestamp patterns `80000000`, `ffffffff` and `12345678`. Embedded Level
coordinates were returned independently of the queried local slot. The region
reader does not establish a chunk/world schema from those values.

The original constructor opens writable storage and may repair a file. The C
inspector reads only and never calls that constructor. It applies bounded
validation to inconsistent extents, overlap, record lengths and selectors;
those diagnostics are separate from valid-format comparisons. Whole-container
checksums follow the [compression module's policy](compression.md).

## Recorded-observation check

`scripts/reference/region/compare_capture.py` starts only the project's C probe.
It reads a local historical observation manifest and the corresponding generated
files, checks recorded original fingerprints and each before/after file hash,
then selects complete aligned, nonoverlapping files with supported, valid
containers. It does not run Java, an original reader or the capture producer.

The current local capture yields 32 selected observations across four recorded
side/runtime groups. Another 164 observations fall outside this profile. Metadata,
declared payload spans, decompressed bytes and root serialization are compared.
Sixteen serialized roots differ only in compound child order; the same bounded
byte-slice comparison used by raw NBT sorts only compound child records. All
other bytes remain exact. Actual C consumption is reported independently of
historical stream read-ahead.

```text
python scripts/reference/region/compare_capture.py --executable <mc_region_probe> --capture <recorded-manifest.json> --work-dir .local/region-capture --tested-commit <40-hex-commit>
```

For CTest set `MC_ENABLE_REGION_CAPTURE_TESTS=ON`,
`MC_REGION_CAPTURE_MANIFEST=<absolute recorded manifest>` and
`MC_REFERENCE_TESTED_COMMIT=<source commit>`. The optional test is named
`reference.region_capture` and labeled `captured`. Missing captures or probes
return 77/unavailable. Data/execution mismatches return 1/fail. Output directories
containing inputs/source are refused; report paths cannot target fixture files.
A fresh running/failure/unavailable result replaces an earlier report.

The result records C/source/capture/input hashes, selected counts, byte records
and explicit mode `recorded_observations`. Its live original-region check stays
`pending`. Capture replay is recorded-evidence validation. The live comparison
acceptance item remains open, and this branch must not close #14 or claim full
region/save parity on the strength of these records.
