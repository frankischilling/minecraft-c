# Nearby Entity source map

Authority is the verified original [client and server binaries](binaries.md).
The complete paired bodies below agree after their actual side symbols and
constant-pool indexes are mapped. This comparison preserves instructions,
branches, PCs, constants and field order. Supplementary mapping names are
navigation aids; client and server obfuscated letters remain separate.

| Body | Client | Dedicated server | Reached source sites |
| --- | --- | --- | --- |
| World nearby query | `fd.b(Lsn;Leq;)Ljava/util/List;` | `dj.b(Llq;Lcz;)Ljava/util/List;` | Current R1/clear4; min/max X floors21/37; min/max Z floors54/71; presence102; acquiring getter113; current R119/gather122; current R138/return141 |
| Normal Chunk gather | `lm.a(Lsn;Leq;Ljava/util/List;)V` | `hi.a(Llq;Lcz;Ljava/util/List;)V` | Y floors12/29; current m length48, clamp reload56; current m73/index78; captured List size88/get100; cast105; identity exclusion113; bounds118/intersect122; destination add131; return149 |
| Blank gather override | `li.a(Lsn;Leq;Ljava/util/List;)V` | `hg.a(Llq;Lcz;Ljava/util/List;)V` | Literal return0, with no argument or receiver-field read |
| Private nonloading presence | `fd.f(II)Z` | `dj.g(II)Z` | Current v1/provider presence6/return11 |
| Acquiring getter | `fd.c(II)Llm;` | `dj.c(II)Lhi;` | Current v1/provider getter6/return11 |
| Double floor | `in.b(D)I` | `fq.b(D)I` | d2i1, compare6, wrapping subtract12, return14 |
| Strict box intersection | `eq.a(Leq;)Z` | `cz.a(Lcz;)Z` | Rejection returns25/51/77; true79 |

World clears before the first nullable query read. Presence does not bypass the
acquiring getter or its real descendants. Chunk coordinates passed to `c(II)`
are already divided; routing them through a block-coordinate getter would shift
them again. The destination is loaded after acquisition, captured for each
Chunk call, then the return value reloads current `R` independently.

Normal gather captures each selected slice List, but not its initial size or
backing array. Its reference comparison skips the excluded candidate before
reading bounds. No equality, type-kind, dead, player or construction-World test
appears. Each duplicate is considered separately. Null destination is reached
only by an actual matching candidate. A null query fails before the current
slice array's length read, even when the eventual scan would be empty.

The actual floor helper first narrows to Java int, then subtracts one with
wrapping when the original value is smaller than the narrowed integer. NaN
therefore yields zero; negative infinity yields `INT_MAX`. The World's inclusive
loops can wrap indefinitely at an entered `INT_MAX` upper endpoint. Normal
gather clamps its upper slice to the actual array's length minus one, but does
not clamp an oversized lower slice down or a negative upper slice up.

An all-NaN query selects slice zero and can intersect every non-excluded
occurrence there. Positive-overlap conjunctions would change that behavior.
World captures four integer loop limits once, while later Chunk calls and
intersections continue to read the original query box. A native hold protects
its lifetime without preventing pooled coordinates from being reused.

The [native contract](../world/collision/entities.md) records C ownership and
explicit native failure policies. Full collision `K`, concrete Block shapes,
Entity movement and later FallingSand behavior remain separate unfinished work.

## Original observation and native replay

The public [reflection helper](../../tests/tools/world/collision/NearbyObservation.java)
invokes the genuine normal superclass method on actual normal and Blank
instances, preserving virtual dispatch. Both original null-World Chunk and
Item/FallingSand constructors run successfully. Reflection sets their actual
slice ArrayLists and six constructor-owned bounds fields as explicit inputs.
No original source is recompiled and no substitute engine class is loaded.

The [comparison driver](../../tests/tools/world/collision/compare_nearby.py)
creates a fresh original capture, then runs the separately compiled C probe.
Native setup uses actual GAME World/store-owned Chunk construction, genuine
Entity construction/insertion, and assignment of the same six bounds inputs.
It seeds existing destination contents through real gather, clears R through
the actual empty-range World query, and retires unpublished Chunk owners through
checked native storage. Constructor equality and full Chunk-state equality are
not claimed by this controlled gather projection.

Each original capture has 364 rows per side. Native replay selects 356 ordinary
and Blank cases: 353 successful rows compare identity order/count and unchanged
source projection exactly; three nullable cases compare the reached native
unsupported status/PC and retained destination prefix. The probe prints its
real native failure rather than synthesizing a Java exception. Eight malformed
array/List cases remain captured and explicitly excluded. Exact native Registry
bootstrap lines are retained separately without removing them from raw stdout.
Each of MSVC and sanitized GCC/Clang passes the fresh paired comparison.

With Java/javac on PATH, both pinned local JARs present, and the normal test
build completed, run from the repository root:

```sh
python tests/tools/world/collision/compare_nearby.py --native build/mc_world_nearby_values_probe --output build/nearby-original
```

Use the `.exe` probe on Windows and a fresh output directory for every run.
`capture_nearby.py --output <fresh-directory>` runs the original-only experiment.
Both tools accept `--repo` for the supplied JAR/source root. The comparison
retains commands, complete streams, source/tool/JAR/class/executable hashes and
every input/result row. It does not build native code or pin the entire JVM;
compiled-product/source binding is verified separately. The C runtime and probe
run without a JVM outside these development-only original comparisons.
