# Original GuiIngame drawing

`HudObservation.java` executes the unchanged original `uq.a(FZII)V` in a real
Windows LWJGL2 context. All 26 calls reach return PC 2101. The fixture covers
an empty inventory, hotbar and selected slot, crosshair, hearts and the original
empty-chat cleanup. Fourteen profiles also execute the actual vignette body.
No source class or method is replaced, and no child return is supplied.

`source-pins.json` records complete class hashes and selected method Code
identities from the verified original archive. `operation-map.json` records
field and invocation PCs. These are static bytecode maps, not JVM instruction
traces. Full `javap` research stays in ignored local artifacts; no original
classes, assets or decompiled source are distributed here.

## Source call order

The main method constructs ScaledResolution at PC 25, captures FontRenderer at
48 and calls actual `px.b()V` at 60. It enables blending at 66, queries actual
`Minecraft.u()` at 69 and conditionally invokes vignette at 91. It then reads
helmet slot 3 at 105, conditionally invokes pumpkin at 147 and interpolates the
Player portal fields before the portal helper at 200.

The ordinary path sets white color at 207, obtains `/gui/gui.png` at 222 and
sets inherited `ub.k=-90` at 243. Actual inherited textured-rectangle calls
draw hotbar at 266 and selection at 302. It obtains `/gui/icons.png` at 317,
sets blend factors 775/769 at 335 and draws the crosshair at 359. It captures
hurt-blink state, current and previous health, then calls actual Random.setSeed
at 447 with the **wrapping int** product `uq.h*312871`, widened to long afterward.

The real controller `ob.d()` at 457 returns true, so the original armor/heart
loop runs. Empty armor gives zero. Health at most 4 consumes one actual
Random.nextInt(2) at 635 per heart. Previous-health blink and current-health
half/full-heart branches remain source-controlled. Actual immersion at 787
returns false for the dry Player. The method enables rescale normal at 951,
pushes/rotates at 954/962, runs actual lighting `u.b()` at 965 and pops at 968.
All nine actual slot helpers at 1015 return at their own PC 22 for null Items.
Actual `u.a()` at 1024 disables lighting; rescale normal is disabled at 1029.

Sleep timer at 1039, debug setting at 1142 and record timer at 1652 skip their
optional bodies. Empty chat still enables blending at 1824, sets factors
770/771 at 1833, disables alpha at 1839, pushes/translates at 1842/1853 and
reads the genuine empty List at 1865. The source pops at 2086, enables alpha
at 2092, disables blending at 2098 and returns at 2101.

## Actual and prepared operands

The original WY/in-memory World, Player, base controller `ob(app)` and UQ
constructors run normally. App uses an actual KQ instance with its constructor
bypassed. PX and Engine are actual original instances with their constructors
bypassed and reached fields prepared explicitly. The real private static
`Minecraft.a` is bound to that App. Actual `Minecraft.u()` reads its settings,
so the second half of the profiles takes the real fancy-graphics branch.

Engine's actual cached-texture map contains three actual authored GL textures:
`/gui/gui.png`, `/gui/icons.png` and `%blur%/misc/vignette.png`. All are 256x256
opaque RGBA procedural images specified in the observer, using nearest filters.
The real `ji.b(String)` cache-hit method executes. No original image is used.
FontRenderer is null and is captured normally; this finite path makes no font
call because debug, record and chat content are absent. The real inventory and
armor arrays contain their constructor-created null slots.

The Player is at (8.5,200.5,8.5), with portal B/C zero, bE zero and sleep timer
zero. World seed is 23. The actual acquired Chunk has air, SkyLight 15 and
BlockLight 0 at (8,127,8). Each profile runs the original World time setter and
skylight-subtraction update. Fast graphics uses time 6000, yielding actual
World.f=0; fancy graphics uses time 18000, yielding World.f=11.

In the first 24 profiles, actual `sn.a(F)F` brightness returns zero: its World bounds query rejects
the above-height bounding box, and the source returns actual bE=0 at PC 161.
These vignette profiles qualify the real fallback path. Each fancy profile starts with
UQ.c=.75; the original vignette mixed float/double smoothing changes it to
raw bits `3f40a3d7` (.7525). Each fast profile starts with c=1 and preserves it.

Two appended profiles use the actual Entity position setter to place the Player
at (8.5,120.5,8.5). They retain fancy graphics, health 20, previous health 20,
hurt blink 0, selected slot 0 and partial tick .375, one profile per source size.
The actual World bounds query over the real resulting AABB returns true. The
exact source brightness sample is (8,120,8); that actual Chunk cell is prepared
as air, SkyLight 15 and BlockLight 0. Real World time 18000 and its update retain
World.f=11. Both actual `sn.a(F)` and actual `fd.c(8,120,8)` return raw bits
`3e044444`, and the source vignette changes c from .75 to `3f404f30`.

