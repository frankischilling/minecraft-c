# Development status

The native C client is pre-playable. It opens a title screen and renders a
fixed-camera generated-world inspection with locally imported resources.
Normal singleplayer World entry, physical input and frame/tick adoption,
dynamic rendering and complete gameplay interactions remain unfinished.

## Current integration

Checkpoint `37dc83f` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40544932686):
1,127 public/JDK cases, comprising 367 Windows, 380 GCC and 380 Clang cases.
All 1,115 preceding cases remain, with four observer checks added per compiler.
The actual checkout `5b671b26` matches the checkpoint's
`fdda932e6f3578c1ec37cde5d0762cbc2601a3ed` tree; all 2,027 archived source blobs
match. Independent log readback confirms every Start/Passed pair, with no skips
or failures. Formatting covers 1,027 C/header files with clang-format 18.1.3;
controller mirrors and lifecycle controls pass on each platform. This matrix
excludes supplied-original comparisons and later startup source proposals.

## Normal startup integration

The separate [normal startup owner](client/normal-startup.md) now constructs
Display, the default game directory, save-format manager, Settings, selected
texture pack, RenderEngine and bitmap Font in source order. It loads the three
water/grass/foliage color maps and calls their real setters. The next reached
boundary is EntityRenderer construction. The default executable continues to
use its title/inspection path; this owner is not a playable entry point.

Published checkpoint `33c777c` has a [failed hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40589053675).
Windows stops on an older MSVC flexible-array warning. Each Linux compiler
passes 400 of 401 tests; the SDK transport test fails during leak checking.
Formatting passes. The corrections use a scoped MSVC compatibility guard and
ordinary Linux X11 linkage. The original transport test passes all 37 checks in
a fresh sanitized Clang build with the latter setting. A complete hosted result
for the corrections is still pending.

The coherent local source builds ALL and the client on MSVC Debug, Clang 18 and
GCC 13. Windows passes 67 distinct selected CTests. Clang and GCC each pass 74 distinct
selected CTests across 81 executions.
Linux uses ASan, UBSan and leak detection. These focused results retain 774
constructor checks and 196,715 array/class checks; they are separate from the
preceding hosted matrix and do not establish a full local suite.

One real Windows startup process exited zero after 166 authored assertions,
including Font/color setup and retirement, and left only its three empty game
directories. The window qualification stopped afterward on its capacity guard.
Its reader also omitted two genuine constructor banners; the remaining six
window profiles did not run. No seven-profile result or original comparison is
claimed from that attempt. World entry, input/tick adoption, dynamic rendering
and gameplay remain unfinished.

## Resource observer source successor

A separate observer producer cohort builds ALL and passes six selected CTests
plus eleven native-only profiles on MSVC Debug and sanitized Clang 18/GCC 13.
It retains 774 constructor and 196,715 array/class checks. These finite profiles
preserve unfinished native frontiers and are separate from a complete local suite.

One retained Windows CPU default attempt now has independently reviewed origin,
capture, input, source and product admission. All seven supplied-original color
rows match, including full arrays and cache identity. The initial reader refusal
remains retained; exact added JDK class proof and a two-pattern log-tag correction
admit the same captured attempt without rerunning the game. ZIP, cold-static and
original texture/GL observations remain outside that result.

