# Biome bootstrap and shared-reference evidence

The native [shared Biome module](../world/biome-registry.md) follows paired
client `kd` and server `gs`, their seven immediate subclasses and the `bj`/`aq`
spawn record. The inputs are the [fingerprinted originals](binaries.md).
These classes own the shared catalogue; `xv`/`ph` are per-World climate managers.
The [numerical reference](biome.md) describes the separate explicit-seed grid
fixture and arithmetic.

The offsets below are bytecode positions within the corresponding paired
methods.

## Constructors and publications

The base has ten instance fields: String `n`, ints `o/r`, bytes `p/q`, Lists
`s/t/u` and booleans `v/w`. Its constructor reads actual Block statics at 5/16,
stores `p/q` at 12/23, stores `r=5169201` at 29, constructs the three lists at
33..62, sets `w` at 67 and appends ten newly constructed spawn records. Each
record stores its supplied Class identity and weight without constructing an
entity. No Biome constructor creates a Random, tree feature or World.

| Base list | Ordered client/server Class pairs and weights |
| --- | --- |
| `s` | `cn/bn`, `uz/ne`, `fr/dv`, `gb/ec`, `uw/nb`, each 10 |
| `t` | `dl/ce` 12, `wh/oc` 10, `ww/or` 10, `bx/bd` 8 |
| `u` | `xt/pf` 10 |

| Client / server class | Constructor effect after the base |
| --- | --- |
| `kd / gs` | Base only |
| `fs / dw`, `tf / ma`, `yj / pp` | Return without further field/list changes |
| `rb / kk`, `g / f` | Append a distinct `gi / eh` record of weight 2 to `t` |
| `t / m` | Clear `s/t/u`, then append `bp / aw` and `ya / pi`, weight 10, to `s` |
| `ry / le` | Clear `s/t/u`, then append a new `ww / or`, weight 10, to `t` |

The initializer constructs thirteen distinct objects in this order. Fluent
methods return their receiver; named publication occurs only at the final
store. Unchanged `r` values remain 5169201.

| Cell / name | Class pair | `o` | `r` | `v`, `w` | Store |
| --- | --- | ---: | ---: | --- | ---: |
| `a` Rainforest | `yj/pp` | 588342 | 2094168 | false, true | 22 |
| `b` Swampland | `tf/ma` | 522674 | 9154376 | false, true | 47 |
| `c` Seasonal Forest | `kd/gs` | 10215459 | 5169201 | false, true | 67 |
| `d` Forest | `rb/kk` | 353825 | 5159473 | false, true | 92 |
| `e` Savanna | `fs/dw` | 14278691 | 5169201 | false, true | 112 |
| `f` Shrubland | `kd/gs` | 10595616 | 5169201 | false, true | 132 |
| `g` Taiga | `g/f` | 3060051 | 8107825 | true, true | 160 |
| `h` Desert | `fs/dw` | 16421912 | 5169201 | false, false | 183 |
| `i` Plains | `fs/dw` | 16767248 | 5169201 | false, true | 203 |
| `j` Ice Desert | `fs/dw` | 16772499 | 12899129 | true, false | 234 |
| `k` Tundra | `kd/gs` | 5762041 | 12899129 | true, true | 262 |
| `l` Hell | `t/m` | 16711680 | 5169201 | false, false | 285 |
| `m` Sky | `ry/le` | 8421631 | 5169201 | false, false | 308 |

The source allocates 39 Lists and 135 records, clearing 20 records from the live
Hell/Sky lists. The resulting graph has 115 distinct live records and 13 shared
entity Class identities. The 4096-reference table is allocated at 314 and
published at 317 before its builder at 320. After all entries are filled,
Desert receives sand `q65` then `p68`, followed by Ice Desert `q85` then `p88`.
The source does not replace every object of their class with sand fields.

The float selector first multiplies rainfall by temperature in binary32.
Its comparisons retain Java NaN behavior. The double method uses two Java
`d2i` conversions and wrapping int indexing before `aaload27`, with no explicit
finite-input clamp. A native NULL/index status is mapped to the corresponding
observed exception category for comparison; C does not throw a Java exception.

The original direct `xv/ph` sampler reaches this owner after output allocation19
and channel calls49/81/113. It stores processed `a294/b303`, increments its local
index307, invokes the real shared double lookup314 and only then stores the
reference317. The native typed sampler preserves that partial-effect seam.
Its source contract is distinct from the existing numerical API's preflight
failure preservation. Bootstrap observations alone do not establish an
end-to-end original World/provider/surface sampling comparison.

## Manager source and native contracts

The concrete World/WorldProvider binding and cached methods follow these paired
method bodies. These source anchors supplement the bootstrap observations;
they do not extend that observer into a running World comparison.

| Client / server method | Source operations |
| --- | --- |
| `xv/ph` World constructor | Three separate seed reads14/41/68, noise publications28/55/82; no Biome active use or stored World field |
| `xa/os.a()V` normal setup | Allocate manager1, capture WorldProvider.a6, constructor9, publish b12 |
| `wd/nz.a()V`, `ay/aj.a()V` fixed setup | Allocate manager1, actual Biome l/m active use5, constructor10/12, b13/15, then subtype flags/ID |
| `hd/et` fixed constructor | Actual nullable e6, f11 and g17 after empty superclass construction |
| `xv/ph` and `hd/et` cached wrapper | Capture d3, virtual direct11, publish d14, reload18, return21 |
| `hd/et` direct | Output allocation19; conditional fresh a49/51 then b60/62; e-fill76, b.g-fill93, a.f-fill110, return114 |
| `xv/ph` point/chunk | Cached1x1 call5, element0 read9; chunk coordinates use wrapping shifts before point13 |
| `hd/et` point/chunk | Return current e4 without direct sampling or cache publication |

