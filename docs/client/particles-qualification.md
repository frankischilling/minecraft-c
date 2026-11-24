# Pickup particle and CPU manager qualification

The authored particle test calls the real client `em` constructor, its `xw`/`sn`
ancestors, `em.w_()V` and `em.c_()I`. Its manager cases call the real `dn(fd,ji)`,
`dn.a(xw)`, `dn.a()V` and `dn.a(fd)` bodies. The reference JAR stays unchanged.
The two actor arguments are genuinely constructed `ju(null)` FallingSand
entities. There is no replacement Entity, Player, World, callback or GL context.

`ParticleInputs` observes actual JDK17 `Random.seedUniquifier()` and
`System.nanoTime()` return values through read-only JDI. It checks the complete
64-bit uniquifier progression from the actual fresh-JVM initial value. Those
constructor inputs enter the existing private native observation binding. The
native Random and lazy shared Math owners execute their real construction and
draw algorithms; observed draws, seeds and field results are never supplied to
them. The target runs with `-Xint`, with no class rewriting or field construction.

The finite comparison contains 71 ordered records. It covers eight constructor
inputs including finite values, signed zero, subnormals, maximum finite values,
infinities, distinct quiet NaN payloads and signaling NaNs. It compares raw current
position and velocity bits, size/offset, all six AABB components, all ten `xw`
fields and all five `em` fields, actual actor aliases, ID, World-null state,
ordinary/shared Random states, full uniquifier and subsequent draws. The native
checks also validate the genuine base watcher, completed constructor PCs and
balanced per-actor holds. It does not claim a new independent comparison of every
unchanged `sn` field.

Four ticks per particle prove that the third increment alone reaches inherited
`sn.K`; the fourth tick still executes. A separate authored actual-field input
checks `INT_MAX` to `INT_MIN` wrap and equality. Layer is three. Cold-Math and
warmed NULL-picked constructions fail before ID or entropy consumption. Native
controls retain RAW on RAW/foreign/particle actor admission failures, reject
particle arguments in either actor position, preserve each picked/target hold
failure prefix, and balance both holds when the two actors are identical.

The manager is constructed before the first particle, including its actual
ordinary Random. The comparison checks four distinct list identities, empty
capacities, normal add and three-tick removal, capacity growth and clear. The
cap case adds one distinct particle followed by 4,000 occurrences of another.
It proves remove-first identity, per-occurrence holds and source tick order:
the first duplicate pass leaves two occurrences after advancing the shared
particle 4,000 times; the next pass removes both. World change to NULL preserves
list identities and allocated capacities.

There are ten successful particle constructions in total. JDI observes 48
`nextFloat` calls (four per constructor plus eight continuation draws), 58
`nextDouble` calls (five per constructor plus eight continuations), and one
manager `nextLong`. Fourteen default Random constructions comprise two actors,
one manager, ten particles and the first shared Math owner. The failed NULL
constructors consume none.

The reference driver requires matching same-bin JDK17 Java/javac, pins the client
JAR and four Java support archives, bounds compilation/version/native/observer
captures, requires exit zero and empty stderr, and checks positive final markers
before comparing every record. Each invocation creates a fresh child beneath
`--work-dir`, so CTest retries are valid. Reports retain compiled helper class and
method Code identities, actual JDK class/Code identities, capture reports and the
observed constructor input trace. The driver uses the existing reviewed stats
capture utility; it does not add a second subprocess implementation.

Run the native route without any JAR or Java dependency:

```text
python tests/reference/client/particles/run.py --native PATH_TO_TEST --work-dir OUTPUT_PARENT
```

Add `--client-jar`, `--library-dir` and optionally the exact `--java` executable to
run the original comparison. Run `test_driver.py` normally and under `python -O`
for nine controller checks, including real subprocess failures, complete record
order, cleared ambient Java options and UTF-8 runtime origins. Original class and Code
pins are in the adjacent `source-pins.json`; it contains metadata only.

The optional JDK-only cleanup test starts an authored suspended child and checks
that a setup failure retires it. Process identity includes its PID, start time
and unique classpath. This regression needs neither the original JAR nor a
native test executable.

Qualification used the complete frozen 1,617-file runtime b cohort plus the
pinned eight particle and three manager files, rebuilt coherently. It predates
the later water ABI and App/notification extension. MSVC Debug used the default
1 MiB PE stack; Clang and GCC Debug used ASan, UBSan and leak detection. Each
profile passed 4,592 checks and 71 exact paired reference records, and 4,571
checks through the native-only route. Each reference run replays its own observed
clock inputs; entropy values need not agree across separate processes. These
finite NULL-World CPU observations
do not qualify nonnull World binding, actual `ji`, drawing, App startup or the
pickup-notification integration. Those are separate owners and tests.
