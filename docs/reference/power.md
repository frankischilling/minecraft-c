# World power source map

The [audited binaries](binaries.md) and [binary manifest](manifest.json) pin the
client and server authorities. The implementation separates
[World queries](../../src/world/runtime/power/world.c),
[Block outputs](../../src/world/runtime/power/blocks.c) and the independent
[horizontal-direction initializer](../../src/block/registry/horizontal_directions.c).
The [ownership contract](../world/power.md) describes native admission and
diagnostics. The mappings below are source evidence, not an executed-original
power comparison.

## Four World methods

| Body | Client | Server | Ordered source sites |
| --- | --- | --- | --- |
| Strong at one cell | fd.j(IIII)Z | dj.i(IIII)Z | Acquire ID4; air returns false15; capture m16; index21; strong output28; return31 |
| Strong neighbors | fd.r(III)Z | dj.q(III)Z | Below/above/Z-/Z+/X-/X+, side0..5, calls7/22/37/52/67/82; first true returns; otherwise false91 |
| Weak at one cell | fd.k(IIII)Z | dj.j(IIII)Z | Solid support4; when true strong-neighbors14/return17; otherwise ID22, air false33, capture m34, index39, weak output46/return49 |
| Weak neighbors | fd.s(III)Z | dj.r(III)Z | Same six coordinates, sides, call sites and short-circuit order through the weak-cell body |

The public C entry is `mc_world_runtime_is_powered`. The four held interfaces
cross these actual bodies without adding World entries. Coordinate arithmetic
wraps as Java int, and directional side arguments pass unchanged to the selected
output. Block.m is captured after the direct ID getter. The solid-support
subcall instead captures m0 before acquiring ID7, indexes that saved m10,
returns false for a null Block, reads Material22, invokes Material.h25 and only
then the concrete support predicate33.

Null m fails at the reached array access21/39. A null selected Block passes
that access and fails at output invocation28/46 before a Block body starts.
The full signed ID is indexed without masking. No remote flag, future-neighbor
preflight, output-table shortcut or source work cap appears in these bodies.

## Output inheritance and registrations

Weak output is `c(Lxp;IIII)Z` on the client and `a(Lpb;IIII)Z` on the server;
the access argument is the supplied World in this implementation. Strong
output is `d(Lfd;IIII)Z` / `d(Ldj;IIII)Z`. Capability is the separate `f()Z` /
`d()Z` predicate. Seven concrete owners override directional outputs; the other
sixty-two admitted native kinds inherit uu/na's false bodies. Unknown native
kinds fail explicitly rather than acquiring an invented base result.

| Owner and native kind | Weak body | Strong body | Capability |
| --- | --- | --- | --- |
| uu/na base inheritance | False0/1 | False0/1 | False0/1 |
| bv/bc, BV_BC pressure | Metadata5; greater-than-zero result18 | Metadata5; zero false12; otherwise side1 result24 | True0/1 |
| ph/jn, PH_JN detector | Metadata5; bit8 result21 | Metadata5; absent bit8 false15; otherwise side1 result27 | True0/1 |
| oi/iz, OI_IZ button | Metadata5; bit8 result21 | Metadata5; absent bit8 false19; orientation-side result98 | True0/1 |
| xr/pd, XR_PD lever | Metadata5; bit8 result21 | Metadata5; absent bit8 false19; orientation-side result113 | True0/1 |
| db/by, DB_BY redstone torch | Receiver flag1; false8; metadata14; raw attachment exclusions through result92 | Nonzero side returns false18; side0 delegates to weak13/return16 | True0/1 |
| wo/oi, WO_OI repeater | Receiver flag1; false8; metadata14; orientation result79 | Delegates to weak8/return11 | False0/1 |
| sm/lo, SM_LO wire | Receiver flag1; false8; metadata14; zero false23; side1 true31; complete geometry follows | Receiver flag1; false8; otherwise delegates to weak17 | Receiver flag1/result4 |

The registration identifies lever ID69 as xr/pd, torch IDs75/76 as db/by and
button ID77 as oi/iz. Torch construction supplies the actual inactive/active
flag. The repeater is wo/oi, IDs93/94, with its separate receiver flag. These
labels are established by the constructor selections and registered names,
not by interpreting output behavior or matching obfuscated names across sides.

For powered button output, orientation is metadata masked with7; true
orientation/side pairs are (5,1), (4,2), (3,3), (2,4), (1,5), in that source
order. Lever adds (6,1) before those pairs. Torch weak output excludes the raw
metadata/side pairs (5,1), (3,3), (4,2), (1,5), (2,4), with no metadata mask.
An active repeater masks metadata with3 and accepts (0,3), (1,4), (2,2), (3,5).
These predicates keep the actual early flag and later getter ordering; a
known-false side does not generally permit skipping metadata.

