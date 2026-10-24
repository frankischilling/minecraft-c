# Minecraft names and source aliases

Current names describe the objects in Minecraft Beta 1.7.3. The
[glossary](../../CONTEXT.md) defines the shared vocabulary; this page connects it
to native types and paired original classes. The [verified binaries](binaries.md)
remain the behavioral authority. Client and server aliases are separate: a short
name on one side does not identify the same class on the other.

The supplied client SHA-256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`;
the server SHA-256 is
`033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.
Names below follow the verified registry constructors, instance IDs, class
families and existing source maps. They improve terminology without establishing
new behavior or original/native parity. Mapping projects can use different
English aliases for the same original class.

## Object roles

| Minecraft role | Current native name | Original client / server | Meaning |
| --- | --- | --- | --- |
| TileEntity | `mc_tile_entity` | `ow` / `jh` | Additional state attached to a Block, with World, coordinates and invalidation |
| WorldAccess | `mc_world_access` | `pm` / `jp` | The interface receiving World changes for presentation and other systems |
| ChunkProvider | `mc_chunk_provider` | `cl` / `bl` interface; caches `ok` / `jb`, generator `yf` / `pk` | A source of loaded/generated Chunks; the native owner combines concrete cache and generator storage |
| WorldProvider | `mc_world_provider` | `xa` / `os` | A dimension's environment, including lighting, celestial calculations and biome source |
| WorldChunkManager | `mc_world_chunk_manager` | `xv` / `ph` family | Biome, temperature and rainfall sampling, with a noise-backed base and fixed overrides |
| Overworld generator | `mc_overworld_generator` | `yf` / `pk` | Terrain generation and population for dimension 0 |
| LivingEntity | `MC_ENTITY_CLASS_KIND_LIVING_ENTITY` | `ls` / `hl` | The shared living base of Players, animals and monsters |
| Carried Block | `carried_block` role | The selected `uu` / `na` Block | The Block type moved by a piston, distinct from the `ut` / `mz` MovingPistonBlock receiver |
| GameSettings | `mc_game_settings` | `kv` / client only | Client options, KeyBinding references and the options File; persistence is separate from effectful setters |
| KeyBinding | `mc_key_binding` | `qb` / client only | A named client action and its assigned key code |

The numerical `mc_biome_manager` remains a climate algorithm/storage helper.
Noise-backed versus fixed WorldChunkManager methods are not Overworld versus
Nether dimension identities. Likewise, normal completion, ordinary versus Blank
Chunk behavior and statistical normal distributions retain their meanings.
Generic resource observers, measurement effects, snapshots and ownership terms
do not name the WorldAccess interface.

Jukebox, Dispenser and NoteBlock name the private TileEntity family selectors
`LIVE_JUKEBOX`, `LIVE_DISPENSER` and `LIVE_NOTE_BLOCK`. Their serialized IDs remain
`RecordPlayer`, `Trap` and `Music`. The LivingEntity registry entry still writes
`Mob`. All original registry, translation, resource, NBT and fixture keys retain
their exact bytes. The thirteen Biome labels also stay unchanged, including Hell,
Sky and Ice Desert; Nether is the dimension name, and Sky is not End.

## Compatibility labels in dedicated STATE schema 2

The dedicated capture/replay STATE format preserves its existing discriminators.
They are compatibility labels, not reflection of current C typedef names:

| Retained STATE label | Current native owner |
| --- | --- |
| `mc_world_provider` | `mc_chunk_provider` (ChunkProvider) |
| `mc_world_dimension` | `mc_world_provider` (WorldProvider) |
| `mc_world_dimension_biome` | `mc_world_chunk_manager` |
| `mc_normal_generator` | `mc_overworld_generator` |
| `split_fields_in_mc_normal_generator` | The retained split cave fields in `mc_overworld_generator` |

In particular, the old STATE label `mc_world_provider` identifies a ChunkProvider;
it does not identify the current WorldProvider typedef with the same spelling.
No field, fixture discriminator or source pin is rewritten to make those names
look current. The [dedicated reference workflow](dedicated-world-construction.md)
retains its declared correspondence and historical evidence.

The [existing-World redstone producer](../world/redstone.md) has a separate schema
2, scope `native-existing-World-redstone-plan`, and current `redstone` state and
diagnostic names. Its version does not change the dedicated STATE contract.
Earlier redstone captures retain their original schemas, helpers and executable
pins; the naming migration does not qualify their admission to the new producer.

## Block families

Each row is one of the 69 native Block kinds. A shared class remains one kind:
instance IDs, metadata and flags still distinguish powered rails, sticky pistons,
single/double slabs, lit/unlit furnaces and other variants. Plain base-Block
instances also keep their separate IDs and properties. The class aliases below
are source evidence, independent of the semantic enum names.

