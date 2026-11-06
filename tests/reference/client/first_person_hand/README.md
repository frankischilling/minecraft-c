# Original first-person hand calls

These fixtures execute unchanged methods from the verified Beta 1.7.3 client
archive. They separate the outer PX call, concrete arm-model drawing and the
empty-hand RA body. All observers use a real Windows LWJGL2 context. They deny
network, listeners, file writes and native libraries other than the extracted
pinned LWJGL DLL. No game class, method or successful child return is replaced.

`source-pins.json` records complete class hashes and selected method Code
identities. `operation-map.json` records field and invocation PCs from unchanged
bytecode. Source PCs are static call mappings, not values observed from a JVM
instruction trace. Each capture fingerprints tools, original archives, native
library, observer source, metadata and compiled helpers before and after use.

## Outer PX call

`HandObservation.java` invokes private `px.b(FI)V` on actual PX, App, Settings
and Player objects whose constructors are deliberately bypassed. The actual RA
field is null. Its 64 profiles cross four cases, partial ticks .375/1, view bobbing
off/on, anaglyph off/on and eye 0/1.

The actual Settings fields are `A=third_person_view`, `z=hide_gui`,
`f=view_bobbing` and `g=anaglyph`. The source tests third person before sleeping
and hidden GUI for arm rendering; only third person and sleeping suppress the
later overlays.

| Case | Actual prepared fields | Original outcome |
| --- | --- | --- |
| 0 | A=true, z=false, Player.u=false | Return without RA calls |
| 1 | A=false, z=false, Player.u=true | Return without RA calls |
| 2 | A=false, z=true, Player.u=false | Null RA overlay receiver after the outer PopMatrix |
| 3 | A=false, z=false, Player.u=false | Null RA arm receiver before the outer PopMatrix |

The Player has health 20, hurtTime 7, hurtDuration 10, hurtYaw 60, deathTime 0,
walk-distance fields bi=1.125/bj=1.5, bob fields h=.1/i=.175 and af=2/ag=-1.
Initial projection is identity scaled (1.25,.75,1.5). Initial modelview is
identity translated (5,6,7) then scaled (2,3,4), so the source LoadIdentity is
observable. The original hurt and bob methods execute normally.

`outer.json` holds 64 `HAND_OUTER` rows: index, case, partial index, bob, anaglyph,
eye, return/null outcome, actual modelview stack depth, matrix mode, 16 modelview
float hex values and 16 projection float hex values. Case 3 retains stack depth 2;
the other cases retain depth 1. Only after recording that source prefix does the
observer prepare the next independent call's initial stack. It does not describe
source failure cleanup as a PopMatrix. Private output also records actual Java
exception stacks. Native matrix comparisons use the separately documented
camera coefficient tolerance; original rows retain raw bits.

## Concrete DS arm model

`ArmObservation.java` genuinely constructs DS and its FH/PS model graph. It calls
unchanged `ds.b()` twice: first compiling the right-arm PS list, then replaying
the same list. The actual DS.a FH has m set to zero by DS.b before its normal
pose method runs. The actual arm is FH.d, with constructor pivot (-5,2,0).

Each call starts with g zero, e.position zero, Vec3 cursor zero, identity
modelview and current color (.125,.25,.5,.75). The recorded `ARM_PART` rows contain
repeat index, actual compiled flag, actual GL list-defined flag, same-list-ID
flag, stack depth, Vec3 cursor, 16 modelview floats and seven scalars: FH.m,
then arm pivot a/b/c and angles d/e/f. The first call consumes 24 pooled Vec3s;
reuse consumes none. List IDs are checked as real positive identities and
stable across reuse; their incidental numeric allocation order is not compared.

The observer then resets g and the Vec3 pool once and invokes all six actual
arm TZ faces in sequence with scale .0625. It does not clear g between faces.
Each original NW batch contains six converted vertices. `ARM_FACE` rows record
face index, vertex count, cumulative Vec3 cursor and SHA-256 of actual g words
0..6 per vertex, each int encoded big endian. The cumulative cursors are
4,8,12,16,20,24. Reserved word 7 is omitted. The original untouched duplicate
normal words retain their actual carry from earlier faces. Private `ARM_WORDS`
rows preserve full selected payloads for diagnosis.

Finally, 16 `ARM_POSE` rows cross all persistent j/k/l/inherited-n flags while
calling unchanged DS.b. Mask bits 1/2/4/8 select those fields. Each row records
mask, pooled cursor and 54 raw scalar words: parts a,b,c,d,e,f,g,h,i in that
order, each with pivot a/b/c followed by angles d/e/f. This records the actual
side effects on the complete biped, beyond the arm that is drawn. `arm.json`
contains `part_rows`, `face_rows` and `pose_rows`.

## Empty hand and dry overlays

`FullHandObservation.java` uses genuine original in-memory WY/World and Player
constructors, and the real global TH/DS/FH/PS graph. It prepares actual App, RA
and Engine fields for their reached bodies. RA.b is null, c=.75 and d=.25.
Engine's real image-download map is empty; its real cached-texture map contains
an actual authored white 64x32 `/mob/char.png` texture, and its actual PackList
slot is present. The real `ji.a(String,String)` fallback and `ji.b(String)`
cache-hit methods run. No skin worker or network request is created.

World seed is 23, time 6000 and the actual sky-subtraction update runs. The actual
acquired Chunk's (8,127,8) block is air, SkyLight is 15 and BlockLight is 0. The real
World brightness query at (8,200,8) returns 1. Player position is (8.5,200.5,8.5),
current/previous yaw 33/12, pitch -17/3 and swing W=.1/X=.5. This above-height
position makes the real opaque and water queries return false in the dry overlay
body. No World method or query result is supplied by the observer.

The four calls are RA.a(.375), RA.a(1), RA.b(.375), RA.b(1). Before each call,
actual g/e and the Vec3 pool reset; modelview is identity and current color is
(.125,.25,.5,.75). Alpha testing is enabled; rescale-normal, lighting, both lights,
color material and fog are disabled. After each call, actual current color is
queried before modelview and the other source state.

`full.json` contains four `HAND_DRAW` rows: entry 0/1, partial index, stack depth,
Vec3 cursor, 16 modelview words, four current-color words, alpha/rescale/lighting/
light 0/light 1/color-material flags and actual NW e.position. First empty-arm
drawing consumes 28 Vec3s and compiles the model; reuse consumes 4. The dry overlay
calls consume none. The first call leaves 48 uploaded NW words; later calls leave
the prepared zero cursor. Source classes and methods remain unchanged throughout.

## Reproduction and scope

Use a matching JDK 17, pinned locally supplied client archive and SDK libraries:

```sh
python -B tests/reference/client/first_person_hand/capture_reference.py \
  --client-jar /path/to/b1.7.3.jar --library-root /path/to/libraries \
  --output .local/hand-outer-new
python -B tests/reference/client/first_person_hand/capture_arm.py \
  --client-jar /path/to/b1.7.3.jar --library-root /path/to/libraries \
  --output .local/hand-arm-new
python -B tests/reference/client/first_person_hand/capture_full_hand.py \
  --client-jar /path/to/b1.7.3.jar --library-root /path/to/libraries \
  --output .local/hand-full-new
```

Output directories must be new. The original device is NVIDIA RTX 3060,
OpenGL 4.6, driver 581.42. No original asset, class or decompiled source is
distributed. These finite observations do not establish arbitrary held Items,
fire/water/opaque overlays, account skin services, prepared constructor parity,
complete Entity rendering or matched gameplay frames/pixels.
