# LivingEntity hurt and color overlays

LivingEntity rendering now redraws hurt and dying mobs with the original red
tint. A separate packed-color pass handles Creeper flashing. The two passes
run in source order when both apply; neither replaces the ordinary model draw.

The hurt pass uses the actual brightness as red, zero green and blue, and
alpha 0.4. The packed-color pass extracts all four ARGB components and divides
each by 255. Only the alpha byte selects that pass: white RGB with zero alpha
does not trigger an overlay.

Each pass redraws the current primary model, then calls the four secondary-pass
predicates again. Spider eyes and Slime shells execute their real predicates;
colors are reapplied after a successful predicate before its secondary draw.
Creeper uses its distinct always-false overlay predicate, so it never enters
the ordinary powered-texture pass through this route. The display backend
supports actual `GL_EQUAL` depth testing. Completion restores `GL_LEQUAL`,
disables blending and enables alpha testing and textures.

The implementation keeps the original field-read and call order, including
fresh model fields after color calls. Model arguments use the original float
subtractions and grouping. Failed calls retain their reached state; cleanup
releases native ownership without pretending that later drawing or GL restores
ran. An injected native floating-point fault stops at the primary yaw
subtraction before the overlay model invocation and retains the real prefix.

## Validation

Seven prepared native profiles cover Spider and Slime hurt passes, Squid death
tint, Creeper packed color, both overlays, neither overlay and negative timers.
Three native controls cover zero-alpha white RGB, visible hurt pixels and a
floating-point failure during a real depth-function callback. Authored textures
and Chunks are generated locally; ordinary tests need no original files or JVM.

Windows MSVC Debug and Linux Clang18 ASan/UBSan pass the direct profiles and
controls. The original comparison pairs 209,478 typed checks, 5,769 identity
relations, eight model receivers and 48 model-argument words per host. The
pixel control changes 5,870 pixels toward red while preserving
59,666 pixels. This checks visible native rendering, not original pixel parity.
Seven graphical CTests pass on each host, covering the overlay, existing Mob,
animal, creature and remaining-Mob draws, GL setup and SDK window creation.
Windows menu and World-frame regressions also pass. The final loaded Windows
Zombie run reaches the missing fire billboard body after progressing past
the overlay. Earlier successful loaded runs retain their own scope; general
Canvas/focus transitions remain unfinished.

The unchanged original has a separate seven-profile ordinary run and read-only
debugger capture. All seven return without GL errors. Actual equal-depth list
replays number 22, 5, 9, 6, 12, 0 and 0; the native backend submits six triangle
batches for each original six-face model-part list. Model arguments, pass
predicates, current state, alias relations and per-process stream invariants
are compared within the declared packet scope. Constructor entropy stays raw.

Current normals after array draws differ across the recorded Mesa and NVIDIA
environments. Windows comparison uses a separate matching-device ordinary
original observation for those normals only. Linux compares with the primary
Mesa capture. Neither result substitutes a tolerance for a normal mismatch.
Prepared pixel comparisons remain separate from the typed/model qualification.

See the [reference packet](../../tests/reference/client/living_overlay/README.md)
for capture chronology, audit inputs and comparison limits. The source body is
in `render/living_draw/overlay.c`; it is shared by the admitted Living renderers.
Damage, death/drop behavior, powered Creeper rendering, fire rendering, complete
Player rendering and a playable single-player session remain separate work.

## Default framebuffer

The SDK `Display.create()` path uses its actual default PixelFormat request:
alpha0, depth8 and stencil0. The native staged Display now makes that request;
diagnostic RGBA hosts retain their separate alpha8/depth16 request. The overlay
fixture explicitly selects the SDK framebuffer request for its genuine native
host, without publishing a fabricated SDK or Canvas object.

The separate ordinary SDK probe on the recorded NVIDIA device observes RGB8,
no stored alpha, depth24, stencil0 and double buffering. All color write masks
are true. Blending still reads alpha255 because the framebuffer has no alpha
storage. The native observed format now matches those attributes.
[Derived evidence](../../tests/reference/client/living_overlay/framebuffer-evidence.json)
and its authored probe retain the exact tools and external artifact fingerprints.

Seven Windows baseline images match the original byte for byte. Five of seven
drawn images also match exactly after the framebuffer correction; both Spider
images retain one RGB pixel difference. The full seven-image pixel comparison
therefore remains unqualified. No channel is discarded and no image tolerance
is used to turn those differences into a pass.

The final Linux images also match all seven baselines, but six drawn images
retain differences: 51 pixels for each Spider, five for Squid, and fourteen
for each Creeper scene. Slime matches exactly. These Linux draw differences
remain unresolved and are excluded from the passing typed/model claim.