| Semantic enum | Client | Server | Family |
| --- | --- | --- | --- |
| `MC_BLOCK_BASE` | `uu` | `na` | Block base class; generic full-block instances retain distinct IDs/materials |
| `MC_BLOCK_STONE` | `eo` | `cx` | Stone Block family |
| `MC_BLOCK_GRASS` | `wp` | `oj` | Grass Block family |
| `MC_BLOCK_DIRT` | `ot` | `jf` | Dirt Block family |
| `MC_BLOCK_SAPLING` | `he` | `eu` | Sapling Block family |
| `MC_BLOCK_FLOWING_FLUID` | `om` | `jc` | Flowing fluid Block; shared water and lava class |
| `MC_BLOCK_STILL_FLUID` | `nx` | `is` | Still fluid Block; shared water and lava class |
| `MC_BLOCK_SAND` | `gk` | `ei` | Sand Block and superclass of falling Gravel Block |
| `MC_BLOCK_GRAVEL` | `ne` | `ig` | Gravel Block family |
| `MC_BLOCK_ORE` | `mt` | `ic` | Ore Block; gold, iron, coal, lapis and diamond instances share this class |
| `MC_BLOCK_LOG` | `vg` | `nk` | Log Block family |
| `MC_BLOCK_LEAVES` | `bk` | `ar` | Leaves Block family |
| `MC_BLOCK_SPONGE` | `xf` | `ov` | Sponge Block family |
| `MC_BLOCK_GLASS` | `fk` | `do` | Glass Block family |
| `MC_BLOCK_DISPENSER` | `xq` | `pa` | Dispenser Block family |
| `MC_BLOCK_SANDSTONE` | `rd` | `kl` | Sandstone Block family |
| `MC_BLOCK_NOTE_BLOCK` | `pt` | `jr` | Note Block family |
| `MC_BLOCK_BED` | `ve` | `nj` | Bed Block family |
| `MC_BLOCK_RAIL` | `pc` | `jj` | Rail Block; normal and powered rail instances share this class and boolean flag |
| `MC_BLOCK_DETECTOR_RAIL` | `ph` | `jn` | Detector Rail Block family |
| `MC_BLOCK_PISTON_BASE` | `jq` | `gi` | Piston base Block; ordinary and sticky instances share the class |
| `MC_BLOCK_COBWEB` | `rn` | `ku` | Cobweb Block family |
| `MC_BLOCK_TALL_GRASS` | `ru` | `kz` | Tall Grass Block family |
| `MC_BLOCK_DEAD_BUSH` | `jb` | `fz` | Dead Bush Block family |
| `MC_BLOCK_PISTON_HEAD` | `h` | `g` | Piston head/extension Block, distinct from base and moving-piston placeholder |
| `MC_BLOCK_WOOL` | `ee` | `cq` | Wool Block family |
| `MC_BLOCK_MOVING_PISTON` | `ut` | `mz` | Moving Piston Block family |
| `MC_BLOCK_FLOWER` | `wb` | `nw` | Flower Block, also superclass of sapling/tall grass/dead bush/mushroom/crops |
| `MC_BLOCK_MUSHROOM` | `tl` | `md` | Mushroom Block family |
| `MC_BLOCK_MINERAL_STORAGE` | `l` | `j` | Mineral storage Block family: gold, iron and diamond blocks; not a container |
| `MC_BLOCK_SLAB` | `ys` | `pw` | Slab Block family: single and double, with stone/sandstone/wood/cobblestone variants |
| `MC_BLOCK_TNT` | `ah` | `t` | Tnt Block family |
| `MC_BLOCK_BOOKSHELF` | `hb` | `er` | Bookshelf Block family |
| `MC_BLOCK_OBSIDIAN` | `fb` | `dh` | Obsidian Block family |
| `MC_BLOCK_TORCH` | `vm` | `np` | Torch Block family |
| `MC_BLOCK_FIRE` | `yq` | `pt` | Fire Block family |
| `MC_BLOCK_MOB_SPAWNER` | `dd` | `bz` | Mob Spawner Block family |
| `MC_BLOCK_STAIRS` | `ss` | `lu` | Stairs Block family |
| `MC_BLOCK_CHEST` | `e` | `e` | Chest Block family |
| `MC_BLOCK_REDSTONE` | `sm` | `lo` | Placed redstone dust/connection Block ID55; not a later solid redstone storage block |
| `MC_BLOCK_CRAFTING_TABLE` | `fi` | `dn` | Crafting Table Block family |
| `MC_BLOCK_CROPS` | `ni` | `ij` | Crops Block family |
| `MC_BLOCK_FARMLAND` | `vl` | `no` | Farmland Block family |
| `MC_BLOCK_FURNACE` | `tc` | `lz` | Furnace Block; lit and unlit instances share the class |
| `MC_BLOCK_SIGN` | `uj` | `mt` | Sign Block; standing and wall-mounted instances share the class |
| `MC_BLOCK_DOOR` | `le` | `hc` | Door Block; wooden and iron instances share the class |
| `MC_BLOCK_LADDER` | `dp` | `cg` | Ladder Block family |
| `MC_BLOCK_LEVER` | `xr` | `pd` | Lever Block family |
| `MC_BLOCK_PRESSURE_PLATE` | `bv` | `bc` | Pressure Plate Block family |
| `MC_BLOCK_REDSTONE_ORE` | `bs` | `az` | Redstone ore Block; glowing and unlit instances share the class |
| `MC_BLOCK_REDSTONE_TORCH` | `db` | `by` | Redstone torch Block; lit and unlit instances share the class |
| `MC_BLOCK_BUTTON` | `oi` | `iz` | Button Block family |
| `MC_BLOCK_SNOW_LAYER` | `jr` | `gj` | Thin snow layer Block ID78, distinct from solid Snow Block ID80 |
| `MC_BLOCK_ICE` | `nk` | `ik` | Ice Block family |
| `MC_BLOCK_SNOW_BLOCK` | `ac` | `p` | Solid Snow Block ID80, distinct from thin snow layer ID78 |
| `MC_BLOCK_CACTUS` | `or` | `je` | Cactus Block family |
| `MC_BLOCK_CLAY` | `rz` | `lf` | Clay Block family |
| `MC_BLOCK_SUGAR_CANE` | `ri` | `ko` | Sugar cane Block, legacy reeds resource key |
| `MC_BLOCK_JUKEBOX` | `fo` | `dt` | Jukebox Block family |
| `MC_BLOCK_FENCE` | `jw` | `gn` | Fence Block family |
| `MC_BLOCK_PUMPKIN` | `fc` | `di` | Pumpkin Block; ordinary pumpkin and jack-o-lantern share this class |
| `MC_BLOCK_NETHERRACK` | `yi` | `po` | Netherrack Block family |
| `MC_BLOCK_SOUL_SAND` | `oa` | `iu` | Soul Sand Block family |
| `MC_BLOCK_GLOWSTONE` | `ly` | `hr` | Glowstone Block family |
| `MC_BLOCK_PORTAL` | `ak` | `z` | Nether portal Block; no End portal class is implied |
| `MC_BLOCK_CAKE` | `um` | `mw` | Cake Block family |
| `MC_BLOCK_REPEATER` | `wo` | `oi` | Redstone repeater Block; powered and unpowered instances share the class |
| `MC_BLOCK_LOCKED_CHEST` | `ks` | `gv` | Beta locked chest Block ID95, distinct from ordinary Chest Block ID54 |
| `MC_BLOCK_TRAPDOOR` | `oq` | `jd` | Trapdoor Block family |