## Complete wire output and connectivity

Wire's horizontal weak-output body first queries same-level connectivity for
X-, X+, Z-, Z+ at40/92/144/196. A failed connection asks support53/105/157/209;
when that adjacent cell is not supporting, a one-below connection is queried
at71/123/175/227. The cell above the wire is tested at247. If it does not
support, the four neighboring support tests262/296/330/364 and eligible
one-above connections280/314/348/382 follow in order. Earlier true connection
locals do not skip these later support calls.

After those calls, an isolated powered wire outputs on sides2..5. A connected
wire outputs on side2 for its Z- connection or side3 for Z+, provided neither
X connection exists; sides4/5 similarly select X-/X+ while excluding both Z
connections. Side1 was handled before geometry. This is a local output
predicate, without propagation or metadata updates.

The static helper is `sm.e(Lxp;IIII)Z` / `lo.c(Lpb;IIII)Z`. Its direction
argument is1/3/2/0 for the four same-level calls and -1 for elevated calls.
It acquires ID4, then reads live Block.aw13 and its bn16. That wire identity
can match even a zero ID. Otherwise air returns false30. Nonair captures m31,
indexes36 and invokes the selected capability37. If capability is false,
live Block.bi47/bn50 and, when needed, bj58/bn61 select the repeater cases.
Those cases read metadata71, capture jj/ge.b80 and index it with metadata&3
at87 before comparing the supplied direction. No capability result substitutes
for weak or strong output.

The jj/ge class is independent of six-direction wj/od. Its complete initializer
constructs a=[3,4,2,5], publishes19; constructs b=[2,3,0,1], publishes41; then
constructs the four-row c array and publishes172 before return175. Its rows
are [1,0,3,2,5,4], [1,0,5,4,2,3], [1,0,2,3,4,5] and [1,0,4,5,3,2]. A b read
performs this actual class active use, including the other arrays and failure
prefixes. The captured reference remains distinct from a later live b reload;
null capture fails at the later element read, and native foreign arrays are
not adopted as owned storage. The existing Registry arena retains allocations
and the class's first initialization failure.

The fixed error sites retain World weak-neighbors/weak-cell/strong-neighbors/
strong-cell callers, concrete strong/weak delegation, capability and static
wire connectivity. Full nearby-reader children preserve actual acquisition
failures. The detailed held support path carries that child while the public
support API keeps its earlier storage-only projection and nested public getter
entry. This changes native bookkeeping for held use without changing source
support reads or duplicating them.

## Primary spans and reproduction

Use `javap -p -c -s` with the side-specific binary hashes in the
[audit](binaries.md) for fd/dj, uu/na and each declaring owner above. Class and
method labels are scoped to their own binary. Offsets are JVM instruction
offsets. The following one-based LF spans refer to complete retained dumps;
hashes include their original CR bytes and final LF. The complete client dump
is 9,770,684 bytes, SHA-256
`15acc525e12512ba96782b57ead3940bf4cc608ad3dae3b9fca4e5acf96189bd`;
the server dump is 4,826,184 bytes, SHA-256
`020035f5590871cb3851e460f60013156fd6cb69c0e46bbee9454ebd33851f7e`.

