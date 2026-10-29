# Systems and subsystems

The [LocalPlayer NBT path](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-nbt.md)
dispatches through actual Entity, Living, Inventory and Player owners. Metadata
saving holds the first selected Player; saved Info.Player attachment clears its
tag only after complete load success. Complete selected child diagnostics share
a tagged late-hook union. This reduces Windows stack use without truncating
causes or changing the default 1 MiB executable reserve. Full wake and normal
gameplay adoption remain unfinished.

The [ordered Timer](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/timer.md)
keeps render partials separate from its accumulator and samples real clocks in
source order. The [Minecraft class/worker owner](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/application-owner.md)
holds the static arrays and actual atomic J/o, with native cancellation/join
separate from source stores. The standalone constructor and retained resource
owners have separate [native coverage](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md).
The [normal startup owner](Startup) now connects Display, the game directory,
Settings, selected Pack, RenderEngine, Font, color maps, EntityRenderer and
RenderManager on the same App. It continues through statistics, input, texture
effects, RenderGlobal, particles and resource-thread startup. The default
no-connection path now constructs GuiIngame and the main menu, publishes the
screen, saves statistics and completes scaled screen initialization. That
source startup method returns. The bounded probe stops at `Minecraft.run`32;
the default [menu runtime](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/menu-runtime.md)
continues with canonical Timer/input owners, no-World ticks and a drawing bridge
that reads actual source menu fields and checks resource/Font identities. Quit
clears the App run flag. Singleplayer now constructs the genuine
[World-selection graph](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/world-selection.md):
real SaveFormat metadata and stable sorting, scrolling rows, selection during
drawing and same-parent Cancel. Synchronous publication keeps the original
input receiver alive through its trailing input. Retained old RQ occurrences
keep App dependencies live through BUSY teardown. Select constructs the actual
controller and invokes the actual retained World-load call. Existing-save
loading constructs the canonical source World, completes all 289 terrain
samples and publishes its real Player/input before renderer attachment at 220.
Version-zero conversion stops at 52. Empty, populated, scrolling and double-click native Windows profiles
pass. All 475 selected Windows CTests and formatting for 1,379 files pass.
Full source run/PX rendering, shutdown
and gameplay adoption remain unqualified.

The save manager uses the same shared MathHelper class owner as the App. Its
DateFormat captures the actual default profile, including for an empty List.
The supported English/en_US, UTC/US Eastern and UTC years 1967..2099 profile
matches 2,926 JDK17 vectors on Windows and Linux; other profiles fail explicitly.
[Build and tests](Build-and-tests) separates these focused results from the
preceding complete hosted scopes.

Linux graphical CI uses one owned Xvfb server for its four CTest workers.
A private pinned-SDL reproduction fails during init/window/quit repetition
with default last-client resets, while three `-noreset` runs complete all
2,400 window cycles. CI now disables that automatic reset without changing
the default screen or test selection. SDL refusal diagnostics remain real
failures. [Build and tests](Build-and-tests) records the matched controls,
the distinct hosted refusals and the following exact `9d83665` green checkpoint:
all 1,362 selected Windows/GCC/Clang tests pass without skips, with unchanged
test names and matching trees. That checkpoint predates source World loading.

The runtime uses C17 libraries with explicit ownership. The [architecture](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/architecture.md)
describes the module boundaries; [parity](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md) records the qualified
scenarios and remaining behavior.

The [active Player entry](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-active-tick.md) composes the Player
and Living wrappers, local action, Inventory/Input updates, travel, nearby
contact, animation and statistic dispatch on the same LocalPlayer. The
[application](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/player-statistics.md) owns its Session, statistics
writer and achievement notice. The outer Timer services the writer separately.
[Movement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-movement.md), [travel](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/living-travel.md)
and [environment](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/environment.md) retain their own interfaces and
failure diagnostics.

