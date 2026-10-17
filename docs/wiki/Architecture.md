# Systems and subsystems

Project-owned runtime code uses C17. Implementations live under
`src/<system>/<subsystem>/`; public headers mirror those paths under `include/mc/`,
and tests mirror them under `tests/`. Create folders with actual implementations
rather than empty placeholders.

| Current module | Responsibility | Runtime dependencies |
| --- | --- | --- |
| `core/java` | Defined integer/cast/Random behavior | C runtime |
| `core/math` | Initialized float trig lookup and Java cast/compare double floor | Caller-owned table, system double sine during initialization |
| `core/text` | Modified UTF-8 with explicit UTF-16 units | C runtime |
| `storage/nbt` | Owned raw tag trees, original IDs 0..10, bounded read/write | Shared core |
| `storage/compression` | Owned gzip/zlib memory output, explicit limits and checksums | Static zlib |
| `world/noise` | Original seeded Perlin/octave and 2D simplex/octave sampling | Shared core, caller-owned layers |
| `world/biome` | Original climate processing, float-table biome lookup and retained arrays | Shared core/noise, explicit allocation budget |
| `world/terrain` | Normal density, base interpolation, surface replacement and cave carving | Shared core/noise/math, immutable layers/table, explicit Random and borrowed spans |

The shared libraries initialize no window, graphics, audio, networking, assets,
or Java. The raw NBT parser and compression module remain separate interfaces;
callers explicitly compose them. No file write or world schema is implicit.
Noise sampling preserves original constructor ordering and bulk-specific
behavior; [its interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/noise.md)
and [reference evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/noise.md)
describe the exact tested scope. Complete terrain generation remains open.

The [math interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/core/math.md)
keeps the 256 KiB table caller-owned and immutable after exclusive initialization.
Lookup methods retain float multiply/phase/conversion order, and the separate
floor helper retains Java narrowing and wrapping. Complete table comparisons
scope the measured native math backends; this is a cave prerequisite.
The separate [simplex interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/simplex.md)
preserves its own floor, wrapped cell sum and independent octave factors. Its
numerical tests do not establish complete biome climate or classification.

The separate [climate/biome module](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/biome.md)
combines three independent seeded simplex channels with original processing and
table lookup. Mutable managers preserve array growth, clear numeric suffixes
and retain biome suffixes under a caller-selected allocation budget. Its
[reference evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/biome.md)
separates numerical fixtures from world startup, terrain and full biome behavior.

The [normal terrain module](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/terrain.md)
constructs all 82 original Perlin layers in order, consumes caller-supplied
processed climate and interpolates a fixed density grid into base blocks.
Constructor state is immutable after initialization; workspace and outputs are
borrowed. Population, lighting and whole chunk integration remain later work.

The [surface interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/surface.md)
uses those existing layers with current material byte pairs and a separate
mutable Random. It preserves Z/X/Y traversal, unconditional bedrock draws and
sea/sandstone state transitions. Field sampling and prepared-field application
can also run independently. Complete chunk generation remains open.

The [normal cave interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/caves.md)
borrows an immutable math table, existing block prefix and separate cave Random.
It preserves the original range-8 neighbor schedule, local path draw order,
water screening, selective carving, lava and grass repair. Checked preflight
precedes mutation; successful calls allocate nothing. Private room/path helpers
are shared by normal runtime and focused reference adapters.

The planned game systems include ordered world/simulation updates, platform
files/sockets/clock/input services, OpenGL 3.3 rendering, audio, original protocol
networking, client sessions, and a dedicated server. Those game systems are not
implemented yet. The server must remain independent of client display/audio
initialization, and shared simulation must retain evidenced original side
differences.

Strict project diagnostics and floating-point rules are configured in CMake.
`.clang-format` specifies four spaces, 100 columns, and Linux-style braces; CI
checks project C files with clang-format18. Reference tooling is kept under
`scripts/reference/<subsystem>/` and `tests/reference/<subsystem>/`. Ignored
original research inputs are excluded from formatting and publication.

Read the versioned
[architecture](https://github.com/frankischilling/minecraft-c/blob/main/docs/architecture.md)
and [roadmap](https://github.com/frankischilling/minecraft-c/blob/main/docs/roadmap.md)
for current module details and remaining dependencies. The
[parity matrix](https://github.com/frankischilling/minecraft-c/blob/main/docs/parity.md)
records scenario-specific evidence; a shared-library test is not gameplay parity.
