# Item and scheduled-record evidence

Status: source research with native contract tests. No original-runtime
comparison or complete item/container/world scheduler qualification is claimed.
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
qualification. No ordered native collection is claimed here.

The native source copier and item provenance overlay are explicit storage policy:
unknown data remains owned independently of caller documents, while clone/split
start without provenance. Container fields remain separate. These operations do
not establish original mutable-compound partial-write behavior or HashMap wire
order.

The integrated GCC 13.3 x86-64 Ubuntu build uses C17, strict project diagnostics,
disabled floating contraction/fast math, and address/undefined-behavior
sanitizers with leak detection enabled. The two CTest cases pass: `item.stack`
has 93 checks and `world.scheduled_membership` has 329. A native x86-64 Windows
MSVC 19.51 Debug build passes the same two cases with strict project diagnostics
and sanitizers disabled. Format and whitespace checks pass. These finite native
API tests supply no original oracle, allocation fault injection, Clang result,
bound-world callback result or gameplay parity result. Those remain required
before broadening acceptance.
