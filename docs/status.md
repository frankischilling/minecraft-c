# Development status

The native C client is pre-playable. It opens a title screen and renders a
fixed-camera generated-world inspection with locally imported resources.
Normal singleplayer World entry, physical input and frame/tick adoption,
dynamic rendering and complete gameplay interactions remain unfinished.

## Current integration

The combined RenderEngine, Biped, Lava animation, LocalPlayer NBT, controller,
camera and World update source passes 1,151 local CTests: 389 Windows MSVC Debug
cases and 381 cases on each Clang 18/GCC 13 Debug build with ASan, UBSan and leak
detection. Every preceding 376/348/348 case remains enabled. No case failed,
skipped, was disabled or remained unrun. All three profiles exercise real OpenGL
startup, title and three fixed-camera World inspection captures.

The tested cohort contains 1,867 files and 1,627 non-Markdown inputs. All 926
C/header files pass clang-format 18.1.3. Source inventories and configured build
products remain unchanged through the suites; all 281 Windows executables keep
their default 1 MiB stack reserve. Documentation edits preserve non-Markdown
inputs and reference text bound by the comparison packages.

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
348 records across 23 cases and 31 calls. Application publication, texture
registration and GPU upload remain unfinished.

The [singleplayer controller](client/singleplayer-controller.md),
[camera methods](client/first-person.md),
[World Entity updates](world/entity-update.md),
[Player spawn/reset](entity/player-spawn-reset.md) and
[weather method](world/weather.md) retain their specified native and original
comparisons in this combined matrix. These are finite method/owner results.
They do not establish a connected playable session. Full evidence and
reproduction options are in the [testing record](testing.md#current-public-runtime-integration).

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
