# Comparing Mob NBT

The original capture and native World fixture exercise different callers. The original recipe constructs the eleven registered client Mob classes with actual NULLWorld and observes their real readers, writers and Boolean storage decision. Ordinary and independent read-only JDI VMs each emit 62 raw NBT files. The native fixture loads authored records through a real source World, its Registry and canonical Entity classes, then saves and reloads that World twice.

The shared comparison covers 61 cases: eleven constructor own-field writers, three authored own/full/Boolean writes for each of the eleven Mobs, eleven missing-field own writers, removed-Pig Boolean and direct writers, three Slime size cases and a Creeper watcher-Byte-2 writer. The native fixture adds 22 own-field readbacks from newly constructed Worlds, making 83 snapshots. Its false Pig result leaves the independent output absent, so it emits 82 binary NBT files.

The original's remaining case passes a preexisting Compound containing `powered` to a Creeper writer whose watcher is 2. The source writer leaves that key in place. The native API returns a fresh independent tree and accepts no preexisting Compound, so this case remains original-only.

## Run the comparison

Run the original [capture recipe](README.md) with a verified client JAR and full JDK 17. Run the registered native `entity.client_mob_nbt` test to obtain its retained `REPORT.json`. Use the matching host's original receipt and the current public comparator:

```text
python -B tests/reference/entity/mob_nbt/compare_native.py --source-root . --native-report /path/to/native/REPORT.json --original-receipt /path/to/original/receipt.json --output /path/to/new-comparison.json
```

Both receipts must report completed, unchanged inputs. The comparator verifies captured artifact hashes, independently decodes the actual native and original NBT files, and rechecks the source World images. It reads bounded files and checks declared lengths, NBT depth, tag counts, modified UTF8, region spans and complete compressed members. Comparison output uses a new path in a disposable evidence directory. The qualified helper rejects an existing result or input alias and creates its output exclusively; retain its complete stdout, stderr and process status.

Three differences have explicit treatment. The native authored records use Pos.z=+2.75 so Chunk 0,0 can adopt them without entering the unimplemented wrong-location diagnostic. The original prepared records use -2.75. The comparator requires an exact three-Double Pos list with that declared Z, preserves its actual X/Y, and changes only the third value to +2.75. Constructor Slime size and Health come from independent entropy and must satisfy their relation within each receiver. Fresh removed-Pig direct output retains constructor Rotation, which is excluded only from cross-process equality. The original marker `Keep` checks that its mutable writer preserves unrelated keys; it is removed before comparing with native fresh-tree output.

All other compared typed fields and Boolean results must agree. Wolf Owner remains a sequence of UTF16 units, including NUL and a surrogate pair. Same-run saved and reopened files must be byte identical. Source timestamps must lie inside the save's actual clock interval. HashMap entry order and compression bytes are not compared across runs.

The native fixture advances real Info.Time from 20000 to 20001 between selected saves. The source Chunk's time test still applies during a forced save. The controller checks the reached World time, last-save time, Chunk LastUpdate and metadata Time for each phase; it does not assign dirty or ownership flags to make a skipped save appear complete.

## Reading the evidence

The original [provenance](provenance.json), Windows/Linux results and passive trace audits describe executed source observations. They are independent of later native builds. Native report and comparison metadata identify each tested executable, helper, source input and retained artifact. A passing Mob comparison does not turn a failed broader regression cohort into a passing one.

The final [E provenance](comparison-provenance-e.json) records rebuilt Windows and Linux fixtures, 33 passing headless CTests per host, and 61 matching common cases per host using the unchanged public comparator and original captures. Its [Windows](windows-comparison-e.json) and [Linux](linux-comparison-e.json) records include the selected native products and the later session-input retirement fix. The earlier [D provenance](comparison-provenance.json) and its 29-test qualification remain unchanged as separate history.

Native source World factory/import and persistence are authored integration evidence. The original NULLWorld factory probe does not qualify native cold-Skeleton NULLWorld admission. These comparisons do not establish natural spawning, complete original World caller equivalence or server/Nether hooks. The [Windows Tiny save/title route](../../../../docs/client/world-saving.md) has separate visible input, disk-output and cleanup evidence. The separately executed Wolf ownership/limit profile checks equal-owner identity at budget zero, exact overlapping replacement limits, retained reader prefixes and external String holds through Entity/World/class retirement. It adds no original Mob writer cases.

The comparator tooling has separate executed corruption controls on both hosts. Twelve controls cover successful input, existing-output/input-alias preservation, artifact hashes, truncated/oversized NBT, modified UTF8, zlib checksum, typed field mismatch, List count overflow and undeclared Pos X/Z changes. Two retained prior-helper controls reproduce its incorrect acceptance of self-consistent X/Z mutations before the narrowed exception. These are real Python runs over deliberately changed retained copies. Their adjusted integrity envelopes let the corrupt bytes reach decoding or semantic comparison; they are not newly executed game captures. All actual original and World capture products remain unchanged.
