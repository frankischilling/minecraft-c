# JDK17 object-sort regression

This is an authored algorithm fixture for the concrete N near-list sorting
defect: a natural-run plus whole-array binary insertion sort changes ordering
once JDK17 TimSort begins merging arrays of at least 32 elements. MD's ordinal
tie branch returns -1 even when both ordinals are equal, making the selected
JDK algorithm's comparator call order observable.

`SortKernelObservation.java` invokes the real bootstrap `Arrays.sort` with
declared integer comparators. It contains no implementation of TimSort, game
JAR, original assets, copied JDK source or native rendering. Its comparator
modes are an MD-like descending key/visibility/ordinal relation, ordinary
stable integer ordering, a cyclic relation and a nonantisymmetric descending
relation. The first mode uses normalized integer distance keys; it does not
invoke or claim parity with original MD, DK distance arithmetic or N itself.
Those are qualified separately by the `render_global_update` original-JAR
fixture.

The 108 finite controls include lengths 2, 3, 8, 31, 32, 33, 64, 65, 85 and
128, plus authored 257/512/1024-element runs exercising galloping and temporary
growth. Repeated ordinal and mixed-run cohorts use declared prepared inputs
from the separate N fixture, with keys normalized and array-slot identities
reassigned. The size85 cohort comes from its discovered comparator-contract
case. Stops at comparison 1, 7, 31, 127 and 189 retain the exact interrupted
array, including duplicate/missing identities that an incomplete merge may
leave. A stop beyond the completed comparison count naturally does not fire.
There are 40 successes, 65 callback interruptions and 3 contract failures.

`cases.txt` stores a header `count mode stop`, followed by `count` triples of
`key ordinal visible`. `cases.json` records case names in the same order.
Each `expected.txt` row contains status (0 success, 1 authored stop, 2 JDK
contract exception), comparison count, FNV64 trace digest, all output array
identities, then **every comparator triple** `(left identity, right identity,
result)`. A result of91 records the authored stop. The full trace and state,
rather than the digest alone, are compared. Identities are zero-based initial
array slots. The entire files also have SHA256 pins.

`runtime-pins.json` records the observed JDK17 runtime profile and SHA256 of
the bootstrap Arrays and TimSort class bytes, plus the exception throw PCs.
The capture rejects another runtime/class profile instead of silently adopting
another algorithm. Java tools, inputs and helper classes are fingerprinted
before and after bounded subprocess execution. No JDK source is distributed.

```powershell
python -B tests/reference/client/render_global_update_sort/capture_reference.py --output .local/update-sort-reference-new
python -B tests/reference/client/render_global_update_sort/capture_reference.py --native path/to/mc_test_render_global_update_sort.exe --output .local/update-sort-differential-new
```

The native executable uses prepared RAW instances of the canonical DK struct
as sortable identities and authored comparator inputs only. It links the
production sort and diagnostic helper. It neither constructs source owners
nor substitutes for actual source MD/World/renderer calls. Its retained error
is also the callback's error slot, checking that callback failure survives
temporary cleanup. Detected JDK contracts must report the typed source
IllegalArgumentException at N411 and TimSort mergeLo607 or mergeHi675, with
native_policy false. Native allocation/admission failures remain separate;
generic JVM allocator/OOME behavior is outside this fixture.

```text
mc_test_render_global_update_sort --check cases.txt expected.txt
mc_test_render_global_update_sort --observe cases.txt
```
