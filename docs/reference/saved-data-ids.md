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

## Source constructor implementation

The [source SavedData entry](../../include/mc/world/saved_data/source_storage.h)
uses the constructor and private loader above inside the canonical manager.
These class pins identify the inspected bytecode in the supplied archives:

| Class | Bytes | SHA-256 |
| --- | ---: | --- |
| Client `hc` | 4017 | `d6885902c84fe2cf2602d6259a5b2862d0402439529e13ca75a7a5951bfe1385` |
| Client `fm` | 3482 | `82f848599887eaf5cd033d6886db167a3abdf1680151ac9e61fd4ca71a22b529` |
| Server `es` | 4017 | `c6fc8ecb771c7976af9a42c7f1197bd54598aa235ba5c4ecdcdd5e1ebfd2a668` |

Client `hc.<init>(wt)` and server `es.<init>(om)` have the same constructor and
loader offsets listed above. The implemented nonnull source transport uses the
client `fm` handler. The explicit server allocation profile can complete the
null-Store constructor; it does not substitute that client handler for `om`.

`hc.b()` calls the actual `wt.a("idcounts")` File factory at 23, then
`File.exists` at 34. `fm.a(String)` allocates File at 0, captures the actual data
File at 5, appends the nullable name at 16 and `.dat` at 21, forms the new String
at 24 and constructs File at 27. The returned File is the one supplied to
FileInputStream at 49. DataInputStream allocation at 40 precedes FileInputStream
allocation at 44, with its constructor at 52 following the successful
FileInputStream constructor. The raw DataInput NBT reader at 57 precedes source
close at 62 and Short imports at 100/138.

Missing input returns normally without creating `idcounts.dat`. An open or
parse exception reaches the source diagnostic at 152 without running close at 62;
there is no finally. A close exception occurs before any map import. The C
owner retains the typed fault and actual stream prefix at that diagnostic
dependency. Native retirement closes remaining inputs without claiming source
close or normal constructor return. Native capacity/allocation failures keep
their own classification and any completed map prefix.

The [authored source controls](../../tests/world/saved_data/test_source_storage.c)
exercise null/missing input, raw Short filtering and duplicate replacement,
UTF-16 names, a directory target, corrupt input, native bounds, Store retention,
source close failure and retryable native close cleanup. They use real File and
stream owners. Native input/collection budgets, copied counter-name units and
insertion traversal do not qualify Java allocation timing, String identity,
HashMap bucket order or original printed exception bytes. Source-backed dirty
saves, named cache-miss input and next-ID persistence remain explicit
dependencies; they do not enter the legacy path-based or memory transport.

These controls and the connected World-constructor lifetime are separate from
the original captures and public memory projection below. They do not qualify
complete terrain preparation or original client entry. Run their native targets
separately:

```sh
cmake --build build --target mc_test_source_saved_data mc_test_source_saved_data_close
ctest --test-dir build --output-on-failure -R '^world[.]saved_data_source_(ordinary|close)$'
```

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
