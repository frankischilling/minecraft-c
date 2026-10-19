# Dimension constructor, table and angle observations

Both unchanged original archives were observed on Windows Temurin
17.0.17+10 with `-Xint`. The isolated recorder calls the actual factory,
reads eight actual base fields and their array identities, invokes the actual
protected light-table method and observes the actual celestial method.
It computes no expected angle or light value and constructs no fake World.

Archive SHA-256 values remain client
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`
and server `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.
The [public provenance](../../tests/reference/world/dimension/dimension_provenance.json)
pins twelve recorder/JDK/archive inputs, all three compiled helper classes,
the raw transcripts and the complete exported fixtures.

Each original transcript has 48,875 literal-LF rows: eight factory calls,
four nonnull results, eleven state snapshots, three table calls, 48,739
angle calls and 24 identity relations. Normal angles cover all signed day
remainders -23999 through 23999 at partial1, plus 35 selected signed times
times twenty finite partials. The selected cases include signed-long extremes,
negative times, wrap boundaries, signed zero, subnormals and large finite ticks.
Nether/Sky each contribute twenty representative pairs.

The original null-World normal bind records its actual exception, diagnostic
and reached state. That outcome and its after-state remain original-only.
Both fixed-biome binds are explicitly unexecuted because their registry closure
is outside this experiment. No World construction, biome setup, runtime
intrinsic selection or complete global bootstrap is qualified by these rows.

## Native projection

The actual client/server request and projected-result bytes agree completely,
so one shared pair retains both archive bindings. Expected angle bits and
table entries come solely from the measured original rows. The native probe
calls production factory, field, table and angle operations; it retains actual
native identities and compares their bijection with the observed references.

The projection has 48,782 rows per side: eight factory calls, ten snapshots,
three table calls, 48,739 angles and twelve nonnull owner/array identities.
Actual original array lengths are compared with the native API's documented
constructor allocations f[16]/h[4]; native pointers do not expose dynamic
length descriptors. Java class/member names are binding evidence rather than
native class objects. No tolerance, float normalization or angle recomputation
is applied.

`world.dimension_reference` uses only public files and the compiled C probe.
It checks both side bindings, 28 altered verifier inputs/results, and four
actual exit1 native protocol refusals. All compared rows and guards pass on
native Windows MSVC and Linux GCC/Clang within each complete 97-case public
run. Linux uses ASan/UBSan and leak checks. This remains a finite standalone
Dimension comparison, without original World/biome startup qualification.

## Reproduction

Ordinary native replay:

```sh
ctest --test-dir build --output-on-failure -R '^world[.]dimension_reference$'
```

Fresh original capture requires the pinned Windows JDK and supplied archives:

```sh
python tests/tools/world/test_dimension_reference.py capture \
  --java /path/to/java.exe --javac /path/to/javac.exe \
  --client-jar /path/to/b1.7.3.jar --server-jar /path/to/b1.7.3_server.jar \
  --output-dir /new/owned/capture
python tests/tools/world/test_dimension_reference.py export \
  --capture-dir /new/owned/capture --output-dir /new/owned/projection
```

Capture compiles the published observer and runs only the original classes.
Export reads those retained streams without a child process. Complete raw
stdout/stderr, class files, process receipts and null-bind diagnostics stay in
the private capture. The public projection contains hashes, finite observations
and ownership scopes. A fresh export reports its exact provenance hash; native
verification requires that explicit hash. The current CMake value is
`5b620c091d1de9881f7c60b267706c693aa4726a99f67b7c99b63152118f813b`.

The paired source anchors are client `xa`/server `os` for base construction,
binding, table and celestial bodies; client `rh,wd,ay` / server `kp,nz,aj`
for the selected concrete classes. Normal angle uses signed long remainder,
one conditional adjustment on either side of the day boundary, binary64
cosine, and explicitly ordered float narrowing. The existing float lookup
cosine implements the later World light calculation, not this smoothing call.
See [native construction](../world/construction.md) for the remaining provider,
spawn, mutation, persistence and startup work.