`MINERAL_STORAGE` names the shared gold/iron/diamond Block family, not a
container. Redstone ID55 names placed dust and connections, not the solid
redstone storage block from later versions. Locked Chest95 remains distinct
from Chest54 and is not a later trapped chest.

## Item families

These are the 38 native Item class kinds. Tool material, armor slots, Minecart
type and other constructor inputs still select distinct instances within a
shared class. ItemBlock subtypes retain their placement metadata and names.

| Semantic enum | Client | Server | Family |
| --- | --- | --- | --- |
| `MC_ITEM_BASE` | `gm` | `ej` | Item base class; many separate simple items retain their own IDs and names |
| `MC_ITEM_SHOVEL` | `wc` | `ny` | Shovel Item family |
| `MC_ITEM_PICKAXE` | `au` | `ah` | Pickaxe Item family |
| `MC_ITEM_AXE` | `ta` | `ly` | Axe Item family |
| `MC_ITEM_FLINT_AND_STEEL` | `ye` | `pl` | Flint And Steel Item family |
| `MC_ITEM_FOOD` | `yw` | `px` | Food Item family; shared heal amount and wolf-food flag |
| `MC_ITEM_BOW` | `qz` | `kj` | Bow Item family |
| `MC_ITEM_COAL` | `rx` | `lc` | Coal Item family |
| `MC_ITEM_SWORD` | `qd` | `jw` | Sword Item family |
| `MC_ITEM_HOE` | `la` | `gz` | Hoe Item family |
| `MC_ITEM_SEEDS` | `rk` | `kr` | Seeds Item family |
| `MC_ITEM_ARMOR` | `wa` | `nx` | Armor Item; material/render-slot and armor-slot parameters distinguish instances |
| `MC_ITEM_PAINTING` | `ym` | `ps` | Painting Item family |
| `MC_ITEM_SIGN` | `vc` | `ng` | Sign Item family |
| `MC_ITEM_DOOR` | `hu` | `fg` | Door Item family |
| `MC_ITEM_BUCKET` | `bc` | `al` | Bucket Item; empty/water/lava/milk instances use the same class |
| `MC_ITEM_MINECART` | `rl` | `ks` | Minecart Item; ordinary/chest/furnace variants use one class and type field |
| `MC_ITEM_SADDLE` | `sa` | `lg` | Saddle Item family |
| `MC_ITEM_REDSTONE` | `ie` | `fk` | Redstone Item family |
| `MC_ITEM_SNOWBALL` | `dm` | `cf` | Snowball Item family |
| `MC_ITEM_BOAT` | `vd` | `ni` | Boat Item family |
| `MC_ITEM_BLOCK_PLACER` | `lv` | `hn` | Direct block-placement Item family used by sugar cane, cake and repeater; distinct from ItemBlock |
| `MC_ITEM_EGG` | `mo` | `hx` | Egg Item family |
| `MC_ITEM_FISHING_ROD` | `cs` | `bv` | Fishing Rod Item family |
| `MC_ITEM_DYE` | `km` | `gu` | Dye Item family |
| `MC_ITEM_BED` | `lp` | `hk` | Bed Item family |
| `MC_ITEM_SHEARS` | `bl` | `as` | Shears Item family |
| `MC_ITEM_RECORD` | `tr` | `mj` | Record Item family |
| `MC_ITEM_SOUP` | `bz` | `bf` | Soup/stew Item subclass of Food; returns Bowl after consumption |
| `MC_ITEM_COOKIE` | `qk` | `jy` | Cookie Item / stack-size-configurable Food subclass, instantiated as cookie |
| `MC_ITEM_MAP` | `wr` | `ol` | Map Item family |
| `MC_ITEM_BLOCK` | `ck` | `bk` | ItemBlock base class; automatic wrappers for otherwise unwrapped registered Blocks |
| `MC_ITEM_WOOL_BLOCK` | `bi` | `ap` | Wool ItemBlock subtype retaining color metadata and names |
| `MC_ITEM_LOG_BLOCK` | `go` | `ek` | Log ItemBlock subtype retaining wood metadata |
| `MC_ITEM_SLAB_BLOCK` | `en` | `cw` | Slab ItemBlock subtype retaining slab variant metadata/names |
| `MC_ITEM_SAPLING_BLOCK` | `mr` | `ia` | Sapling ItemBlock subtype retaining sapling metadata |
| `MC_ITEM_LEAVES_BLOCK` | `uy` | `nd` | Leaves ItemBlock subtype setting placed metadata bit8 |
| `MC_ITEM_PISTON_BLOCK` | `qr` | `kc` | Piston-base ItemBlock subtype returning placement metadata7 |

