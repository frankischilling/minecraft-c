# Particle draw entries and World-frame continuation

The client connects the real `dn.b(sn,float)` lit-particle entry and
`dn.a(sn,float)` sprite entry to the existing effect manager. They use its
four actual Lists, the same particle Entities and the actual graphics and
Math namespaces. The supported empty paths return from DN and continue the
enclosing World frame. Reached particle and World draw dependencies still
stop that frame at their source invocation.

The [draw API](../../include/mc/client/particles/render.h) returns a retained
call with its source position, completed stores, current particle and typed
failure. [The entry implementation](../../src/client/particles/render.c)
contains the DN loops;
[the pickup draw body](../../src/client/particles/render_pickup.c) implements
the reached `em.a(nw,float,float,float,float,float,float)` prefix. Source
authority is the client archive pinned in the
[binary audit](../reference/binaries.md). The
[CPU particle documentation](particles.md) describes construction, ticking,
insertion and World changes separately.

The [original metadata](../../tests/reference/client/particle_draw/source-pins.json)
pins complete classes and every method. Selected Code identities are:

| Method | Code bytes | SHA-256 |
| --- | ---: | --- |
| DN.b `(Lsn;F)V` | 79 | `1c7cb3bb79b88884eb5dabea52e93c50f3d8d14d9a309d788d9c18f4ee243e83` |
| DN.a `(Lsn;F)V` | 312 | `1dbedcabec40db7d9f244104178a7bae197906ea963c9002213f6d308f3fc4fc` |
| EM.a `(Lnw;FFFFFF)V` | 290 | `8822f2698e49acb9a991f2f54c0f3bc5783f34b4f96e7abfeb632e18d4585720` |

The services tuple supplies the enclosing graphics, Math, Display and TH
namespace owners. Source camera, World and RenderEngine operands still come
from their reached arguments and fields. The World frame supplies its actual
startup TH namespace; passing that namespace does not initialize TH. Its first
particle active use remains EM259.

`dn.b` first reads layer3 and its size. An empty List returns at16 without
reading the camera, another layer, NW, Math, Display or floating controls.
For a nonempty List, NW active use occurs at17. The loop reloads the actual
List size at33, obtains each current row at49 and invokes its concrete draw
at69. A successful loop would return at78. It neither begins a quad batch nor
refreshes the XW camera origins.

`dn.a` reads camera orientation before any List emptiness test. Its five Math
lookups evaluate cosine and sine of yaw, sine of pitch twice, then cosine of
pitch, with the source float operations between those calls. A null camera
fails at1 even when every List is empty. The method then independently
interpolates the three camera coordinates and stores XW `l`, `m`, `n` at
97,117,137. Only after those stores does it inspect layers0,1,2. Empty layers
skip texture and NW work; an entirely empty sprite traversal returns at311.

For a nonempty sprite layer, the source texture is `/particles.png`,
`/terrain.png` or `/gui/items.png`, respectively. Lookup uses the manager's
current source RenderEngine, followed by the real texture bind, NW capture,
quad begin, live-size traversal, concrete draw invocation and NW draw. The
current concrete particle support is the genuine EM pickup particle, whose
normal source layer is3. Other reached virtual draw bodies remain explicit
dependencies. The loop structure does not establish successful sprite draws.

The [origin API](../../include/mc/entity/particles/origin.h) stores XW origins
in the same client Entity namespace as the particles. These are class fields,
shared by effect managers in that namespace. Their default values are +0.
Each ordered source store performs the required superclass active use and
publishes only its own component. A later failure preserves earlier stores.
Passive inspection reads those cells without initializing a class. EM uses
the values present when its draw reaches them, including values left by an
earlier sprite call. A lit call does not substitute its camera position.

