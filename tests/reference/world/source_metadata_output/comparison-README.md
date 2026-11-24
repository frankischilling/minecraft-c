# Reproduce the metadata transport comparison

First build `mc_test_source_metadata_output`, then run the native controller on a fresh output directory:

```text
python -B tests/tools/storage/test_source_metadata_output.py --executable <native-test-path> --source-root . --work-dir .local/metadata-native
```

Run the original capture with a full JDK17 and the verified local JARs. Its output identifies the fresh retained directory; the source metadata reference option also provides this original-only capture through CTest when explicitly enabled.

```text
python -B tests/reference/world/source_metadata_output/capture.py --source-root . --client-jar b1.7.3.jar --server-jar b1.7.3_server.jar --jdk-home <JDK17-home> --work-dir .local/metadata-original
```

Feed the two reported receipts to the standalone standard-library comparator. It performs no Java or native execution itself and refuses to overwrite an existing output report.

```text
python -B tests/reference/world/source_metadata_output/compare_native.py --reference-receipt <original-retained-dir>/receipt.json --native-report <native-retained-dir>/REPORT.json --output .local/metadata-comparison.json
```

All actual retained original/native snapshots are independently decoded again. The complete typed Data-only tree must match except for LastPlayed, whose Long kind and actual process interval are checked independently on each side. Only that sampled clock field is removed for the cross-process value comparison. Source File flags/results, resulting file paths, and actual obstruction sentinels must match. Each original/native cohort separately requires its backup to equal the immediately preceding current file byte for byte. Raw gzip files and compound order remain retained; they are not equated across implementations.

The recorded Windows comparison uses Temurin17.0.17+10; the Linux WSL2 comparison uses Ubuntu17.0.20.1+1. Both profiles use the same original source JAR and identical compiled observation helpers. The Linux original case roots were on `/mnt/c`; arbitrary filesystems gain no credit from that cohort.

The native entry begins after an actual completed Info record, using an explicitly selected null Player. The original helper calls the actual mx List entry with empty or null-first Lists and observes Player omission. Matching output values do not qualify a native List caller or LocalPlayer serializer. The native existing-first/second cases use separate process Handlers; its separate repeat control checks one Handler. Native File call flags are listed in the reviewed source order; original order comes from direct caller JDI exits, without an invented native per-primitive trace.

The original genuine FOS-open failure catches, prints, and returns150. Native C keeps the real FOS cause at the explicit fm147 printStackTrace dependency. The comparison credits only the common pre-rotation prefix and effects for that case. It does not turn the native failure into a successful caught return. Chunk output, full World save/reload, direct JDK codec payloads and the authored as fault supplement remain separate scopes.