The [normal-manager contracts](../../tests/world/dimension/test_biome_runtime.c)
exercise actual constructed Worlds, manager calls, callback replacement, retained
aliases and class failures. The
[fixed-manager contracts](../../tests/world/dimension/test_fixed_biome_runtime.c)
cover both sides and fixed dimensions. They use public allocation accounting to
reach a successful a publication followed by b allocation failure, then the
subsequent uncorrected null/short-b fill failure after reference writes.
No class phase, opaque layout or failure outcome is fabricated for these tests.
Their lookup consistency checks are native integration checks; the existing
original-derived numerical fixtures remain the climate oracle. Java OOM/GC
timing, nullable fixed e reached through recursive original class flow, and
complete original World startup are not newly observed here.

## Actual observations and native comparison

The [project-owned observer](../../tests/reference/world/BiomeRegistryObservation.java)
loads each pinned original in an isolated loader and requires ordinary Biome
class initialization to return before reflection. It reads the actual objects,
fields, lists, records, Class identities and every table slot. Explicit later
controls mutate actual `p/q` or table cells and then invoke original helpers;
these controls are declared separately from the baseline.

The successful Windows Temurin 17.0.17+10 capture ran both sides under `-Xint`,
`-XX:TieredStopAtLevel=1` and default flags. All nine owned children—two version
queries, one compiler and six helper JVMs—exited naturally with zero status,
were reaped and hit no timeout/output limit. Input and compiled-helper pins
matched before and after. The three modes produced identical protocol bytes
per side; this is not proof that a particular method was selected for JIT
compilation.

Each side's unchanged fixture contains 30,356 rows: six graph snapshots,
3,836 declared operations and 3,824 raw-bit selector queries. Every snapshot
includes all 4096 table slots. Its 194 logical references describe the live
reachable graph, including names and the table; they are not JVM addresses or
a heap census. Cleared records and transitive Block objects are outside that
graph count.

The public [client](../../tests/reference/world/biome_registry_client.tsv) and
[server](../../tests/reference/world/biome_registry_server.tsv) files are exact
copies of observed output. The
[requests](../../tests/reference/world/biome_registry_requests.tsv) declare
inputs independently; the
[sanitized provenance](../../tests/reference/world/biome_registry_provenance.json)
binds originals, tools, helper, streams and child outcomes without distributing
original JARs, classfiles or resource payloads. No native executable computes
the expected results.

The native probe traverses actual production owners and uses pointer equality
to assign graph-local identities. The checker changes only the exact native
producer-policy token to its original counterpart before full stream equality.
It does not normalize values, aliases, fields, row order or line endings.
Ten deliberate corruptions per side, 20 total, must be rejected, covering
object/table aliases, a signed byte, weight, Class, dropped slot, selector
result, exception category, footer and false producer attribution.

All 26 selected native cases pass on MSVC, GCC and Clang, including 101,396
registry, 609 reference-sampling and 33,155 numerical checks. Linux runs use
ASan/UBSan and leak detection. Both complete original graph/query streams
match under synthetic and imported resources, with all 20 corruption controls
rejected. All 575 code/tool/fixture inputs match retained source copies and
26 products per host. The [status note](../status.md) records the remaining
World integration scope.

## Reproduction and scope

After a normal test build, the fixture replay needs Python and the native
probe, with no original JAR or JVM:

```text
ctest --test-dir <build> --no-tests=error -V -R "^world[.](biome_registry(_observation)?|biome_reference_sampling)$"
```

For a standalone replay, use a fresh work directory:

```text
python scripts/reference/world/check_biome_registry.py --executable <build>/mc_biome_registry_probe --work-dir <new-private-directory>
```

The [capture tool](../../scripts/reference/world/capture_biome_registry.py)
requires the pinned originals, one supplied modular JDK able to compile/run
the Java 17 helper, the declared requests and a fresh destination:

```text
python scripts/reference/world/capture_biome_registry.py --java <jdk>/bin/java --javac <jdk>/bin/javac --client-jar <client.jar> --server-jar <server.jar> --requests tests/reference/world/biome_registry_requests.tsv --output-dir <new-private-directory>
```

The [exporter](../../scripts/reference/world/export_biome_registry.py) accepts
only a successful complete capture and fresh fixture destinations; it copies
observations unchanged. The
[checker](../../scripts/reference/world/check_biome_registry.py) verifies fixture
pins, schema, bounded native children, retained outputs and unchanged inputs.
It preserves real initialization console output separately from the protocol.

These finite observations cover ordinary shared bootstrap, the live graph,
selector inputs and declared mutations. They do not qualify full game or World
startup, arbitrary class-loader failures, Java heap/GC/OOM timing, Entity
construction, spawning, feature placement, generation, surface application or
persistence. Those remain separate source and integration obligations.
