# Client random Chunk ticks

The retained client World scheduler now executes the whole `fd.n()V` body.
It clears the actual active coordinate Set, gathers each live Player's
radius-nine square, decrements the cave-sound cooldown and captures the Set's
iterator. One Player away from a Chunk boundary supplies 361 coordinates.
Every completed coordinate performs all 80 random Block samples. A full call
returns at bytecode 994, allowing its `fd.l()` caller to return at 198.

## Sampling and source order

The sampling accumulator is the actual wrapping integer `World.g`. Each
sample multiplies it by three and adds the literal 1013904223. This method
does not read `World.h`. `World.r` is a separate Java Random owner. The code
keeps its rejection sampling and the original order of weather tickets,
ambient draws and selected Block draws.

Each coordinate acquires the current provider's Chunk once. A native capture
holds that exact receiver through callbacks and Block mutations. Requested
coordinates determine World offsets even when the cache returns an alias.
Ambient, metadata and saved-light reads dispatch the normal or Blank Chunk
method. Random samples instead reload the actual raw block-byte array, even
for Blank. The selected byte indexes the real `Block.n` array; only a true
flag reaches the separate `Block.m` read, late World Random read and actual
virtual tick. There is no empty-Chunk shortcut or configurable sample limit.

With a zero cooldown, cave ambience uses plant light, saved sky light and the
nearest actual Player. Sound callbacks run before the next cooldown draw and
store. A failed callback preserves the pitch draw and leaves the cooldown
unchanged. The ordinary client sound and ambience paths have their own later
dependencies.

The lightning ticket precedes the rain and thunder tests. A successful rain
location constructs a genuine [Lightning Entity](../entity/lightning-construction.md),
publishes it in the weather List and then stores the flash counter. Snow and
ice use an independent ticket, the current manager's actual Biome snow flag,
the captured Chunk's saved Block light and the original notifying setters.
Still water with metadata zero can freeze without rain. Snow placement and
freezing retain their separate gates and ordering.

## Block bodies and ownership

Grass, Leaves, Ice and Snow Block now execute their concrete client tick
bodies through the shared mutation dispatcher. Grass retains its repeated
light queries and exact Random calls. Leaves owns its real 32,768-integer
scratch array for the Registry lifetime. Allocation precedes area-presence
testing; an absent area still reads the retained center value. Ice and Snow
Block keep their saved-light, drop and notifying-mutation order.

The call owns its World, actual Math namespace, captured Chunk, returned
Player and reached Lightning candidate. Published Entities have independent
List occurrences. Retained WorldAccess failures preserve their producers and
source exceptions. Native retirement releases these holds without clearing
source Sets, restoring fields, replaying callbacks or undoing earlier writes.
The captured iterator detects a structural change at its next source `next`
operation, rather than retargeting a replacement Set.

The genuine fixed manager constructor also supports the original `hd`
object: nullable Biome, exact temperature/rainfall bits and empty inherited
arrays. Assigning it uses the actual provider manager field and checked
ownership. Weather tests can therefore use real Plains and Tundra managers.

## Qualified native integration

All 76 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan.
All 89 Windows tests pass in 130.56 seconds and all 85 Linux tests in
172.59 seconds, without failures, skips or sanitizer findings. All 2,422 C,
helper, build and reference inputs remain unchanged through both builds and tests.
Formatting checks 1,679 files; source bindings cover 7,862 entries and 3,361 files.

The selection includes World queries and the spring's returned client Grass
tick. Its bright prepared cavity checks the exact three supplied Random draws,
cleared immediate flag, complete arrays and unchanged World Random/profile.
The server Grass dependency and genuine failed-tick prefixes remain separate.

Each host passes 89,842 direct World random-tick checks. All 14 original frame
rows, 14 rows of five array/identity hashes and 59 callbacks match exactly.
The 18 direct Block profiles add 69,740 mandatory checks and 368 probe checks,
with 54 array hashes, 18 persistent scratch results and five complete dropped
Item records, including exact positions. Lightning adds 440 checks over five
constructors, 594 deterministic field comparisons (including 66 JDI-only
failed-object fields) and 35 separate scalar checks. Independent constructor
entropy remains separate from deterministic source Random inputs.

The audited original World/Block package checks 19 live method bodies, 801
detailed samples and 77 Random returns, with 205 class pins and the same 204
initialized classes in each VM. Native class-failure controls distinguish the
first source initializer exception from cached erroneous-class use, while
allocation limits and missing native dependencies stay native failures.

The loaded Windows controls preserve saves, resources and all 72 original focus
histories. A focused profile completes 361 random Chunk coordinates and 28,880
samples, then reaches client ambience at `Minecraft.k:1311`. Further Block
overrides, Mob ticking/storage/drawing, ambience and gameplay remain unfinished.
The null-byte-array control retires its retained call unrepaired; complete
Store shutdown with that malformed source field remains unsupported.

## Evidence and limits

The [original observation package](../../tests/reference/world/random_tick/README.md)
and authored native controls exercise the method separately from the earlier
World scheduler prefix observations. The unchanged client JAR is identified
in [binary evidence](../reference/binaries.md). No original classes or assets
are required by the public native tests or the normal executable.

The audited ordinary and read-only debugger runs agree on all 14 World profiles
and 18 direct Block profiles. The package pins 205 original classes and checks
19 live method bodies, 801 detailed first-Chunk samples and 77 Random returns.
Each VM initializes the same 204 original classes.

Native whole-method comparison checks all 14 frame rows, 14 rows of five array
and identity hashes, and 59 callback events. A separate native control changes
`h` and verifies that the source's literal accumulator remains unaffected.
Direct Block comparison adds 18 rows, 54 array hashes, 18 persistent scratch
results, 18 callback events and five complete drop records, including positions.
Both hosts pass 69,740
mandatory Block checks and 368 checks over those 18 transported calls. Item
constructor entropy remains separate from those World-Random-derived positions;
callback IO adapters pair reached effects without equating Java Throwable behavior.

Run the asset-independent contracts with:

```sh
ctest --test-dir build -R '^world\.(random_tick|random_blocks(_probe)?|chunk_tick|fixed_manager_constructor)$' --output-on-failure
```

For Windows multi-configuration builds, add `-C Debug`. To compare a native
executable with a separately captured original observation file, use
`tests/tools/world/test_random_tick.py` or `test_random_blocks.py` with
`--executable`, `--work-dir` and `--observations`. The original package's README
records the capture command and the distinction between ordinary and debugger
observations.

The exceptional null-byte-array control retires the random call while the
actual array remains null. Whole-Store terminal validation still refuses that
malformed storage owner. The fixture restores its owned storage only after
checking the refusal, for disposal; this does not qualify whole-Store shutdown
with a null source array.

A focused loaded Windows control completes 361 coordinates and 28,880 samples,
then reaches client ambience at `Minecraft.k:1311`. Single-player remains
pre-playable. Mob ticking, drawing and persistence, further Block overrides,
Lightning ticking, inactive saving and gameplay actions remain unfinished.
