# Original Player sleep and wake components

This finite packet observes unchanged client `GS.b(III):CW` sleep entry and
`GS.a(ZZZ):void` wake bodies, their reached children and partial failures.
It is original component evidence. Native parity, elapsed sleep ticks, day
advance/weather, default-hostile CQ/FW, a complete World session and graphical
behavior require separate evidence. The older `player_sleep` 48-case packet
and its 13 segregated later completions are unchanged and receive no new credit.

Two fresh guarded headless JDI target VMs produced identical selected
projections: 76 calls, 1,313 selected invocations and 3,979 ordered events per
VM, 26 live Code hashes, and 16 original classfile/origin pins. The source JAR
SHA256 is `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Each call constructs a genuine FD World, WY saved-data handler, XA Overworld
provider, LM Chunk and OK Map provider, then genuinely publishes DC into both
actual World Player/entity lists. DC's App argument is deliberately null.
The selected bodies do not use it; this is not a loaded Minecraft App.

The authored Chunk has Grass support at y63, explicit Block/metadata/light/
height planes, and Bed ID26 in all four directions. Direct fixture plane
writes precede selected calls. World.f=5 supplies the night predicate; no time
is progressed. After genuine sleep entry, wake fixtures explicitly supply
timer37 and occupied metadata, then optional absent/null/blocked/remote inputs.
The null-bounds cases deliberately reflectively replace final SN.aW before
the selected call. They are negative component inputs, not normal construction.

| Cases | Scope |
| --- | --- |
| 0 | Sleep null-bounds partial failure |
| 1–4 | One published Player, local sleep, four directions |
| 5–8 | Local sleep with a second published awake Player |
| 9–12 | Declared remote sleep, four directions |
| 13–16 | Loaded air ID0 carrying orientation metadata; no invented GS Bed-ID guard |
| 17–48 | Matching Bed wake, four directions × eight boolean combinations |
| 49–56 | Absent Bed wake, eight flags |
| 57–64 | Null current-v wake, eight flags |
| 65–68 | Blocked-exit fallback wake, four directions |
| 69–72 | Declared remote wake, four directions |
| 73–75 | Null World/nonnull-v, null World/null-v, and null-bounds-after-clear prefixes |

`profiles.tsv` contains five space-separated signed decimal integers per row:
case, kind (0 sleep/1 wake), direction, flags (bit4 first boolean, bit2 second,
bit1 third), scenario. `profiles.json` preserves their names and values.
There are 16 successful sleeps, 56 successful wakes and four actual NPE
prefixes. Cases0/75 throw at SN.e86 after XYZ stores; case73 at GS.a48; case74
at GS.a187 after u=false. Earlier stores and the case75 occupancy clear survive.

`packet.read_reference()` returns `BED_NIGHT_COMPONENT_FINITE_V1`, with `rows`
and `live_code`; `packet.profiles()` returns the named five-int profiles. Every
row retains exact before/after selected fields, within-call preservation facts,
both Random states and the ordered call/store/exception journal. Position,
motion and bounds use raw 16-digit lowercase double words; dimensions/offsets
use 8-digit float words. Coordinates are null or three signed integers.
Identity booleans compare actual entry/current objects; new-v and Q-copy
identity remain distinct. World.d/b counts and J are actual graph/body results.
All five complete Chunk planes remain represented by their hashes.

Journals retain source PCs, typed arguments/results, ENTER/RETURN/STORE/
EXCEPTION ticks and failed open calls. Only explicitly named raw JDWP IDs are
excluded from the canonical journal. Symbolic identities and alias relations
remain. Raw constructor entropy, complete field snapshots, process pipes,
commands, generated classes and private World outputs stay private. Unselected
Player fields and World/provider/Random preservation were checked within each
call; constructor values are never compared across implementations or fresh
VMs. No blanket state normalization is used.

The real resize dispatch is GS -> LS.b(FF) -> SN.b(FF), including LS invoke3.
Size stores do not rebuild bounds. Sleep preserves Q, publishes fresh v, zeros
motion in Y/Z/X order, invokes local World.y and reads CW.a late. Wake clears
occupancy before exit search, uses an actual BR fallback when blocked, stores
u=false before reading World.B, then conditionally updates J, sets timer and
copies late current v into Q. Absent/null-v wake skips reposition and preserves
the existing small bounds. Later World ticks, natural wakes, damage/NBT parent
dispatch, WorldAccess replacement callbacks and hostile CQ/FW are outside scope.
Local B=false does not create GH: the local graphical continuation is UQ's
sleep fade plus actual World/wake/tick behavior. Remote GH behavior is untested.

The target guard counts zero game network/listen, native-link, exec and write/
delete operations after installation. Debugger transport/JVM startup precede
that guard. Ticks/GUI are never invoked; zero scope labels are not independent
execution counters. JDI observes entries/exits, stores and exceptions without
target invocation/write/redefinition/forced return. Provenance preserves the
actual executed Java/Python/helper fingerprints and the two JDK executable/
four imported SDK archive fingerprints. It does not invent historical checks
for additional JDK runtime files. Current portable reproduction/exporter hashes
are separate; this offline export adds no original execution qualification.

Run `python -B tests/reference/entity/player_sleep_wake/test_packet.py` normally
and with `-O`. The reader needs neither JAR/assets nor private raw directories.
It checks anchored historical maps, canonical hashes, closed typed schemas,
source order and corruption controls. Historical private audits rejected 687
corruptions in each mode; portable controls additionally cover canonical-only
reference shapes, historical map/inventory edits and output-path refusal.

To reproduce, supply local `--jar`, `--jdk` (the pinned Windows JDK17.0.17),
`--libraries-root` and `--output .local/<new-attempt>` to `capture_reference.py`.
It records a new private two-VM attempt and checks the unchanged producers,
classes, helpers and finite effects. Public/foreign/existing destinations are
rejected before any mkdir. `export_reference.py --evidence <private-sealed-root>
--output .local/<new-export>` checks the immutable original seal and reconstructs
both raw projections before copying portable members only. It confers no new
execution qualification. All members use LF; the executed Java/data bytes were
already LF and were copied unchanged. Original class/JAR/assets are excluded.
