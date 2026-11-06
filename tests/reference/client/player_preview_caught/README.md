# Original cold Player preview catch and escape

The first cold UE preview catches and prints its missing-Engine NPE, then
escapes from the GV suffix with a distinct missing-camera NPE. UE does not
return. A fresh process calls the genuine TH frame method and then completes
two UE draws. This packet retains both outcomes without repairing the failed
scene or relabeling a partial continuation as success.

The ordinary and read-only-JDI processes agree on exact stderr bytes, exit
codes, declared constructor state and observed GL/pose results. The cold
process exits 1 after its first attempted draw, so the requested second cold
call is not executed. The separate warm process exits 0 with two returned
calls. The capture's PASS status describes that bounded observation protocol.

Each scene uses the genuine complete KQ constructor, Settings, Session,
Engine, Font, World, LocalPlayer, IX/default AA, statistic notice, canonical
TH singleton/DS/FH models, RA and UE/ID constructors. Held and armor remain
constructor-empty. The actual LR(Settings) constructor and DC Input
publication follow the declared Minecraft load protocol at 194/199/202/205;
all ten key cells retain their constructor state. The genuine TH.a(World)
setter publishes g before the UI call. Engine e, camera h, Settings k and
Font p remain constructor-null until the actual frame method runs. No helper
writes those owner fields or substitutes a sneaking result.

The cold source path is observed directly:

- BW.a(String,String)11/source36 throws the Engine NPE. GV's Exception handler
  at 743/source116 catches that same object.
- The source invoke at 747 calls Exception.printStackTrace. The actual inherited
  JDK declaring body is Throwable.printStackTrace. JDI sees the same NPE enter
  and return from that body, then reach GV750 with its materialized trace.
- GV reaches cull-enable753, pop756 and its dynamic suffix child766/source125.
  The suffix reaches SN.f5/source678 with a null camera argument and throws a
  second NPE. GV769, DS cleanup115/139 and UE continuation350 are not reached.
- The authored caller's finally only reads state. It retains bE=1, temporary
  H/aS/aT, modelview depth 2 and rescale-normal enabled, with zero GL error.
  It does not reset fields or unwind the leaked transform. The exception then
  reaches the JVM uncaught handler.

The real Source print begins with this JDK17 helpful-NPE message:

```text
java.lang.NullPointerException: Cannot invoke "ji.a(String, String)" because "<local3>" is null
```

Read-only JDI observes detailMessage NULL, the helpful extendedMessage
materialized by the real print, cause=self and the actual nine-element
StackTraceElement array. Its eight original frames are:

| Declaring method | Caller/throw PC | SourceFile line |
| --- | ---: | ---: |
| BW.a(String,String) | 11 | 36 |
| GV.a(LS,DDDFF) | 254 | 60 |
| DS.a(GS,DDDFF) | 112 | 81 |
| DS.a(SN,DDDFF) | 14 | 17 |
| TH.a(SN,DDDFF) | 24 | 141 |
| UE.a(F) | 347 | 73 |
| ID.a(IIF) | 32 | 37 |
| UE.a(IIF) | 4 | 32 |

The ninth frame is the authored PlayerPreviewCaughtTarget.main caller. Its
compiled Code and line metadata are retained separately. The subsequent
`Cannot read field "aM" because "<parameter1>" is null` diagnostic comes from
the distinct camera NPE and is printed by the JVM uncaught handler. Its
creation frames and JVM printer call frames remain separate from the Source
print's creation trace. No native or full-game caller tail equality is claimed.

The warm contrast uses fresh owners and a real call to
TH.a(World,Engine,Font,Player,Settings,.375). JDI observes the Engine store at 7,
post-store10 and complete owner publication27. The two subsequent UE calls
restore H/aS/aT, reset bE, retain TH.i=180, complete DS flag cleanup and balance
the queried matrices/stacks with zero GL errors. All seven primary model Parts
return in each draw; compilation occurs only on the first warm draw. The probe
calls the actual TH frame body directly; it does not execute a complete N World
traversal or repair the failed cold objects.

`observations.json` retains raw UTF8 stdout/stderr with original line endings,
all selected JDI events, actual Throwable snapshots and entered live Code.
`finite.json` selects the cold prefix/escape and warm frame/returns without
changing any captured value. `source-pins.json` inventories 34 original classes;
`line-metadata.json` retains their SourceFile, LineNumberTable, catch-table and
local-variable metadata plus the compiled authored caller. Seventeen entered
original Code hashes agree with the JAR. `jdk-live-code.json` retains four
actual bootstrap JDK17 print/getMessage bodies and their line metadata. The
JDK runtime image/JVM and all 110 execution inputs match before/after.

The 36 Java-free controls reject changed messages/Throwable identity, rewritten
trace frames, missing real print returns, collapsed exceptions, altered catch
metadata, fabricated cold cleanup/return, field/GL repair, pre-cold frame binds,
missing warm Parts and wrong frame publication. They qualify this pinned JDK17
constructor scene and diagnostic path. No general Throwable formatter, cold UI
success, native continuation, pixels, complete startup, unrestricted play,
focus stability or FPS is claimed.

Prepared-1 remains by path/hash in provenance. Its cold source failure and warm
returns were real; its final audit rejected probe-created NoSuchFieldExceptions
from a reflective inherited-field search. Prepared-2 uses a read-only field
enumeration helper and fresh processes. No failed cohort or original class was
repaired or reused. No Throwable, PrintStream, Random, model state or NW backing
is replaced or reset.

```powershell
python -B tests/reference/client/player_preview_caught/packet.py
python -B -m unittest discover -s tests/reference/client/player_preview_caught -p test_packet.py
```

A fresh capture requires a serialized desktop window. Prepare compiles and
pins helpers without opening a Display. Observe runs ordinary/JDI cold and
warm processes in sequence:

```powershell
python -B tests/reference/client/player_preview_caught/capture.py --output .local/player-preview-caught-new
python -B tests/reference/client/player_preview_caught/capture.py --observe --output .local/player-preview-caught-new
python -B tests/reference/client/player_preview_caught/publish.py --capture .local/player-preview-caught-new
```

All seven executed helper/input files were frozen LF before capture.
Publication validators were authored afterward and are hashed separately.
No captured helper was normalized after pinning. The World/Player/Screen
constructor scene is explicit; App.r publication, real E-key dispatch and the
complete World-load caller have separate qualification gates.
