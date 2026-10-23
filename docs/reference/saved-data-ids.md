# SavedData ID reference and verification

This checkpoint records the ID-counter owner used by original World
construction. Its ID-only evidence excludes complete World construction and
named cache/subtype behavior. The current [ownership contract](../world/saved-data.md)
also describes the named MapData manager; its [separate comparison](population-named-data.md)
covers one default value in a held World.

## Paired original authority

The verified archives in [binary evidence](binaries.md) supply client `hc` and
server `es`. Their own constructor, ID loader and next-ID instruction sequences
agree at these relevant locations:

| Operation | Original reached order |
| --- | --- |
| Constructor | New map/list/map stored at 12/23/34, storage at 39, ID loader at 43, return at 46 |
| ID load | Clear at 4, fixed filename at 23, raw Compound reader at 57, close at 62, Short-only filter at 100 and map insertion at 138 |
| Next ID | First Short zero at 19; existing increment/narrow at 33/34; map insertion at 50 before file work |
| ID write | Fixed filename at 74, build actual Compound, open/truncate at 171, raw writer at 183, close at 188; caught Exception still returns the selected Short at 203 |

Storage is client `wt`/`fm` and server `om`/`dq`. The actual filename delegates
append `.dat` beneath the handler's data directory. They do not check session.
Client `ul` and server `mv` are the Short tags. The raw `as`/`ag` DataInput and
DataOutput overloads differ from their gzip stream overloads used by named
SavedData.

NULL and nonnull-empty counter keys remain separate in the original HashMap.
The NBT base's name getter exposes empty text for a NULL tag name, so both keys
can emit physical empty-name children. Reload retains the last serialized
duplicate. The actual collection profile determines that order; native insertion
order cannot establish the original winner.

## Actual original capture

The [observer](../../tests/reference/world/SavedDataIdObservation.java) executes
unmodified original classes through an isolated loader. Windows Temurin
17.0.17+10 uses `-Xint`. One actual compile and one process per side finish with
exit zero, natural reaping and empty outer stderr. Twelve input fingerprints
and five compiled observer classes remain unchanged.

Each side retains 320 literal rows, sixteen distinct owners, 65566 actual
next-ID calls, 38 selected returns, 49 actual map snapshots and 33 file snapshots.
Nine disk cases construct the real `fm`/`dq` handler, call its actual filename
delegate and preserve its real initial session bytes separately. A tenth storage
case is explicitly a path-only Proxy control. There are eleven cases including
NULL storage, and 110 retained regular files per side. No native output supplies
an expected value.

The cases cover the Short wrap/continuation, missing and prepared files,
raw save/reload, stale session, independent owners, malformed data, a directory
target, NULL/empty collision, overlong modified UTF and the isolated path trace.
Prepared fields use original tag constructors/writers, including physical
duplicates and trailing bytes. All file bytes remain untouched in raw transcript
hex and separate snapshot files. Each of 54 scoped diagnostic captures per side
retains the actual printed bytes, with the explicit UTF-8 capture encoding.
Paths, stack frames, line endings and error messages are not rewritten.

The real handler clock is not a fixed input or stable native expectation.
Its raw token remains in retained files. The stale-session case instead uses
an explicit distinct test-input token and observes that ID operations leave it
unchanged. These are standalone original manager/handler experiments, without
constructing a World or proving complete storage interoperability.

## Public memory projection

The [exporter](../../scripts/reference/world/export_saved_data_ids.py) derives
the public fixtures only from the captured NULL-store history. It keeps all
fifteen selected returns and seven actual map snapshots with fifteen values per
side. Original call-index differences become native repeat requests, which
execute all 65543 next-ID calls per side.

The projection maps the single original owner handle to command slot zero and
replaces key handles with their observed UTF-16 content/nullability. It does not
compare Java String argument identity with native copied spans. Original maps
already use canonical diagnostic order; the exporter creates no intermediate
expected maps, modifies no signed value and never rewrites an original file.
Each side supplies 39 exact literal-LF expected rows. The full original capture
remains separate from this deliberately limited public projection.

The [native probe](../../tests/world/saved_data/observation_probe.c) invokes the
actual ID-owner interface, prints only requested snapshots and uses actual
borrowed counter descriptors. It exposes no expected-state producer or clock
hook. The [checker](../../scripts/reference/world/check_saved_data_ids.py)
compares all 78 rows, tests fourteen altered-output rejections, and executes
four malformed-protocol refusals with retained streams and exit status.

These checks do not qualify native disk order, the NULL/empty persistence gap,
arbitrary JVM exception/resource equivalence, forced native short-write/flush/
close errors, named SavedData, actual World startup or population completion.

## Native qualification

The ID-owner unit passes 196,823 constructed checks on native Windows
MSVC 19.51, Linux GCC 13.3 and Clang 18.1.3. The unit and memory replay pass
again within 25 selected storage/runtime cases on each compiler: 66.32s,
139.64s and 139.38s respectively. Linux Debug runs use ASan/UBSan and leak
checks; Windows uses strict Debug without sanitizers. Each replay matches
78 rows, rejects fourteen altered outputs and executes four actual child
protocol refusals. Successful children exit zero; refused inputs exit one.
All six children per replay finish naturally and are reaped.

All 352 code/tool/fixture inputs match before, after and across the three
selected runs. Retained executable hashes bind the native capture reports;
each run has 25 actual CTest passes. clang-format 18 passes all 176 C/header
files. The declared public inventory is 91, without a fresh full-91 or
hosted-CI claim. The earlier complete 89-case checkpoint remains separate.

Public reproduction needs a native build and Python, without game files:

```sh
cmake --build build --target mc_test_world_saved_data mc_saved_data_id_observation_probe
ctest --test-dir build --output-on-failure -R '^world[.]saved_data_ids'
```

The original recorder requires the pinned local JDK and both supplied archives;
missing inputs never count as a successful original run. Capture outputs,
original binaries and worlds remain untracked. Native artifacts remain in the
build's `saved-data-id-contracts` and `saved-data-memory-replay` directories.
