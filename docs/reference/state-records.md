# Item and scheduled-record evidence

Status: source research, native contract tests, a finite JDK collection replay
and finite original World scheduling observations. The World observations have
no native comparison; complete item/container/world scheduler qualification is
not claimed.
The [binary manifest](manifest.json) identifies the supplied original inputs.

| Side | Original archive SHA-256 | ItemStack | Scheduled record |
| --- | --- | --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` | `iz` | `qy` |
| Server | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` | `fy` | `ki` |

Both sides' complete bytecode was inspected for item construction `(III)V`, NBT
construction/read/write, clone and split, and for scheduled-record construction
`(IIII)V`, equality, hashing and comparison. Numeric overloads retain scalar
item fields. Item/block overloads dereference the supplied object's final ID
before delegating. NBT field reads assign each completed prefix in order. Split
decrements the original count before allocating the new owner.

Scheduled equality uses X/Y/Z/block, while ordering uses time/sequence. A rejected
world scheduling request can already have consumed its construction sequence.
The server time-shift body mutates retained records before assigning world time,
without rebuilding its ordered collection. World insertion now uses the native
record and collection owners; the server time-shift operation remains outside
that insertion API.

The captured Temurin 17.0.17 library profile has modules SHA-256
`2d290b746c6497265cc2498d30e9695166a3fc282f231ebed467ce29c32946a8`.
Its TreeSet/TreeMap bodies confirm that finding the structural first entry and
searching for removal are separate operations. Removal uses current comparison
values; after time mutation it can miss the entry returned by structural-first
lookup. This is profile-specific source evidence, not Java 6 or arbitrary-runtime
qualification. The native ordered owner preserves this recorded search and
structural-history behavior independently of full-key membership.

The native source copier and item provenance overlay are explicit storage policy:
unknown data remains owned independently of caller documents, while clone/split
start without provenance. Container fields remain separate. These operations do
not establish original mutable-compound partial-write behavior or HashMap wire
order.

The integrated GCC 13.3 and Clang 18.1 x86-64 Ubuntu builds use C17, strict
project diagnostics, disabled floating contraction/fast math, and address/undefined-behavior
sanitizers with leak detection enabled. Three native cases pass: `item.stack`
has 93 checks, `world.scheduled_membership` has 329 and
`world.scheduled_ordered` has 940. A native x86-64 Windows
MSVC 19.51 Debug build passes the same three cases with strict project diagnostics
and sanitizers disabled. These finite native API tests supply no original-game
oracle, allocation fault injection,
bound-world callback result or gameplay parity result. Those remain required
before broadening acceptance.

The [ordered fixture](../../tests/reference/java/scheduled_ordered_fixtures.json)
contains 13,584 commands and their complete observed results. The project-owned
[Java helper](../../tests/reference/java/ScheduledOrderedObservation.java) uses
only public `TreeSet` operations and records with the source-proven signed
time/sequence comparison. Two independent executions on the pinned Windows
Temurin profile produced identical output before any native comparison. Input
fingerprints remained unchanged. No native result supplied an expected answer.

The canonical commands/results SHA-256 is
`91796c6ecddec91376486ada468ff5cdebfc8fb5e51ed153963c8a555537bd7f`;
the separately pinned capture provenance SHA-256 is
`c2c518b5442b160e73880f079004d728b277ebd6c5a89b6fbc1c4a3543fe21a4`.
The [replay controller](../../scripts/reference/java/compare_scheduled_ordered.py)
checks both fingerprints and all events. Native GCC, Clang and MSVC replays match
every event. Public replay needs no JDK or original archives. Optional live
observation requires the exact captured Windows module/VM fingerprints.

These scenarios cover comparator ties, distinct and repeated identities,
insertion/deletion histories, arbitrary signed time changes, wrapped shifts and
structural-first/search disagreement. They establish neither exhaustive tree
branch coverage nor world enqueue/drain/reentry behavior. This JDK helper does
not instantiate the game's scheduled-record classes. The original World
observations below are separate from its native collection replay.