Profiles iterate fancy 0/1, source size 0/1, health case 0/1/2, then partial
tick .375/1. The first 24 indices are stable; the in-height cases are appended
as indices 24 and 25. Every call passes false,17,23
for the three remaining arguments; this body does not consume them.

| Input | Size 0 | Size 1 |
| --- | --- | --- |
| Actual App d/e scalars | 320x240 | 641x481 |
| Actual Settings.I GUI scale | 0 | 2 |
| Actual QQ integer GUI dimensions | 320x240 | 321x241 |
| Actual QQ double dimensions used by PX glOrtho | 320x240 | 320.5x240.5 |
| Actual UQ.h tick operand | 17 | -2147483647 |

| Health case | Y current | Z previous | by hurt blink | Inventory selected index |
| --- | --- | --- | --- | --- |
| 0 | 20 | 20 | 0 | 0 |
| 1 | 3 | 7 | 15 | 8 |
| 2 | 11 | 13 | 15 | 4 |

The actual Display and viewport remain 320x240 for both source App scalar sizes.
PX does not set the viewport. Its actual projection uses exact QQ double
dimensions; rectangle placement uses the ceiled integers. Each independent call
starts with g zero, NW e.position zero and Vec3 pool cursor zero. Initial
projection is identity scaled (1.25,.75,1.5); modelview is identity translated
(5,6,7). Current color starts (.125,.25,.5,.75), texture/depth/alpha testing
and depth writes are enabled, and blend/cull/fog/lighting/lights/color material/
rescale normal are disabled. Color writes are enabled, shade model is smooth,
alpha function is GREATER .1, and color/depth buffers are cleared.

## Published observations

`draw.json` contains five arrays:

* `state_rows`: 26 actual post-return GL/source/Random observations.
* `geometry_rows`: 26 hashes of the final actual NW quad-converted batch.
* `pixel_rows`: 26 actual authored-texture pixel counts and digests, for reference
  only. Native authored textures may differ; these do not claim pixel parity.
* `input_rows`: actual brightness and World subtraction observations before
  each independent draw, documenting the selected vignette path.
* `light_rows`: the two actual in-height sample coordinates, successful bounds
  flags and World light-query results.

`HUD_STATE` has 63 cells: prefix; index/size/health/partial identities; k and c
float hex words; actual Random 48-bit seed in decimal; Vec3 cursor; modelview
depth; projection depth; matrix mode; 16 modelview words; 16 projection words;
four current-color words; texture/depth/alpha/blend/cull/fog/lighting/light0/
light1/color-material/rescale flags; depth-write mask; blend source/destination;
and actual NW e.position. Fancy is false for indices 0..11 and true thereafter.

Current color is queried first, then modelview and projection, followed by pool,
stack and other GL state. Each successful draw leaves white current color,
modelview translated by z=-2000, both stack depths 1 and mode MODELVIEW. Actual
lighting consumes four pooled Vec3s. Actual NW e.position is 48 uploaded words.
Random is the genuine bootstrap JDK class; `--add-opens` permits reading its
actual AtomicLong seed after the source reseed and optional ten jitter calls.

`HUD_GEOMETRY index 5 6 sha256` hashes actual g words 0..4 (position and UV) for
six converted vertices, each int encoded big endian. Color/normal/reserved words
are omitted because these rectangles do not author them. This is the **final
heart batch**, not an interception of every preceding hotbar/crosshair batch.
Private capture output retains selected payloads as `HUD_WORDS` for diagnosis.

`HUD_PIXELS index 320 240 visible sha256` counts nonblack RGB pixels and hashes
actual glReadPixels RGBA bytes in GL row order. It establishes output for these
authored textures and the original device. No game screenshot is distributed.

## Reproduction and limits

Use a matching JDK 17 and locally supplied pinned original archive/SDK:

```sh
python -B tests/reference/client/gui_ingame_draw/capture_reference.py \
  --client-jar /path/to/b1.7.3.jar --library-root /path/to/libraries \
  --output .local/hud-original-new
```

The output directory must be new. The wrapper verifies archive/class/method
pins, compiles only observer helpers, denies network/listeners/file writes and
native libraries except the extracted pinned LWJGL DLL, and fingerprints tools,
inputs and compiled helpers before and after execution. The original device is
NVIDIA RTX 3060, OpenGL 4.6, driver 581.42.

These finite returns establish neither prepared-owner constructor parity nor
arbitrary Item rendering. Pumpkin, portal, armor Items,
water bubbles, sleep tint, debug strings, record text and nonempty chat remain
separate unqualified paths. No original assets or matched gameplay frame/pixel
claim follows from this fixture.