EM computes the square of `(age + partial) / lifetime`, reads the picked
actor's current position, and interpolates the target's position using
partial. It adds the source vertical offset to that target position, then
interpolates the picked position toward it using the squared fraction.
Arguments and fields are reloaded in source order; the arithmetic keeps its
binary32 and binary64 operation boundaries. Math floor calls at183,198,205
precede the actual particle World read at211 and brightness invocation220.
A null target therefore fails at53; a null World fails at220 after the
completed interpolation and floor operations.

After brightness returns, EM reads the actual XW origins, subtracts them
from its draw coordinates and publishes the brightness color at256. TH
active use occurs at259 through the supplied actual manager namespace.
The source coordinate narrowing/widening and picked yaw read precede TH's
Entity draw invocation286. That renderer draw is an explicit dependency.
The retained draw currently admits one captured EM World; reaching a different
World in another row stops at its brightness invocation220 with an explicit
dependency.
Failures retain the DN invocation and EM child position, together with the
typed Math, World, graphics, Display or manager cause. Strict floating
controls are checked where arithmetic is reached; unsupported controls and
unqualified numeric operands preserve the completed source prefix.

A draw call owns an independent native occurrence of DN and each reached
particle or service resource. The caller keeps the complete camera, World,
RenderEngine and namespace graph live and exclusive through retirement.
Outputs start null. An admitted failure returns its call for inspection and
checked retirement; it cannot be replayed to resume the source method.
Pending calls block effect-list mutation and terminal manager cleanup.
Retirement consumes only that call's reached native rights. It preserves
List contents, Random state, class fields, NW batch state and GL effects.
A refused cleanup keeps its remaining rights for another cleanup attempt.
Borrowed views expire as their actual owners are consumed.

Texture failures follow the same rule. A successful lookup already consumes
its attempt. A failed draw owns a pending attempt only when that invocation
created it. Cleanup preserves a foreign lookup that was already pending.
The same distinction applies to the World frame's terrain lookups.

The [World-frame API](../../include/mc/client/world/world_frame.h) preserves
the camera captured at48, N at57 and FX at66. Both DN calls receive those
same captured camera and FX identities. The
[frame implementation](../../src/client/world/entity_renderer/world_frame.c)
and its [continuation](../../src/client/world/entity_renderer/world_frame_after_particles.inc)
execute the following source sequence:

| PX position | Reached operation |
| --- | --- |
| 510 | Invoke the actual N Entity traversal with the pooled eye and source frustum. Its positive-delay branch consumes the source delay once. |
| 518 | Invoke the captured FX lit entry. Retire a completed native call before the later same-FX sprite entry. |
| 521,527,535 | Disable standard lighting, invoke a fresh fog setup, then invoke the captured FX sprite entry. |
| 538 through634 | Read a fresh App Hit, then the captured camera's water and Player predicates when required. A qualifying underwater case reaches N's damage/placement overlay at601 before its outline at625. |
| 640 through725 | Set blending, invoke fresh fog646, read the current App RenderEngine for terrain texture673, then follow the current fancy-graphics and ambient-occlusion fields into the fancy branch's translucent submission725. |
| 730 through792 | Read fresh anaglyph settings, restore the selected color mask, invoke N display-list replay786 over the current T/TG batches if the fancy submission returned a positive count, then set flat shading. |
| 805 | The nonfancy branch invokes its actual translucent submission here. |
| 809 through822 | Restore depth writes, enable culling and disable blending. |
| 825 through930 | Evaluate zoom, Player, a fresh App Hit and water in order. A qualifying ordinary selection reaches the damage/placement overlay at897 before the outline at921. |
| 932 | Invoke [precipitation drawing](precipitation.md) using a fresh App World and camera. |
| 938 through957 | Disable fog, execute a fresh fog setup and enable fog. |
| 963 | Invoke the captured N's cloud draw. This child remains an explicit dependency. |

Fog calls at306,324,449,527,646 have distinct retained calls and snapshots.
The completed N510 receipt retires before a later N invocation, leaving its
delay counter and other source fields unchanged. Completed call snapshots
keep scalar results and clear borrowed views after consumption. Captured FX,
N, camera, eye and frustum ownership lasts through their reached children.
Later source App reads remain fresh.