The later reader and metadata policy retain their pure parser controls and
read-only admission checks, separate from the earlier native producer. At
`37dc83f`, CI compiles both observer probes and passes four public pure checks
per compiler. Supplied-original profiles stay off; this hosted result does not
rerun the seven-row original attempt or the eleven-profile native producer.
See the [resource observation scope](client/render-engine.md#resource-lookup-observations).

## Preceding local cohort

The preceding Timer, Minecraft class/worker, RenderEngine, Biped and World/Player
cohort passed 1,163 local CTests: 393 Windows MSVC Debug cases and 385 cases on
each Clang 18/GCC 13 Debug build with ASan, UBSan and leak detection. Every
preceding 389/381/381 case remained enabled. None failed, skipped, was disabled
or remained unrun. All three profiles exercised real OpenGL startup, title and
fixed-camera World inspection.

The tested cohort contained 1,885 files and 1,643 non-Markdown inputs. All 936
C/header files passed clang-format 18.1.3. Source and configured products remained
unchanged through execution; all 283 Windows executables kept their default
1 MiB stack. Documentation updates preserve non-Markdown inputs and pinned
reference text.

[Ordered Timer construction](client/timer.md) passes 1,267 checks per compiler,
including retained clock-failure prefixes and independent render-partial storage.
[Minecraft class activation and the Timer hack worker](client/application-owner.md)
pass 225 checks and 70 controls in each normal/optimized Python run. Five original
class/worker projections retain their declared prepared-receiver scope. These
owners are startup prerequisites; normal startup and World entry remain unfinished.

[RenderEngine construction](client/render-engine.md) passes 8,333 CPU and 223
device checks, with 55 separate texture-trace checks. Fresh original CPU runs
match all 4,096 ARGB pixels and ten glyph metrics on Windows and Linux; the
selected original GL profile has ten exact Windows rows. [Biped construction](client/biped-construction.md)
passes 4,381 checks and 435 allocation prefixes, with 838 fresh original records
per compiler. Normal application publication, animation and drawing remain
unfinished.

[LocalPlayer NBT](entity/player-nbt.md) now executes the actual Living, Player
and Inventory read/write hooks, first-Player World metadata save and saved
Info.Player attachment. Each compiler passes 490 NBT, 110 coordinate, nine
diagnostic and 169 publication checks. Thirty-three original cases match 1,302
normal tag nodes. Windows runs fresh originals; current Linux executables match
the retained Windows observations. The separate wake93 prefix has 37 records,
including 33 tag nodes. Full wake remains unfinished.

[Lava CPU animation](../tests/reference/client/lava_animation/README.md) passes
132,659 checks per compiler. Fresh Windows and Linux original CPU runs match all
348 records across 23 cases and 31 calls. That isolated profile did not include
application publication, texture registration or GPU upload.

The [singleplayer controller](client/singleplayer-controller.md),
[camera methods](client/first-person.md),
[World Entity updates](world/entity-update.md),
[Player spawn/reset](entity/player-spawn-reset.md) and
[weather method](world/weather.md) retain their specified native and original
comparisons in this combined matrix. These are finite method/owner results.
They do not establish a connected playable session. Full evidence and
reproduction options are in the [testing record](testing.md#current-public-runtime-integration).

The preceding published `4a6b805` [hosted checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/40464703757)
passed 1,055 public/JDK cases: 345 Windows and 355 per Linux compiler. Its hosted
scope and the 1,163-case local cohort remain separate from the current results.

## First playable singleplayer session

The next end-to-end milestone is one World that can be loaded, explored with
Player controls, changed through block actions, saved and reloaded. It requires:

- Complete normal startup and World/Player/view publication in source order.
- Connect physical input, frame timing and World/Player ticks to the real owners.
- Update rendered terrain as Chunks and Blocks change, with the live Player camera.
- Complete targeting, mining, placement and the reached inventory interactions.
- Verify native Windows/Linux play and persistence across a complete session.

Existing terrain, population, lighting, storage, Player, inventory, audio and
rendering modules have separate qualified scenarios in the [parity matrix](parity.md).
Those results remain narrower than complete subsystem or gameplay parity.

## Remaining deliverables

Multiplayer interoperability, the standalone C dedicated server, extended play,
all dimensions and remaining presentation/gameplay behavior still require
implementation and verification. Account authentication is the only approved
gameplay exception. See the [roadmap](roadmap.md) and [original inventory](inventory.md).

Earlier checkpoints remain in the [client runtime history](history/client-runtime.md),
[population history](history/population.md) and [integration history](history.md).
