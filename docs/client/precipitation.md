# Rain and snow drawing

The default World frame invokes `px.c(F)V` after selection and translucent
terrain. Its dry return now continues through fog disable, another fog setup,
fog enable and the captured RenderGlobal's cloud draw at `px.a(FJ)V:963`.
[Cloud drawing](clouds.md) now returns, followed by fog setup and depth clear;
the next missing child is hand drawing at997. This does not complete a gameplay frame.

The [precipitation API](../../include/mc/client/world/precipitation.h) and
[implementation](../../src/client/world/entity_renderer/precipitation.c) execute
both snow and rain traversals. The renderer's separate
[rain tick](renderer-ticking.md) covers particle and sound scheduling prefixes;
its splash-particle constructors remain unfinished.

## Source behavior

Authority is the client archive in the [binary audit](../reference/binaries.md).
[Class and method identities](../../tests/reference/client/precipitation/source-pins.json)
pin PX, FD and the concrete App subclass. The original precipitation method has
1,736 Code bytes and returns at 18 or 1735.

The method reads the current App World for interpolated rain strength. Strength
less than or equal to zero returns before camera, Settings, Math, NW or Display
work. NaN follows the drawing branch. The camera and World are then captured by
separate App reads; the current camera coordinates supply three Math floors.
NW active use precedes culling, normal, blending and alpha state changes.

Snow texture lookup precedes camera interpolation and the next Math floor.
The current fancy-graphics setting selects radius 5 or 10. The captured World's
actual biome manager supplies one cached square array, traversed in X then Z
order. Snow uses each Biome's snow flag. Rain uses its rain predicate, which
also excludes snow biomes. There is no substituted biome enum or weather map.

Each qualifying column queries the World's top solid/liquid position and clips
its lower and upper vertices against that result. Empty vertical spans skip
Random and drawing. Snow clamps negative surface heights to zero and samples
brightness at the greater of surface height and interpolated camera height.
Rain keeps the returned surface height and samples brightness at 128.

The genuine PX Random is reseeded per drawn column using Java int wrapping and
signed widening. Snow consumes two floats and two Gaussian values in source
order; rain consumes one float. Tick arithmetic, texture coordinates, radial
fading and brightness retain the original float/double boundaries. Every column
begins a real NW quad batch, publishes color and camera-relative translation,
emits eight vertices, clears translation and draws through the canonical Display.

After snow, a fresh App RenderEngine supplies the rain texture. A second Settings
read can increase the radius to 10 but never reduces it. The same captured biome
array is reused. A successful tail enables culling, disables blending and
restores the alpha reference to 0.1. Source failures preserve earlier effects.

## Alpha testing

The source GL3 shader uses a stored `GL_GREATER` reference instead of a fixed
threshold. Finite references clamp to [0,1]. Weather sets 0.01 before its passes
and restores 0.1 afterward. State inspection exposes the stored reference;
unsupported functions and nonfinite references fail without changing it.
Compilation of these state calls remains unsupported.

## Ownership

A retained call holds the reached camera, World, biome manager, Math, graphics
namespace and texture Engines independently. A World frame lends its existing
PX occurrence and active guard; standalone callers acquire a PX occurrence.
The entire graph remains live and exclusive through retirement.

Failed calls are terminal. Inspection reports the reached source position and
typed child cause. Retirement releases native rights without restoring Random,
GL, NW, biome arrays or source fields. It consumes a failed texture attempt only
when this call created that attempt; a foreign pending lookup survives. Refused
cleanup preserves the remaining rights for retry. Wrapped traversal at an
INT32_MAX loop increment is refused explicitly after its completed column.

## Evidence and limits

The [original observer](../../tests/reference/client/precipitation/README.md)
records 100 unchanged-original calls over ten strength and ten partial-tick
bit patterns with an explicitly null camera. Native execution compares every
return/null outcome and subsequent Random value. These observations cover dry
returns and failure precedence, including NaN and infinities. They do not
execute original weather rendering or constructors.

The native render control uses genuine World, Player, Settings, texture-pack,
Engine, NW and Display owners with disposable authored chunks and textures.
It prepares actual mutable Biome flags, then draws snow, rain and dry-biome
profiles at both radii. It also checks foreign lookup preservation, reached
null-Random failures, source state restoration and checked retirement. A real
pixel control proves that alpha 0.05 survives reference 0.01 and fails reference 0.1.
These checks establish native composition, not original weather pixel equality.

Clouds, hand and HUD drawing, full Entity rendering, repeated interactive frames
and complete gameplay remain unfinished. No playable-client or whole-frame
parity result is claimed.

## Qualification

All 46 selected production/test targets build on Windows MSVC Debug and Linux
Clang18 ASan/UBSan. The selection passes 55 Windows CTests in 74.43 seconds and
51 Linux CTests in 112.59 seconds, with no failures, skips or sanitizer findings.
All 2,319 C/helper/build inputs remain unchanged through both builds and tests.
Formatting checks 1,634 C/header/fragment files with clang-format18.

Each host passes 677 precipitation checks, including the 100 original prefix
rows and six native render profiles. Snow and rain each draw 121 or 441 columns;
dry biomes draw none. Native alpha pixels distinguish 0.01 from 0.1. The Windows
loaded-World selection suite preserves saves/resources and all 72 focus histories,
then reaches PX963. Its inspected capture shows authored grass terrain and Sky
without HUD. The original observer passes a fresh run with unchanged inputs and
helpers. These are separate finite source, native state and device scopes.