The [N replay entry](../../include/mc/client/world/render_global_submit.h)
uses the same retained-call inspection and retirement machinery as terrain
submission. Its actual `n.a(int,double)` body visits the current TG objects
and returns at29. It does not reselect chunks, reset batches, poll dirty
renderers, read a camera or activate Math. A failed replay retains its source
frame and reached TG cause. Native call retirement ends before any later N
selection invocation.

Selection arguments are evaluated before the N receiver invocation. The
frame reads the fresh Hit, then the actual Player Inventory and its current
Stack through `ix.b()`. Nullable returned Stacks remain nullable, and reached
Hit/Stack arguments receive their own native occurrences. The first missing
selection child is the damage/placement overlay
`n.a(gs,vf,int,iz,float)` at601 or897. The outline
`n.b(gs,vf,int,iz,float)` at625 or921 follows only after that first child
returns. A failure keeps the alpha-test disable already performed by PX.

The loaded native Windows fixture returns from both empty DN paths, executes
the later terrain and selection predicates, and reaches the weather draw at
PX932. That observation establishes this native prefix with the actual
loaded owners. It does not establish composed original/native frame equality,
successful nonempty particle rendering, weather, clouds, hand or HUD drawing,
an anaglyph second pass, or a playable client.

[Existing paired CPU observations](particles-qualification.md) cover their
stated constructor, tick, layer and List behavior. Draw observations must be
assessed by their own reached operations. An empty lit return executes no GL
work; an empty sprite return also exercises orientation and origin stores.
A null-target or null-World failure establishes only the operations before
that failure. Native ownership, cleanup, floating-control and loaded-frame
checks are separate from original draw correspondence. Qualification of
successful nonempty draws requires the returning concrete renderer and its
actual World, texture, NW and Display dependencies.

## Qualification

All 45 selected production/test targets build on Windows MSVC Debug and Linux
Clang18 ASan/UBSan. All 54 Windows CTests pass in 79.27 seconds and all 50 Linux
cases pass in 103.43 seconds, without failures, skips or sanitizer findings.
All 2,313 C/helper/build inputs remain unchanged through both builds and tests.
Formatting checks 1,630 C/header/fragment files, including the frame continuation.

Each host passes 661 particle draw checks and 44 shared-origin checks. They
use genuine native constructors and source add/clear APIs, ten raw partial
values, actual null/failure precedence, List/Random passivity and reached
namespace/Entity holds. Native camera pose inputs separately cover orientation,
ordered origin stores and sharing across two actual effect managers. These
controls do not compare peer constructor entropy or complete particle draws.

The Windows device test passes 16,775 checks, including the replay extension:
genuine RAW N fails at T length9 before unused partial/FP work; SOURCE N visits
four genuinely inactive TGs and returns29; canonical configure/append and an
authored display list then prove two actual active replays. Buffer contents,
flip/replay cursors, matrix restoration, color effects and absence of chunk
selection/counter/reset work are checked. This device execution is separate
from the asset-independent tests run on Linux.

Seven Windows World-selection modes preserve pre-existing save/resource bytes.
Both loaded modes preserve all 72 original input histories. The terminal mode
passes 1,165 checks and reaches exactly PX932. The inspected frame-prefix image
shows authored grass terrain and Sky without HUD. These are native composition
controls; the conditional terrain replay has its separate device coverage.

The [optional original observer](../../tests/reference/client/particle_draw/README.md)
passes three fresh-JVM profiles: empty DN, genuine pickup/null target, and
genuine pickup/null World. Its 124 logical rows and 30 complete exception
stacks support 40 selected outcome projections over the same ten partial bits.
All 505 actual inputs remain unchanged. The finite fixture records return16,
null camera1, DN69 to EM53, and DN69 to EM220, with source PCs mapped separately
from complete Code. It also records per-process passivity. There is no automated
C comparison or constructor/private-field/method-local/pixel equality credit.
