# Original Block construction observations

The untouched client and server were observed at Block initializer PC3398,
after all 96 constructor/fluent/named-publication chains and before the Item
active use. The original files are the [pinned Beta 1.7.3 inputs](binaries.md).
This experiment explicitly starts Block initialization in an isolated loader;
it does not establish ordinary client/server startup order.

## Capture and termination

[The helper](../../tests/reference/block/BlockRegistryPrefixObservation.java)
checks the original JAR and loads only that original through a URLClassLoader
with the platform parent. [The controller](../../tests/reference/block/BlockRegistryPrefixController.java)
binds the actual loader and `<clinit>()V` bytecode boundary, then reads the
suspended VM through JDI. It does not invoke game methods, write game fields,
replace classes or construct a substitute World.

The successful public-tool capture has 6,771 raw client rows and 6,681 server
rows. It includes declaring-class fields, static cells, concrete instances,
arrays, UTF-16 strings, private Random/AtomicLong state, selected class state,
loader identities and the measured stack. Raw primitive bits and reference
identities remain intact in the private capture.

After the snapshot, the controller requests debugger `exit(0)`. Both owned
children exited zero and were reaped, with complete streams, no watchdog firing
and unchanged original/runtime/helper inputs. This is deliberate debugger
termination. Neither original Block initializer completed naturally, and no
successful full bootstrap is inferred.

The capture uses the pinned Windows Temurin 17.0.17 runtime in interpreted mode.
Java executables, modules, native libraries, original inputs, helper sources and
compiled helper classes have retained fingerprints. The public
[provenance](../../tests/reference/block/registry-prefix/provenance.json) records
the input and output hashes without host paths. The capture-manifest SHA-256 is
`0b66ae92d43eb8be70a24db96aff991f7e8ec89c72eb6e40d86e020c5f9e1de9`.

## Finite native comparison

[The original-only projector](../../scripts/reference/block/project_registry_prefix.py)
exports 3,950 rows per side. It includes:

- All 113 base static reference aliases and 28 Material reference identities.
- Every one of the 17 base fields of all 96 Blocks, including all six bounds.
- The three fields of each of the nine sounds.
- All 2,048 cells of the eight property arrays, including null and zero tails.
- Every encountered String identity, length and UTF-16 code unit: 100 strings.

References are renumbered in a fixed first-encounter order, preserving equality
and inequality among the 241 included identities. Scalar bits, field values,
array order and string content are not normalized. Raw subtype fields, JDK
internals, clock-dependent seeds, global uniquifier values, VM/thread/loader
state and initialization phase are excluded from this finite projection. In
particular, the original is suspended INITIALIZING while the incomplete native
entry has stopped FAILED at its unfinished dependency.

[The native probe](../../tests/block/registry/prefix_probe.c) emits actual public
views; it has no expected property vectors or injected state. Exact comparison
passes all 3,950 rows per side on MSVC, GCC and Clang, alongside the three native
registry/Material/profile cases. Linux uses ASan/UBSan and leak checks. The
projector also rejects a missing final LF, a duplicated instance-field row and a
missing physical array cell, with framing counters rebound so each structural
guard reaches its intended check.

This result qualifies the recorded base-field and identity projection. Native
subtype/failure tests are separate controls. The complete stat/resource/recipe tail,
full bootstrap, gameplay methods, World integration, population and persistence
remain unfinished; see the [native contract](../block/registry.md).

## Reproduction

Public CI replays the recorded facts without original files or Java:

```text
ctest --test-dir build --output-on-failure -R "^block[.]registry_(construction|base_observation)$"
```

To recapture locally, supply both pinned originals and the pinned Windows JDK:

```powershell
python scripts/reference/block/capture_registry_prefix.py --java "$jdk/bin/java.exe" --javac "$jdk/bin/javac.exe" --client b1.7.3.jar --server b1.7.3_server.jar --output-dir .local/fresh-registry-capture
python scripts/reference/block/project_registry_prefix.py --capture-dir .local/fresh-registry-capture --output-dir .local/fresh-registry-projection
```

Both output directories must be fresh and their parents must exist. Missing or
different inputs are unavailable reference results. Keep the complete raw
snapshots, streams and process receipts when checking a recapture; a partial
snapshot or an expected fixture alone does not qualify original execution.
