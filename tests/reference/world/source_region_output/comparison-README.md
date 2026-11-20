# Compare source region output

Build `mc_test_source_region_output`, then run its native controller. The controller creates new owned case roots and reports its retained directory:

```text
python -B tests/tools/storage/test_source_region_output.py --executable <native-test-path> --source-root . --work-dir .local/source-region-output-native
```

Run the original-only capture with the matching recorded host and a full JDK17. Keep the reported retained directory. The default-OFF reference CTest option also runs this exact capture recipe.

```text
python -B tests/reference/world/source_region_output/capture.py --source-root . --client-jar b1.7.3.jar --jdk-home <full-JDK17-home> --work-dir .local/source-region-output-reference
```

Feed its receipt and the native controller report to the standard-library comparator. It launches no original or native process; it reads their actual retained snapshots again and refuses to overwrite an existing output report:

```text
python -B tests/reference/world/source_region_output/compare_native.py --reference-receipt <original-retained-dir>/receipt.json --native-report <native-retained-dir>/REPORT.json --output .local/source-region-output-comparison.json
```

Every live selector2 payload in the mapped snapshots is independently zlib/Adler decoded; each ByteArray byte and typed NBT framing is regenerated. Six normal cases compare exact RAM offsets, disk extents, sector allocation/free masks, local coordinates, primitive count, outer close, late growth and zero reset. Actual int timestamps remain within their own process intervals; unrelated own-process timestamps are preserved. Compressed lengths/hashes remain recorded, without forced cross-zlib byte identity or whole-file equality.

The shared cache-flush comparison checks same old QJ/RAF identity versus a distinct replacement, actual old source close, zero old cache entries, unchanged failed-prefix files/RAM and uncommitted replacement. The original catches/prints its old closed-RAF IOException and returns; native ends at qj441, rewrite74 and lower seek33. That original return does not qualify a native caught return or any later replacement commit/growth. Native extra readback/ownership/cleanup controls and original per-operation JDI traces remain separate evidence. No source Chunk, World, Entity, TileEntity or Info-size caller is exercised here.
