# Systems and subsystems

The [LocalPlayer NBT path](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-nbt.md)
dispatches through actual Entity, Living, Inventory and Player owners. Metadata
saving holds the first selected Player; saved Info.Player attachment clears its
tag only after complete load success. Complete selected child diagnostics share
a tagged late-hook union. This reduces Windows stack use without truncating
causes or changing the default 1 MiB executable reserve. Full wake and normal
gameplay adoption remain unfinished.

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
body is implemented; normal RenderEngine (ji) startup and particle drawing
remain unfinished. [Item ticking](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/item-tick.md) supplies the actual
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
Player hooks use the actual empty bodies. Application Player/view publication,
controller updates, mining, item use and complete World entry remain unfinished.

The water owner has its CPU animation body on the existing arrays and shared
Math profile. Its [comparison guide](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/tests/reference/client/water_animation/README.md)
records the finite raw-array/RGBA scope. Texture registration and upload remain
unfinished. [Notice/model ownership](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/achievement-notice.md) is also
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
Both pass the current full Windows/Linux matrix; normal App adoption remains unfinished.

The [OpenAL backend](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/audio-backend.md) owns real devices, caches and
workers; [sound resources](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/sound.md) retain streams and decoder state.
Application audio startup and original device correspondence remain unfinished.
The shared simulation and server graph initializes no graphics or audio device.
See [dependencies](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/dependencies.md) for source pins and notices.

Application World entry, complete World time/block scheduling, Map tracking and
terrain updates, remaining wake/hurt/GUI operations, fog, normal RenderEngine startup and drawing still prevent a
playable client. Dedicated-server startup and ticking remain separate work.
[Test scope](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md) distinguishes the current full integration
from earlier full matrices and isolated module comparisons.

<a id="save-unload-and-retirement"></a>
<a id="normal-generator"></a>

The [former architecture page](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-architecture.md) retains earlier
technical checkpoints, including [save, unload and retirement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-architecture.md#save-unload-and-retirement)
and the [Overworld generator](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-architecture.md#normal-generator).
