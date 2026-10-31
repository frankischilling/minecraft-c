# Original local statistics comparison

`compare.py` executes the unchanged client `xi`/`cj` constructors, real `vk`
read/refresh and `vj` save workers, normal pending-map service, local file
rotation, static parser/serializer and `hf` checksum. It compares 22 authored
state/codec/checksum rows with the concrete native test executable, plus exact
single-entry JSON bytes. The C tests separately roundtrip every one of the
486 registered statistics and exercise native ownership/failure prefixes.

Run with explicit JDK17 tools and a verified, locally supplied client JAR:

```text
python -B tests/reference/client/statistics/compare.py --client-jar /private/b1.7.3.jar --native /build/mc_test_client_stats_file --java /jdk17/bin/java --javac /jdk17/bin/javac --source-root /source --work-dir /ignored/stats-reference
```

Each invocation creates a fresh work directory. The runner validates the whole
JAR, 30 selected class structures and 227 Code bodies/attributes, all authored
helper bytes/compiled Code, and the selected native sources before execution.
This metadata scope does not imply every selected method is executed. Original
game receivers come only from a JAR-only URLClassLoader whose parent is the
platform loader. The process classpath contains only the one authored helper.
No replacement game class, Unsafe, captured object graph, supplied entropy or
network service is used. Actual origin/bytes for 11 principal classes and all
loaded game-class origins are checked; class initialization logs must exclude
Minecraft, local Player/tick, notice and legacy session-check owners.

The reference profile is JDK17, English/US default Locale, UTF-8 default
charset and immutable `Player123` Session username. Original constructor
workers start normally and are then joined for finite observation. Workers do
not directly service the writer's maps. Refresh exercises the nonnull pending
map branch without clearing its identity. The async case reaches the actual
100-service delay gate and the real save thread.

Three additional direct JDK file-provider observations verify the actual
rename-existing outcome (false on Windows, true on Linux), FileReader's
directory-open rejection and deletion of an empty directory. These are
reported separately from the 22 game projections. Permanent native regressions
cover the directory open error at CJ11 and deletion of a preexisting empty old
directory during real synchronizer rotation.

Stdout captures retain the actual recipe/achievement console banners, including
their different original/native initialization order. Only `splitlines()` is
used for line comparisons. Both processes must exit0 with empty process stderr.
Three intentionally malformed JSON documents reach the original parser's own
caught `xe.printStackTrace`: the observer transports that actual stderr to a
private file, and the runner verifies three real syntax diagnostics and source
frames there. These effects are reported separately; they are not erased or
counted as an absence of source exceptions. Other type/path/number failures
are caught by the authored reflection observer after the original call throws.

Only `lang/en_US.lang`, `lang/stats_US.lang`, and `achievement/map.txt` are
privately extracted for the native comparison, after size/hash validation.
No JAR, resources, original classes, audio/graphics assets, captures or JVM
outputs belong in this public package. The authored public wrapper
`tests/tools/client/test_stats_file.py` needs no JAR and creates its own three
small language/achievement resources. It requires exit0, empty stderr, exact
console/row structure and a positive check count. Its malformed CLI checks
require exit2 and exact diagnostics, so crashes/sanitizer failures cannot count
as successful rejection.

The comparison does not execute a Player tick or full Minecraft startup, install
the achievement notice, query an account, test the V==6000 legacy network
worker, reproduce arbitrary JVM identity-hash iteration, or qualify every
locale/filesystem/charset provider. Multi-key Argo fields use the selected
JDK17 list-bin HashMap traversal; identity-dependent tree bins are an explicit
unsupported native profile. Writer-generated entries each have one key.
Native map traversal is its documented first-insertion order with a matching
checksum, rather than a fabricated captured JVM identity order.

The revised capture transport reuses the reviewed environment-controller
mechanics in the existing stats support helper. Both call sites use read1,
record pipe/artifact open/read/write/short-write/close faults, kill and reap
failed children, and require explicit empty reader-errors plus complete capture.
Capture reports accompany the original stdout/stderr files; evidence reread
lengths are checked. No lost reader exception can become empty stderr success.
Run `python -B tests/tools/client/test_stats_capture.py` and again with `-O`
for 52 controls across both call sites: 42 injected I/O/spawn/report faults,
clean/nonzero children, overflow before a sleeping child exits, and timeout.

On POSIX, the native and genuine xi constructor/save regressions additionally
use a real parent directory whose name ends in one literal backslash. The stats
child remains inside that parent with an inserted slash; no sibling directory
is substituted. Windows keeps its normal slash/backslash separator behavior.
