# Texture-effect construction and registration

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

## Verification scope

Native controls cover constructor/default state, CPU updates, concrete resource
images, registration/allocation prefixes, live World/spawn dependencies and
strict ownership. The genuine Windows Session returns from all nine calls before
RenderGlobal/particle construction and the resource-thread boundary664. The
selected Linux cohort covers the affected CPU and device prerequisites.
The [testing record](../testing.md#startup-scene-construction-and-typed-settings-save)
keeps these scopes separate from earlier water/lava original comparisons.

Frame-driven texture updates, subimage upload, the complete render loop and
broader original numerical/pixel correspondence remain unfinished. Registration
and finite native checks do not establish texture animation in playable sessions.
