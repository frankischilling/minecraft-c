# Texture effects and atlas updates

Normal startup registers nine actual effects on its live RenderEngine, in source
order: constructor-owned lava, constructor-owned water, portal, compass, watch,
flowing water, flowing lava, fire orientation0 and fire orientation1.
The interface is [effect.h](../../include/mc/client/texture_effects/effect.h).

Each registration captures the Engine before evaluating its argument. New
effects retain their own constructor prefixes; the original lava/water arguments
remain the same App-owned objects. Registration appends the actual occurrence
to the Engine's List, then invokes its first update. A null receiver is reached
after argument evaluation. A failed update preserves the appended occurrence
and earlier Random/array/field effects.

## Shared services and lifetimes

The implementations use the existing Java Random/Math, Registry, image, String
and World owners. Resource images come from the actual class provider and keep
their stream/decoder failure distinctions. Water and lava reuse their existing
animation storage and kernels. Compass/watch read their actual borrowed App
fields, including the current World, rather than an inspection-session copy.
The shared spawn helper also serves the existing Player construction path.

Native effects and Engine List entries own separate occurrences. The wrapper
keeps failed constructors and first-update failures inspectable. Compass/watch
take native App borrows for their real storage dependency; external effects
therefore keep the App alive without a counted App/Engine ownership cycle.
Engine cleanup releases its List occurrences before App retirement checks for
surviving borrows. Native retirement does not perform another update or reverse
source List/field publications.

The capability-checker `cx` has its own cold/active class namespace and real
Display capability observations. Startup publishes it as `S477` before binding
SoundManager and registering effects. Unsupported native device/profile paths
remain explicit failures.

## Frame updates and uploads

The actual `ji.a()V` walks the retained Engine List in order. For each occurrence
it copies the live anaglyph setting, runs that effect's update, fills the shared
byte buffer, resolves and binds the actual atlas texture, and uploads the tile.
Repeated tile regions and the source-enabled four mipmap levels use the same
updated pixels. A second pass handles effects with a separate texture ID only
after every first-pass occurrence has returned. Duplicate List entries update
and upload repeatedly; failures retain earlier field, buffer and GL effects.

The [default menu runtime](menu-runtime.md) invokes this path over the nine
registered effects. It uses the Engine's real Settings, selected Pack and class
resource identities. No diagnostic texture list supplies the frame updates.

## Verification scope

Native controls cover constructor/default state, CPU updates, concrete resource
images, registration/allocation prefixes, live World/spawn dependencies and
strict ownership. The genuine Windows Session returns from all nine calls before
RenderGlobal/particle construction. The
selected Linux cohort covers the affected CPU and device prerequisites.
The [testing record](../testing.md#startup-scene-construction-and-typed-settings-save)
keeps these scopes separate from earlier water/lava original comparisons.

The focused Windows `client.effect_upload` test passes, along with GuiIngame,
source screen events and a 100-frame, 32-tick native menu run. This finite native
result does not qualify original GPU pixels or animation in playable sessions.
The preceding complete Windows selection passes all 463 CTests. Native controls also update the genuine
nine-effect startup list with 15 terrain/items atlas uploads. Four-level mipmaps
remain unexecuted. Both Linux effect-upload cases pass at the hosted
[`de4290b` checkpoint](../testing.md#hosted-runtime-checkpoint-de4290b).
Original numerical/pixel correspondence retains its separate scope; the
complete gameplay render loop is unfinished.
