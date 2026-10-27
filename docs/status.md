# Development status

The native C client is pre-playable. It opens a title screen and renders a
fixed-camera generated-world inspection with locally imported resources.
Normal singleplayer World entry, physical input and frame/tick adoption,
dynamic rendering and complete gameplay interactions remain unfinished.

## Current integration

Published `e8fcb3a` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40517903395):
1,115 public/JDK cases, comprising 363 Windows, 376 GCC and 376 Clang cases.
All 1,055 preceding cases remain. Every checkout matches the reviewed
`409c8bc698b7ec30f7d9fce08166f78ea4efb3ae` tree. Formatting covers 1,024 C/header
files with clang-format 18.1.3; controller mirrors and 556 positive/2,011 negative
lifecycle controls pass on each platform. This matrix excludes supplied-original
comparisons.

The new startup/resource owner graph includes the standalone Minecraft
constructor, bitmap fonts, SoundManager construction, selected texture packs,
shared image loading, texture lookups and mutable color arrays. Separate focused
local validation passes 48 logical checks per compiler: 47 CTests and the direct
later-owner regression. Each passes 774 constructor checks and 196,715 array/class
checks; Clang 18 and GCC 13 use ASan, UBSan and leak detection.

The Windows source precedes one POSIX-only unused-parameter annotation found by
Clang; both Linux runs use the corrected source. These are focused owner and
resource results. The executable still uses the existing menu/inspection startup;
normal startup adoption, World entry and the playable loop remain unfinished.

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

The later reader, metadata, CI and documentation proposal has pure parser controls
and read-only admission checks; it has no new native build or hosted matrix.
Enabling the observer option in CI compiles the probes and runs four public pure
checks. Supplied-original profiles stay off. See the [resource observation scope](client/render-engine.md#resource-lookup-observations).

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
