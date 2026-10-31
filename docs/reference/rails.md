# Rail source map

The [audited client and server binaries](binaries.md) define inherited rail
owners `pc`/`jj`, detector `ph`/`jn`, connection helper `vo`/`nq` and coordinate
owner `wf`/`oa`. Their native bodies are
[connections](../../src/world/runtime/mutation/rails/connections.c),
[lifecycle](../../src/world/runtime/mutation/rails/lifecycle.c),
[powered search](../../src/world/runtime/mutation/rails/power_search.c) and
[detector update](../../src/world/runtime/mutation/rails/detector.c).
The [runtime contract](../world/rails.md) describes ownership and diagnostics.
This is bytecode implementation evidence; no executed-original rail comparison
is recorded.

## Connection helper

The complete helper has thirteen methods, including its constructor and static
degree accessor. The native opaque helper owns its private List and coordinate
occurrences; World reads and metadata notifications remain production calls.

| Source method, client/server names where different | Behavior and source sites |
| --- | --- |
| Constructor `(rail,World,III)` | Save outer2; allocate List10; save World22/coordinates27/33/39; ID48 then metadata59; capture m64/index69/cast70; flag accessor73; build shape List102 |
| `a(I)V` | Clear4, append the two literal endpoints for shapes0..9; other shapes leave the List empty; return717 |
| `a()V` | Resolve each current occurrence29; remove nonreciprocal links53 or replace with the resolved coordinate86 |
| `a(III)Z` | Presence at the same level7, above24, then below41; short-circuit true |
| `a(wf)vo` / `a(oa)nq` | Resolve same level16, above68, below122; construct a fresh helper at the first match |
| `b(vo)Z` / `b(nq)Z` | Membership compares X and Z, ignoring Y |
| `b(III)Z` | Coordinate membership also compares X/Z only |
| `b()I` | Count physical neighbors in Z-, Z+, X-, X+ order through three-level presence |
| Static `a(vo)I` / `a(nq)I` | Invoke the degree body at1 |
| `c(vo)Z` / `c(nq)Z` | Existing link succeeds; exactly two links reject a new one; other sizes succeed after the reached reads |
| `d(vo)V` / `d(nq)V` | Append the connection23, select shape/slopes, preserve a flagged rail's late bit8, always notify metadata420 |
| `c(III)Z` | Resolve11, prune25, then test admission31 |
| `a(ZZ)V` | Test connectability15/34/54/74; choose shape/slopes; rebuild List516; test equality577 unless forced; notify603, then resolve/prune/admit/reconnect neighbors638/653/659/668 |

The ten endpoint pairs are literal offsets from the helper's cell:

| Shape | First endpoint | Second endpoint |
| --- | --- | --- |
| 0 | Z- | Z+ |
| 1 | X- | X+ |
| 2 | X- | X+, Y+1 |
| 3 | X-, Y+1 | X+ |
| 4 | Z-, Y+1 | Z+ |
| 5 | Z- | Z+, Y+1 |
| 6 | X+ | Z+ |
| 7 | X- | Z+ |
| 8 | X- | Z- |
| 9 | X+ | Z- |

Unique straight/curve decisions precede ambiguous junction choices. Powered
curve assignments run in order 6,7,9,8; unpowered assignments run 8,9,7,6. Later
eligible assignments win. Flagged rails skip curves. Both elevated predicates
of a selected straight axis still run, even after the first changes the shape.
A nonforced equal reshape has already rebuilt its List; it then skips the
write and reconnect loop. Forced reshape skips the equality getter. Reconnect
always calls the notifying setter, including equal metadata, and does not
rebuild its List.

The constructor gets the actual flag from the selected rail in captured m,
not from its retained outer reference. When flagged, it clears bit8 with `metadata & -9`;
later flagged writes use `(live_metadata & 8) | shape`. Rail presence performs
the actual ID getter before comparing live defining-owner `aH`, `U`, `V` IDs.
The ID-only predicate's static/field pairs are1/4,11/14,21/24; presence uses
11/14,22/25,33/36 after getter4. These are active-use operations on the actual
Registry, rather than a native kind or constant-ID test.