The complete paired insertion bodies are client `fd.c(IIIII)V` and base-server
`dj.c(IIIII)V`, both PC 0 through 235. They allocate an actual `qy`/`ki` at PC 0
and call its constructor at PC 9 before reading the immediate flag at PC 19.
The immediate branch checks radius-eight presence at PC 74, reads the actual
block at PC 96, checks full-ID equality and positivity, then reads the live Block
array at PC 116/121 and dispatches the virtual tick at PC 142 with the current
World Random. The deferred branch checks
presence at PC 171. For positive IDs only, it reads current metadata time at
PC 191 and stores wrapped time-plus-delay at PC 195. It then calls `F.contains`
at PC 205, `F.add` at PC 219 and `E.add` at PC 231 on the same record. F is the
membership set and E the ordered set. Each add's Boolean result is discarded;
an E failure retains completed F effects. Absent requests, duplicates and
immediate no-ops can already have consumed the class-shared sequence. These
bodies do not reset it per World or draw Random
directly. The [World scheduling contract](../world/scheduling.md) describes the
native ownership and failure policies.

The project-owned [SchedulingObservation.java](../../tests/reference/world/SchedulingObservation.java)
uses genuine original Worlds through the unchanged construction and read-only
context adapter in [MutationObservation.java](../../tests/reference/world/MutationObservation.java).
Separate client and base-server executions on the pinned Windows Temurin profile
each completed 18 direct calls, returned naturally with exit zero and retained
39 complete raw context objects. Each transcript includes actual E/F iteration,
record identities, due times, sequence values, flags, metadata time, Random,
cache, lighting and Registry context. All measured before/after context and
flag-restoration receipts passed. The recorder also rejected 18 malformed framing
controls per side; these are transcript checks, not gameplay controls.

The first counter value is explicitly `UNOBSERVED`: reading static `f` before a
real call could initialize the scheduled-record class. After the 18 calls, its
observed value is 18 and both E and F contain eight records. A duplicate with a
changed delay retains the old owner, due time and sequence while consuming a
new sequence. The calls also observe absent regions, wrapped presence ranges,
distinct full-coordinate keys, full and nonpositive block IDs, and immediate
no-ops. Every measured call returned void normally. The optional present-stone
tick is `SKIPPED_NO_ACTUAL_FOOTPRINT` on both sides; no original tick dispatch is
established by that optional case.

The retained raw JSON SHA-256 values are
`4608adc9abf6ec47de960e92bddb62921a493fc098d6ce1d71b1778072ae304c`
(client) and
`3e82e0a0c091d173e273e7e203b44343fca9d68a8d780ad9d18e54374b30f63d`
(server). The capture report SHA-256 is
`76f21e7e78248f5d20baa29d88496df44800205485431cd97ddc03a420a28be9`.
Its input and compiled-class fingerprints match before and after execution.
No original queue, sequence, time or Random state is replaced. Explicit a/o/B
inputs are recorded and restored; constructor internals and inspector effects
outside the measured World state remain uninstrumented. No cross-context
normalization or native expected-result producer is used.

To make a fresh observation, compile both project helpers with the pinned JDK:

```text
javac -encoding UTF-8 --release 17 -d out tests/reference/world/MutationObservation.java tests/reference/world/SchedulingObservation.java
java -Xint -Xmx1g --add-opens java.base/java.util=ALL-UNNAMED -cp out SchedulingObservation <original-jar> <client|server> <new-owned-world-root>
```

Use a separate absent world root for each side. Stdout is one strict JSON object
with a final LF; setup and measurement diagnostics go to stderr. Setup
unavailability exits 77 and measurement failure exits 1. Fresh clocks, Random
state and object identities are observed, so later captures need not have the
same bytes.

Separately, the native `world.scheduling` case passes 424 contracts on MSVC, GCC
and Clang; see its [validation scope](../world/scheduling.md#native-validation)
and [test source](../../tests/world/runtime/test_scheduling.c). Those contracts
are not a replay of these original transcripts. The finite original observations
establish no native comparison, helper-free control equivalence, dedicated-server
route, population, queue draining or save interoperability.