| Complete bodies | Client LF / bytes / SHA-256 | Server LF / bytes / SHA-256 |
| --- | --- | --- |
| Four World power methods | 91747-91950 / 5007 / `a3874ff9e448b28dd29a322f845b370e583666a8f06c8438d8e6d4bb692b27d6` | 26561-26764 / 5007 / `b8bb946754e924c3022e255402940aece02ef49fc7cc6a0ae8c077b3412da38c` |
| World solid support | 89931-89957 / 807 / `bbc5b8186498118d9401cc17ca23c4c871598a64a01472809f7843693438b911` | 24842-24868 / 807 / `279104bb79871931cc4f5e2df7e144a59e26394e1456bb4ef1dd6fca96afb922` |
| Base weak/capability/contact/strong | 257248-257270 / 452 / `cbed0b7695b2c5c5deba3cf2383667ff6bcbc98a3292d3a3cd09225417facb33` | 117038-117060 / 452 / `e974498513d59107d9053a1baa201c1c63eba030d5a0215b90cea355f8cf3d0e` |
| Pressure outputs/capability | 13354-13392 / 925 / `e90d8ab5e705c41fa7391d414dee3ea434a595970e1e962a8ce1553890a4d012` | 6409-6447 / 925 / `1d4378b6153719e00457f99f2e3ca58d2fa418933fdf70ce09a2935ca88ba7e0` |
| Detector outputs | 207231-207267 / 915 / `fd8e9ad48463a5d73de22850fa99169384a500e2ab2606bfd6bbc3ef0a28af62` | 87515-87551 / 915 / `21e2b2c51b6c0f5f3937ec76c1d7effd8d4df7d2d9eb1c2c0a648263c3271ea5` |
| Button outputs/capability | 181977-182060 / 1984 / `2a4aefa5c429a6a661e2e0e585b52d1c683ab02c187e23d8ba09237a0fcc00d5` | 83066-83149 / 1984 / `7bdaaa1780606dc385dfa67c24904365b5d5372a34482c0b24991fc24c6d4e5a` |
| Lever outputs/capability | 279708-279799 / 2183 / `6a35c208b135ed103811a0a188b4224dd79c2d3b84ba1e1b5b301df811c14a53` | 134360-134451 / 2183 / `26fec1c892375f9c41cfdf667b1c9f564306105577924bd48194303e522e2188` |
| Torch weak output | 68452-68508 / 1401 / `a341bb76a5f14610b4472be7fb5212169a64d191348e9d7aee615dc86842eff5` | 11950-12006 / 1401 / `586ec51a03eafae9ec8bf22b09b76e1e59d7a16de94a395b4c4eb1be742b7c0a` |
| Torch strong output | 68769-68784 / 399 / `24691d636f83c1d9f556bd3df0efe7239b9e2203a5db69d91f6e465735b72bf7` | 12267-12282 / 399 / `3143d527733bdd3e86ebeb81b3240b80fa1032e499afad7c347db0f09de378bd` |
| Repeater outputs | 270482-270543 / 1533 / `0921f725be3fc11f3fca7deb2a4b00e5e4e982e56cac9a3153d512a8effa9ed4` | 128411-128472 / 1533 / `35a13cdbb3521c82722c79b9d50057bf28c3ccc3f4276a05999efb8ac2d06162` |
| Wire outputs/capability | 234896-235223 / 8503 / `7d74484b6316b5accba679bdd6514b994e1a6ca8d019c4bab3aaa0423f2ba53d` | 99894-100221 / 8503 / `2dec415f3204b3b04e9f4b8c9af039733b79edffb73e1bcd39d392f5731427e6` |
| Wire static connectivity | 235324-235377 / 1697 / `966a0c5cbec5008e80de9a5f8d1b0ebe3093d41c7d17b17b5b6ba353f7f64783` | 100222-100275 / 1697 / `d7768c4db2dcd1176087aebe2c867809d4d411ee326b24e94b67bc3ab7dddbf1` |
| Complete jj/ge class | 132117-132295 / 3592 / `9916a52fd755f184d228997b44bfff5ba392a49a1552c483a60d390011d964af` | 59187-59365 / 3592 / `070a8cf9001e15602491d03fc44c47afb4f09f3998e1e2a7140b7bcf32cbf22f` |
| Lever registration | 258352-258364 / 726 / `3399e781e4567a4d784e5d4eef3a6b4d12f91e7d220ee60b91832df95c85847e` | 118137-118149 / 726 / `4aea23643d45416d911e34ab810adac43193cd2df5333154a79d68adc8a9df31` |
| Torch/button registrations | 258445-258488 / 2364 / `1d82a7464b6d10bea771726006ff677e45a8c247d5fb374e6df31a7a89a1e0cb` | 118230-118273 / 2364 / `e99a758d486eba89e32d744f999103956d6b7cd8e35023b7a0589fa4fe8fca78` |

The additional capability bodies are detector client207171-207175 /
server87455-87459, torch68792-68796 /12290-12294 and repeater270808-270812 /
128737-128741, under the same whole-dump pins.

## Validation boundary

The [native fixture](../../tests/world/runtime/power/test_power.c) passes 7,593
checks per host; the separate horizontal-direction case passes 359, and existing
public support passes 4,359. Complete suites pass 173 MSVC and 175 GCC/Clang
tests each without skips. Linux uses ASan, UBSan and leak checks. All 499 C
files and headers pass clang-format 18. [Testing](../testing.md) describes the
held source maps, actual process receipts and regression scope.

These results do not establish executed-original power equality, wire
propagation or lifecycle, torch burnout or lifecycle, switch interactions,
rail integration, scheduled draining or whole-game acceptance.
