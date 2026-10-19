# Item and scheduled-record evidence

Status: source research, native contract tests and a finite JDK collection replay.
No original-game runtime comparison or complete item/container/world scheduler
qualification is claimed.
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
without rebuilding its ordered collection. Those world operations remain outside
the implemented record/membership layer.

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
branch coverage nor world enqueue/drain/reentry behavior. The helper does not
instantiate the game's scheduled-record classes; their construction/equality
binding remains source research pending original-game comparisons.