`BLOCK_PLACER` is the direct Item subclass used for sugar cane, cake and repeater
placement. It is distinct from the actual ItemBlock class and does not imply
that every instance is a sugar cane item. Protected legacy names remain literal:
for example the Diamond item's `emerald` key and the `hellrock`, `hellsand` and
`lightgem` keys for netherrack, soul sand and glowstone.

## Migration and validation

Commands, paths and C identifiers in current documentation use the canonical
names. Earlier test and comparison counts retain their exact checkpoints,
source/class/executable pins and declared projections; they are not reruns of
the renamed code. Frozen history snapshots keep their original text, with moved
links directed to the corresponding immutable source where needed.

Recorded cache fixtures keep their original generation-module paths and source
fingerprints. Their validators admit one complete historical or current module
family and reject mixed, missing or extra members. The WorldProvider projection
similarly retains its [historical recorder](../../tests/reference/world/dimension/recorder.py)
as immutable evidence, with a separate current-checker pin. Neither adaptation
rewrites the original fixture or presents current code as its captured source.

At checkpoint `ef1434015420678559cfa3d3b2f21018c5b733ce`, local suites passed all
619 tests and 587-file formatting. [Hosted verification](https://github.com/frankischilling/minecraft-c/actions/runs/40052114598)
passed all four jobs with 619 actual Passed results and the same formatting
count. Its checkout `b9f16ba78946db69163cccd34103f7ec8c3b1ba3` has tree
`f7e4e0bfa2061c307471d023927d2a129f85104f`, equal to that code checkpoint's tree.
These results precede the canonical naming migration.

The renamed sources pass all 619 local public/JDK tests: 205 MSVC, 207 GCC and
207 Clang, with no failures or skips. Linux enables ASan/UBSan. All 587 C/header
files pass clang-format 18, and controller mirrors and 556 positive/2,011 negative
lifecycle interval controls pass on Windows and Linux. The retained 1,130-file
source copy matches every current non-Markdown input. Eight separate file-only
controls check complete module families, immutable recorder bytes and rejected
provenance substitutions. Initial full runs exposed historical path and recorder
binding failures; those results remain retained alongside the complete passing
reruns. No new original observation follows from the naming changes.