[Item pickup](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/item-pickup.md) runs Inventory insertion, achievements,
sound, the concrete Player notification and the later Item death check.
[Pickup particles](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/particles.md) own em construction/ticking and the
four dn effect Lists. The application captures its manager before constructing
the effect and checks a null manager afterward. The selected manager startup
body is implemented and adopted by the reached normal startup prefix;
particle drawing remains unfinished. [Item ticking](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/item-tick.md) supplies the actual
Item environment, push-out, movement, friction and aging body. The
[client World Entity pass](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/entity-update.md)
now invokes genuine Player/Item updates through live lists, removals, Chunk
membership and retained riding actors. The enclosing World scheduler and normal
application ticking remain unfinished.

[Spawn/reset](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-spawn-reset.md)
performs the real Player size/collision search before health/death stores. The
[weather descendant](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/weather.md)
updates live Info timers, toggles and rain/thunder values in source order.
Neither completes application World entry.

The [singleplayer controller](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/singleplayer-controller.md)
constructs genuine LocalPlayers from the live Session and World provider,
preserves failed candidates and applies yaw preparation. Its World and final
Player hooks use the actual empty bodies. Selected existing-save loading now
publishes the actual App Player and movement input. View publication, controller
updates, mining, item use and complete World entry remain unfinished.

The water owner has its CPU animation body on the existing arrays and shared
Math profile. Its [comparison guide](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/tests/reference/client/water_animation/README.md)
records the finite raw-array/RGBA scope. The reached normal startup now registers
all nine effects and the menu invokes their real atlas-update path. The isolated
comparison does not establish GPU correspondence. [Notice/model ownership](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/achievement-notice.md) is also
separate from notice and Item drawing.

[World rendering](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/world-renderer.md) remains a fixed inspection of a
constructed GAME World with leased CPU meshes and owned GL buffers. It does not
run the Player/World gameplay loop. The separate
[first-person camera](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/first-person.md)
reads genuine Player state and late App Settings/World fields; finite geometry
can consume those matrices. Normal App h/i publication and render scheduling
remain unfinished. [Lighting](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/light-propagation.md),
[Player publication](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/player-publication.md), [World queries](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/queries.md),
[storage](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/storage.md) and [saving](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saving.md) have their own
source-order and lifetime contracts. [Input attachment](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/player-input.md)
retains the real MovementInput owner and live Settings bindings.

[Crafting](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/inventory/crafting.md) owns the concrete inventory, grid/result
and Slot graphs. [Registry](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/block/registry.md) and [Item](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/item/registry.md)
classes retain shared identities and initialization prefixes. [SavedData](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saved-data.md)
keeps named MapData, ID counters and mutable color handles separate from Store
and WorldInfo lifetimes.

The [RenderEngine](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/render-engine.md) separates CPU constructor/font state from actual Display resources.
[Biped construction](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/biped-construction.md) shares concrete model geometry without aliasing Notice's owner.
Both retain their scoped native/original comparisons. At checkpoint `37dc83f`,
the hosted Windows/GCC/Clang matrix also compiles the resource lookup probes and
passes four pure observer checks per compiler. The separate earlier native
producer and retained seven-row Windows CPU original result keep their own
source/admission scope. Supplied-original profiles and later startup source
proposals are outside that hosted result. The newer normal startup now adopts
the real App resources; its scope is recorded separately above.

The [OpenAL backend](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/audio-backend.md) owns real devices, caches and
workers; [sound resources](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/sound.md) retain streams and decoder state.
Normal source startup binds the real SoundManager and scans local resources.
Original device correspondence and complete gameplay audio remain unfinished.
The shared simulation and server graph initializes no graphics or audio device.
See [dependencies](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/dependencies.md) for source pins and notices.

Application World entry, complete World time/block scheduling, Map tracking and
terrain updates, remaining wake/hurt/GUI operations, fog and dynamic World drawing still prevent a
playable client. Dedicated-server startup and ticking remain separate work.
[Test scope](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md) distinguishes the current full integration
from earlier full matrices and isolated module comparisons.

<a id="save-unload-and-retirement"></a>
<a id="normal-generator"></a>

The [former architecture page](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-architecture.md) retains earlier
technical checkpoints, including [save, unload and retirement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-architecture.md#save-unload-and-retirement)
and the [Overworld generator](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-architecture.md#normal-generator).
