# Original Item construction observations

Both untouched [pinned originals](binaries.md) were observed at Item initializer
PC2664, immediately before the statistics active use. Their measured stacks
retain the outer Block initializer at PC3398. The experiment explicitly enters
Block through one isolated original-JAR loader; it does not establish ordinary
application startup order or the separate Item-first path.

[The helper](../../tests/reference/item/ItemRegistryPrefixObservation.java)
checks the original JAR's size and SHA-256 before loading it with the platform
parent. [The controller](../../tests/reference/item/ItemRegistryPrefixController.java)
binds the actual loader, both initializer bytecodes and exact instruction
boundaries, then reads the suspended VM through JDI. It invokes no game methods,
writes no game fields and supplies no replacement classes, resources or World.

## Capture and termination

The public-tool capture contains 40,842 client rows and 40,741 server rows.
It records all 108 Item statics, 106 distinct named Items, eight base fields,
subtype and ancestor fields, tool/armor/dye state and every physical cell of
the actual 32,000-entry Item array. The related 96-Block graph and five distinct
ordinary Random owners remain in the raw data. Declaring-class metadata,
primitive bits, reference identities, UTF-16 contents, JDK state and the measured
stack are retained without replacing them with expected C values.

The public [provenance](../../tests/reference/item/registry-prefix/provenance.json)
binds 104 sanitized input fingerprints, seven compiled helper classes and the
complete artifact ledger. Its actual capture-manifest SHA-256 is
`2d927d98abb6b2e491cb45631d67ec4a7189f2b92cb721c50250537340a8aff2`.

After capture, the controller requests debugger `exit(0)`. Both owned children
exit zero and are reaped, with complete streams, no watchdog firing and
unchanged original/runtime/helper inputs. This is deliberate debugger
termination. Neither initializer completes naturally, and no completed
statistics or registry bootstrap follows from reaching this checkpoint.

The capture uses pinned Windows Temurin 17.0.17 in interpreted mode. The
capture driver binds source files, compiled helper classes, Java tools/modules,
native runtime libraries, originals, streams and process receipts. The larger
array is admitted explicitly with a 100,000-row, 65,536-cell individual-array,
262,144-cell total and 8 MiB TSV bound. An unavailable or partial observation
does not qualify original execution.

## Finite comparison

The [original-only projector](../../scripts/reference/item/project_registry_prefix.py)
and [public native probe](../../tests/item/registry/prefix_probe.c) compare
the eight base fields of all 106 Items, their named references, the actual
Item-array reference and all 32,000 cells, plus encountered name-String
identities and UTF-16 contents. Reference renumbering preserves equality and
inequality. Scalar bits, nulls, array order and String contents are unchanged.

All 33,064 rows per side match on Windows MSVC and Linux GCC/Clang, with Linux
ASan/UBSan and leak checks. Each projected fixture is 622,620 bytes. The native
probe reads actual public views and contains no expected property vectors.
The projector rejects three specific corruptions per side: missing final LF,
a duplicate instance-field row, and a missing physical array cell. Rebound
framing counters ensure the latter two reach their intended structural guards.
The first export attempt rejected the actual VM's signed synthetic-field
modifier values; the corrected parser admits canonical signed 32-bit modifiers
and preserves their full bits. Original snapshots and projected values were
unchanged by that parser correction.

This projection excludes subtype/class metadata, JDK internals, clock-dependent
Random/profile values, VM/thread/loader state and initialization phase.
The original remains suspended INITIALIZING; the incomplete C entry has stopped
FAILED at its explicitly unfinished dependency. Those phases are not equated.
Native subtype, entry-order and retained-failure checks are separate controls.
See the [native contract](../item/registry.md) for remaining implementation work.

## Reproduction

Public replay requires neither Java nor original files. Local recapture requires
both verified originals and the pinned Windows JDK, using fresh output paths:

```powershell
python scripts/reference/item/capture_registry_prefix.py --java "$jdk/bin/java.exe" --javac "$jdk/bin/javac.exe" --client b1.7.3.jar --server b1.7.3_server.jar --output-dir .local/fresh-item-capture
python scripts/reference/item/project_registry_prefix.py --capture-dir .local/fresh-item-capture --output-dir .local/fresh-item-projection
```

Keep complete raw snapshots and lifecycle receipts with any exported fixture.
The earlier [Block-only observation](block-registry.md) retains its separate
checkpoint, capture tools, provenance and fixture unchanged.
