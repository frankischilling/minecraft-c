# Original dropped Item billboard reference

The ordinary and read-only-JDI captures each complete four actual
BB.a(HL,DDDFF) calls on a real LWJGL Display. Genuine Stick Stacks 280 with
metadata 0 and counts 1/2/6/21 produce 1/2/3/4 copies: ten original NW draws at
bytecode 776. The selected local geometry contains 480 raw words, 340 active and
140 inactive words checked against each batch's own prior backing. Another
160 unwritten tail words retain their actual prior values.

The scene uses the genuine complete KQ constructor, Settings, Session, Engine,
Font, World, Player and TH singleton. The actual TH frame method binds those
owners. TH's canonical HL-class lookup supplies the real BB renderer; every
constructed Item resolves back to that same instance. Each profile constructs
its own actual IZ and full HL(World,8.5,64,8.5,Stack). The Stack reference,
World reference, phase, yaw, velocity, age 0, pickup delay 0, health 5 and private
Random remain genuine constructor values. Items are not inserted into the
World Entity list by this helper. The OO close/drop caller, World traversal
and Block-flat branch are outside this constructor scene.

The actual parent supplies its first icon, color and World brightness reads.
JDI records IZ.b returning icon 53, GM.a returning 53, GM.f returning white,
SN.a returning the actual World brightness, the sprite-sheet lookup/bind and
GL color arguments. The call uses render position (1.25,2.5,-3.75), yaw 35 and
partial .375. Whole-parent matrices, stacks, selected GL flags/color and error
state are queried before/after; actual GL calls and their arguments are also
observed. Read-only JDI does not invoke GL queries in a stopped target, so
per-prefix driver-matrix reads are not claimed. All four parent calls return
with balanced matrices/stacks, rescale-normal disabled and zero GL errors.

The original source reseeds its own BB Random to 187 at bytecode 7. The helper
does not seed or replace it. JDI sees that actual call, its receiver, exact
nextFloat returns and three offset floats for each additional copy. The same
real Random owner persists between profiles. Both processes independently
audit the actual final seed progression. JDI also observes all four shared
Math returns in each full HL constructor and checks the phase/yaw/velocity
formulas. Those values remain independent between processes.

Bob is not normalized. The actual IN sine-table call receives that process's
real constructor phase, and its return feeds the actual parent GL translation.
Actual manager yaw and billboard rotations are retained. Raw constructor,
World/renderer entropy, per-copy offsets and GL evidence remain in
`observations.json`; none is replaced with a common cross-VM value or alias.
The local NW comparison has a narrower scope than those parent transforms.

At bytecodes 673/681 the actual parent begins NW and supplies normal(0,1,0).
The four vertex calls at 703/726/749/771 use exact local XYZ/UV values for
Stick 53. The genuine singleton NW expands each quad into six vertices. The
normal is active on original vertices 0/1/2/5; duplicated vertices 3/4 keep their
own prior normal words. All color and reserved words also keep their own
indices. Actual draw 776 returns for each copy, then the original parent pops
that copy and reaches its final disable/pop/return suffix. No backing is
cleared or staged for comparison.

The native protocol is `ITEM_BILLBOARD_GEOMETRY {JSON}`, with ten ordered
rows keyed by profile 0 through 3. Each profile's copy index runs from 0 through
the profile number. Fields are profile, copy, icon 53, uv [u0,u1,v0,v1], prior
48, words 48, state and completed-draws. State retains vertices 6,
source-vertices 4, words 48, mode 7, building true, color false, texture true,
normal true and three raw64 zero translations. The original rows have
completed-draws 1; native entered-prefix rows have 0. The comparator first
audits each process's own inactive backing, then compares all 340 active
XYZ/UV/normal words and selected source state exactly. Original unwritten tails
remain retained separately. This component does not credit a native full
parent return, GL draw, bob transform or pixel comparison.

The APIs are `packet.audit_public()` (also `packet.audit()`),
`compare.rows(stdout)`, `compare.compare(actual, finite["rows"])`,
`compare.active_indices(row)` and `compare.audit_inactive(row)`.
`finite.json` is an unmodified selection from actual JDI draw records.
`source-pins.json` and the live records identify 14 original, 9 LWJGL and 2 JDK
method bodies. Exact raw UTF8 stdout/stderr and six executed helper/input files
are preserved. All 109 original/library/native/JDK/helper inputs and compiled
helper identities match before/after execution.

The 40 Java-free controls cover typed protocol/order/counts, active geometry,
normal duplication, own inactive backing/tails, real constructor entropy,
Source reseed/copy offsets/bob, actual normal/vertex/GL draw calls, parent
returns, color leaves and camera rotation. Synthetic native-shaped JSON tests
the transport; it is not a native execution receipt. Native integration runners
and native receipts are separate from the captured helper manifests.

Prepared-1 is retained as prepare-only: no Display ran before a qualified
LWJGL-owner parser correction. Prepared-2 failed in the authored CURRENT_COLOR
reader before BB entry because LWJGL2 requires a 16-float buffer even for that
four-component query. Prepared-3 uses a corrected read buffer and fresh
processes. The private generic preparation label still says two draws; the
actual receipts and public scope are four calls and ten completed quads.
No captured helper/result was rewritten to change that descriptive label.

```powershell
python -B tests/reference/client/item_billboard/packet.py
python -B -m unittest discover -s tests/reference/client/item_billboard -p test_packet.py
```

Prepare a fresh directory without GL, then reserve the desktop for ordinary
and JDI Display processes in sequence:

```powershell
python -B tests/reference/client/item_billboard/capture.py --output .local/item-billboard-new
python -B tests/reference/client/item_billboard/capture.py --observe --output .local/item-billboard-new
python -B tests/reference/client/item_billboard/publish.py --capture .local/item-billboard-new
```

All executed helpers/inputs were frozen ASCII/LF before capture. Publication
validators were authored afterward and are hashed separately. No source class,
renderer, Stack, World field, Item scalar, Random or backing substitution or
repair supplies the result. This packet makes no full startup, OO close/drop,
Block-flat, complete World, gameplay, focus-stability or FPS claim.
