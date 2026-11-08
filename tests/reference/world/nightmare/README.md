# Complete original nightmare calls

This packet observes the unchanged client `cq.a(fd, List)Z` body, including real Mob construction, eligibility, repeated direct-World `fw` searches, Bed exits, entity publication, shared postspawn handling, Player wake and ambient sound. It contains no original class bodies or assets.

The 28 declared profiles cover empty and null inputs, terrain rejection, bright rejection, a blocked Bed, all four Bed directions, supported Torch exit blockers, two Players, changed live Lists, an authored publication exception, Zombie-only and Spider-only class-array elements, and a successful Spider/Skeleton jockey. Scene 8 contains Stone at `(6,64,8)`, `(7,64,9)` and `(7,65,8)`.

Both ordinary VMs and both read-only JDI VMs execute each entered original call through its actual return or first uncaught exception. JDI neither invokes game methods nor changes target fields, results, Random seeds or clocks. Preparation uses genuine original World, Chunk and Player constructors and successful original sleep entry. Only declared inputs are prepared before the call. The default final three-element class-array reference is preserved; profiles modify its elements.

Each VM records 23 returns (13 true and 10 false), five uncaught exceptions, 180 attempts and 77 direct-World searches. The portable controls pass normally and with `-O`: 410 constructor arithmetic checks and 1,198 corruption refusals. Native comparison covers 39,908 selected scalars; its completed-call scope is 21 returns plus five source-exception outcomes, with the two caught-printer cases kept as prefixes.

`expected.json` contains selected before/after state, raw floating-point words, all 81 normal Chunk plane hashes, exact World/Math Random state, List membership, retained Player-coordinate identities, publication and sound ordering, private constructor-entropy evidence and each completed reusable-finder search. Larger raw field and backing-array journals remain in the ignored capture attempt named by provenance.

New Entity Random instances retain their actual independent constructor entropy. Their seeds are not compared across processes. The portable checks verify the observed original `seedUniquifier ^ nanoTime` construction and Math draws. The comparator checks unchanged Player streams and derives every emitted ambient pitch from that run's actual final Mob stream.

The original caught reflection exceptions at cases 6 and 8 print a real stack trace and immediately return false at CQ366; they do not run more attempts. Native replay currently stops at the actual printer frontier at CQ362. Their recorded original after-return state is compared with the native mutation prefix because printing is followed only by loading and returning the prior boolean. The packet does not claim a separately captured original World snapshot at CQ362 or a native whole return for those two cases. Native policy failures never qualify as original false returns.

Run the portable packet checks without Java or game assets:

```powershell
python -B tests/reference/world/nightmare/test_packet.py
python -O -B tests/reference/world/nightmare/test_packet.py
```

Capture a new original cohort into a new ignored directory, then publish its selected observations:

```powershell
python -B tests/reference/world/nightmare/capture_reference.py --client-jar b1.7.3.jar --library-root "$env:APPDATA/.minecraft/libraries" --output .local/nightmare-new-attempt
python -B tests/reference/world/nightmare/export_reference.py --capture .local/nightmare-new-attempt
```

The producer checks the original JAR, SDK libraries, class origins and class/Code identities; retains bounded stdout/stderr and completion receipts; and rejects changed inputs or generated helpers. Every failed or precursor attempt remains separate. The earlier two-stone scene is a retained precursor, not the canonical jockey observation.

Provenance separates the exact captured Java producers and declared original inputs from current postprocessing and reader sources. `capture-receipt.json` preserves the historical capture bytes, including the hashes of Python tools that were present but not executed by the original JVM. Updating postprocessing does not relabel that earlier producer inventory.

Replay the native executable through the genuine nightmare API:

```powershell
python -B tests/reference/world/nightmare/run_native.py --executable build-windows/mc_test_nightmare.exe --work-dir .local/nightmare-comparison
```

This component evidence does not qualify natural sleep timing, complete World ticks, rendered frames, App behavior, elapsed gameplay or performance.

The separately retained [loaded Bed sleep scene](../../../../docs/client/bed-sleep.md) qualifies its own natural-tick, dawn, presentation and save scope.
