# Pause save and title: source order

This record covers static inspection of the verified Beta 1.7.3 client. It does
not claim an executed original pause-to-title session. The original client JAR
SHA256 is `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
[Code pins](class-code-pins.json) identify the seventeen inspected method bodies
by class hash, descriptor, length and Code hash. They contain no original class
or bytecode bytes.

## Save and quit

`oz.a(Lke;)V` handles Button ID1 by capturing App.I at41, initializing the
statistics class at44 and incrementing the selected statistic at48. The local
World branch calls `Minecraft.a(NULL fd)` at76. Only after that call returns
does it allocate a new `fu` at83, construct it at87 and display it at90. The
startup title screen is a different object. The remote disconnect branch has
separate dependencies.

The one- and two-argument `Minecraft.a` overloads supply the canonical empty
String and null Player to `a(Lfd;Ljava/lang/String;Lgs;)V`. That body performs
these operations in order:

| Site | Operation |
| --- | --- |
| 4, 11 | Call statistics `xi.b`, then execute a fresh `xi.c` save. |
| 16 | Clear the actual view Entity `i`. |
| 24, 33 | Start Loading, then call its actual message operation. |
| 46 | Execute the null streaming-source branch. |
| 64 | Save the previous nonnull World with the same nullable Loading argument. |
| 69 | Publish the target World, which is null for this action. |
| 355, 358, 363, 366 | Clear Player `h`, request advisory GC, set `P=0`, return. |

The null-target branch skips RenderGlobal World replacement at220 and particle
World replacement at235. Both retain their old World references. Native
retirement must account for those references; it cannot invent either source
call or restore cleared App fields.

`fd.a(Lyb;)V` delegates to `fd.a(true,yb)`. Saving level progress at20 precedes
metadata26; Saving chunks progress36 precedes provider save47. Concrete `mk.b`
ignores its String argument, sets its own flag and reloads its stored title.
The client provider's `ok.a(ZLyb;)Z` does not read the progress argument. Its
forced save still applies each Chunk's time/dirty eligibility test.

## Native verification boundary

The implementation is described in
[client saving](../../../../docs/client/world-saving.md). Its visible fixture
uses genuine native Frame/Canvas hosting, SDK input, source World construction
and independently decoded files. The [hash-only native qualification record](native-qualification.json)
records the current Windows normal Tiny leave and session-failure/cleanup
control, including source/helper/product and capture hashes. Its declared
pre-saved statistic and Tiny setting do not qualify fresh-player notices or
default-Far playability. The session case retains native `fm.b:42`; it throws
no `us` and creates no title. Those native observations remain separate
from the static source record and the earlier prepared
[Pause construction observations](../pause_screen/README.md).

Complete original/native pause-to-title caller comparison, ordinary World
reselection, remote disconnect and extended play need separate evidence.