## Lifecycle and powered search

Added checks World.B1 and calls private shape entry14 with force=true. That
entry checks B independently, constructs the helper8/18, asks actual World
power26 and reshapes31. Neighbor checks B1, saves metadata13 and reads its own
flag23. It checks support below46 and the applicable slope support68/90/112/134,
including when the below result was already false. Loss of support reaches
metadata159, inherited drop162, then notifying air171.

With support present, neighbor saves receiver bn179 before live U182/bn185.
Equality selects power196, power209 above, forward search237 and reverse253,
each later call reached only after false. A changed power bit writes292/321,
then rereads bn before below343 and applicable above381 notifications. The
other branch requires positive sourceID, captures m392, indexes397, calls
capability398, then checks receiver flag405. Ordinary rails with degree3
rerun private shape438 with force=false.

Search `a(World,IIIIZI)Z` returns false when depth>=8 before any World getter.
Otherwise its six-way shape switch applies Java integer wrapping and chooses
the next coordinate and axis. It calls verifier207, then only on false and an
eligible nonascending step calls verifier234 one block below. Default masked
shapes6/7 keep the current coordinates.

Verifier `a(World,IIIZII)Z` reads ID5 before live U12/bn15, including for air.
Only an ID match reaches metadata26. Axis1 rejects shapes0/4/5; axis0 rejects
1/2/3. Saved bit8 must be set before power101 and optional power114 above.
If both power checks return false, it recurses136 with the full raw metadata
and wrapped depth+1.
Lifecycle starts at depth0. The native implementation adds no negative-depth
gate or recursion/work cap to this source rule.

## Detector contact and tick

Contact `ph/jn.a(World,III,Entity)V` checks B1 and metadata13, returning when
bit8 is already set. Tick `a(World,III,Random)V` uses the opposite bit guard.
Both pass saved full metadata to private `f(World,IIII)V` at35. The supplied
Entity and Random are unused arguments.

Private update resolves the defining owner's Minecart Class23 before float
conversion27 and the actual pooled box64. X/Z use binary32 conversion and
`0.125f`; upper coordinates wrap integer+1 before conversion/subtraction.
Y is double, with maximum Y+0.25. Real Class query67 returns a fresh List;
the body reads that captured List's size74.

The saved on/off value and captured nonempty result select transitions even
across synchronous notifications. Rising writes saved metadata OR8 at105,
notifies current117 and below131, then range143. Falling writes saved
metadata AND7 at166, then current178/below192/range204. Each notification
reads live receiver bn separately. A nonempty result reads bn218, invokes
the detector's literal delay20 at222 and schedules225, including when already
on. The genuine scheduler retains its immediate/deferred and partial-publication
behavior.

## Primary evidence and validation boundary

Reproduce the source inventory with `javap -p -c -s` on the pinned side binary
for pc/ph/vo/wf or jj/jn/nq/oa. Class names belong to their respective binary.
These one-based physical-LF spans retain their original CR bytes and final LF.
The complete client dump is 9,770,684 bytes, SHA-256
`15acc525e12512ba96782b57ead3940bf4cc608ad3dae3b9fca4e5acf96189bd`;
the server dump is 4,826,184 bytes,
`020035f5590871cb3851e460f60013156fd6cb69c0e46bbee9454ebd33851f7e`.

| Complete source span | Client LF / bytes / SHA-256 | Server LF / bytes / SHA-256 |
| --- | --- | --- |
| Connection helper | 262840-264190 / 46083 / `d95726b3e60357faab37233a63ebe0994d73f5fab9992c3ca9df470d6f04ba67` | 123594-124944 / 46083 / `47d35f277dab0761a85632aa901fd5980428c2a966eee96a4588450f493e3cc5` |
| Coordinate class | 269290-269314 / 625 / `d8ffce80eb89c9d0810441fede3d73ddb26ed999d20bfe8472acd27b97bda1de` | 127340-127364 / 625 / `3b27871675886459c587e5eece7d37ae269f62bf3b4d1244ef4ada36cdd3a1aa` |
| Rail predicates | 205519-205568 / 1655 / `89a0ce7cb543b2f520746b84911cbbf3615fd4be66d566ec2c82d8481d8b6333` | 85890-85939 / 1655 / `6338beaa5aeafea378584fbea4d3c0f22e10882ee21b855eb004d0d9a2dd5ee5` |
| Flag accessor | 206219-206225 / 160 / `ce4588b2e0bdd7d8da131219ae50b6aebfd77ef2cb73c6fd8e45460186d6c897` | 86584-86590 / 160 / `16fc6b10e5fdaa549e711030c37271bec3d108be338067f703e55d5f9f94f7e2` |
| Support, added and neighbor | 205712-205994 / 7840 / `d8a027b6da85abcd30d0faae7cf9f7d640dba125d421a87b3f047a22c86af9cb` | 86077-86359 / 7840 / `9d632ce1a31fcb61b9bcc0d7c14a5b01a1ad5be3d1bfc5378259cf6b60b00be3` |
| Private shape entry | 205995-206018 / 733 / `3ae3452d7b2ac3fc6532d51ddeaa0bbff244e77667d823b21bf97199e9c678b0` | 86360-86383 / 733 / `6156dedda72659ce4834dbeb849a1bdd6f194fb2809fed24e2ed8978f6aa2c90` |
| Both powered searches | 206019-206211 / 5208 / `79fbce1ae31676743cc53aeb3ba65d449b114f00ad47976c1f3b0111eee7ec1d` | 86384-86576 / 5208 / `35fbc71171d108e3abf671052f6129315998912cdb08dae997bc8f8d5f845d11` |
| Complete detector class | 207150-207407 / 6924 / `e0148258f24b040eab905c41af224e77f216ff7b04ada786a22b4d8c8046ee42` | 87434-87691 / 6924 / `70ee9ad6f1be4bd6f012a4bd454da85c5e754ff1de6d3dd560c428860137cd38` |

The [graph/search fixture](../../tests/world/runtime/rails/test_rails.c)
passes 814 checks across 144 literal cases on each host. The separate
[error fixture](../../tests/world/runtime/rails/test_error_sites.c) passes
1,444 checks across 94 constructed record-transport cases. All 175 configured
MSVC tests and 177 tests on each GCC/Clang host ran and passed, with zero
failures, errors or skips. Linux uses ASan, UBSan and leak checks. Controller
mirrors and lifecycle intervals also pass. Related Snow, movement and power
regressions pass 392, 564 and 7,593 checks per host.

The tested source-file map contains 977 paths and has canonical-LF SHA-256
`1c6bf379c6f4d4cdaa742caf9ca12e7d4d494c1a7b478574e60b50318d7d55b4`.
It binds the rail overlay and the held 961-file public baseline at
`9f9d3a22900ac25c8fc2813ce2f5881fcedb63a2`. Source bytes matched before and after execution. Closed stream and result
seals remained stable through the result audits. [Testing](../testing.md)
describes the source-map and strict-profile evidence. Closed result receipts
have these byte identities:

| Host | Receipt bytes | SHA-256 |
| --- | ---: | --- |
| MSVC | 3,623 | `98ad70ead94f2f423e7978aa28940d521e83f0ec77f87c7670ac314d8cb2fd8a` |
| GCC | 3,235 | `94f3e3be27a9b8608086c54c005d1ca8964395454ddfe277f83976a29c19f005` |
| Clang | 3,242 | `842f32e66d7308efc1b54e4a35c0f8ee5a45ad281535f0b2320c3a4c2266dc1e` |

These are finite native contracts over constructed inputs and error records.
The case and check counts do not represent original rail observations.

Positive Minecart construction/contact, gameplay rail physics, successful
ordinary rail-drop publication, scheduler draining, rendering and
executed-original rail equality remain unverified. The finite
[power comparison](power.md#validation-boundary) does not supply rail runtime
evidence.
