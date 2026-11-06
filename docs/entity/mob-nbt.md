# Client Mob NBT

The client NBT reader and writer support Spider, Zombie, Skeleton, Creeper, Slime, Pig, Sheep, Cow, Chicken, Wolf and Squid through their real Living, Creature, Monster, Animal and WaterCreature inheritance. The public entry points are [Entity NBT read/build](../../include/mc/entity/nbt/nbt.h) and [named save/factory](../../include/mc/entity/nbt/factory.h). This scope covers the client Overworld classes; server and Nether Mob hooks remain separate work.

All eleven classes inherit the Living fields `Health`, `HurtTime`, `DeathTime` and `AttackTime`. The writer narrows each current Java int to a Short. The reader uses Short getters. Missing Health first stores the getter's zero, then the presence check replaces it with 10. Missing timers become zero. A wrong Health Byte throws at the Short getter before the first Living store. During a full read, the earlier Entity base stores have already happened and remain visible.

| Mob | Additional fields | Read and write behavior |
| --- | --- | --- |
| Spider, Zombie, Skeleton, Cow, Chicken, Squid | none | Their hooks delegate to Living. Zombie resolves the inherited Monster hook. |
| Pig | `Saddle` Byte | Read any nonzero Byte as true; write the current saddle Boolean as 0 or 1. |
| Sheep | `Sheared`, `Color` Bytes | Sheared uses mask 16 in watcher Byte 16. Color reads a signed Byte, masks it with 15, and preserves that byte's high bits. |
| Creeper | optional `powered` Byte | Read a nonzero Byte into watcher value 1. Write the key only when the current watcher Byte is exactly 1. |
| Slime | `Size` Int | Read stored Size plus one through the real resize operation; write signed watcher Byte minus one. |
| Wolf | `Angry`, `Sitting` Bytes; `Owner` String | Read the Boolean flags, then replace Owner and set the tame bit only for a nonempty owner string. |

These hooks do not persist `Age`, `EggLayTime`, Creeper `Fuse` or `ExplosionRadius`. Chicken's constructor egg timer is unrelated to the NBT record. Authored unknown fields disappear from fresh source saves.

## Slime size and health

Slime reads Living first, then adds one to stored Size with Java int arithmetic. Resize narrows that requested int to a signed watcher Byte, computes dimensions from the full int, and replaces Health with its Java int square. The later writer reads the signed Byte rather than reconstructing the full requested size.

| Stored Size read | Requested size | Current Health | Signed watcher | Size written | Health Short written |
| --- | --- | --- | --- | --- | --- |
| 3 | 4 | 16 | 4 | 3 | 16 |
| -1 | 0 | 0 | 0 | -1 | 0 |
| 127 | 128 | 16384 | -128 | -129 | 16384 |
| 255 | 256 | 65536 | 0 | -1 | 0 |

These are bounded, observed cases. They do not establish allocation or gameplay behavior for every possible size.

## Wolf owner lifetime

The source reader tests Owner length after reading Angry and Sitting. Missing Owner supplies the empty string. That branch leaves the previous owner and tame bit unchanged, while missing Angry and Sitting clear their bits. The original authored case moves watcher flags from 7 to 4 after a missing-field read and keeps the same UTF16 owner value.

The native owner is a counted UTF16 String. A changed nonempty owner is copied while the previous owner remains live, then replaces the watcher reference. Native limits include the overlapping old and new String storage. Equal, missing and empty values keep the existing owner. The String remains owned until replacement or Mob destruction; it does not borrow an input NBT document's storage.

The separate native Wolf profile has executed on both hosts. Its nine-unit owners include NUL, Omega and surrogate units. Equal content reuses the actual String identity even with allocation budget zero. A replacement needs the old and new 42-byte owners together: budget 83 refuses the replacement before publication, while budget 84 succeeds. The refusal retains the previous owner identity and the earlier completed Living and Angry/Sitting stores. It is a native limit result, with no source exception or Java OOM claim. Explicit external String holds remain readable after Entity, World and class retirement, then receive two balanced releases.

## Compact native diagnostics

The x86-64 Windows and Linux common payload of `mc_entity_nbt_error` occupies 544 bytes. Independent Player sleep ownership cells extend the complete error to 568 bytes; Mob paths leave those cells NULL. The selected Mob hook detail retains Living, concrete, parent, child and watcher positions. Constructor failures select `MC_ENTITY_NBT_LATE_MOB_CALL`; expand that arm with `mc_entity_nbt_mob_call_error` before reading its lower causes. The accessor returns a complete value, without borrowing the original Entity, class owner or another error object. Unselected union arms are not error projections.

The executed diagnostic controls cover an actual cold NULLWorld Skeleton factory refusal at constructor site 45, before the NBT reader. They also reach a real Sheep class-allocation limit, then a repeated use of that failed class. Both the first and later causes survive class retirement and expand to their complete recorded values. A separate authored matrix checks every diagnostic member and arm transition; those matrix values are native storage controls, not claims that the original game produced every authored error shape. These contracts are described in [errors.h](../../include/mc/entity/nbt/errors.h) and tested by `entity.player_nbt_errors`.

## Source saves and the legacy overlay API

Full `SN.d` writes the seven Entity base fields and dispatches the Mob hook. Direct `mc_entity_build_nbt` follows that behavior and supplies no `id` or removed/name decision. Boolean `SN.c`, exposed by `mc_entity_save_nbt`, resolves the exact runtime Class name before reading the removed flag. An admitted Mob gets `id` plus the base, Living and species fields. A removed or unmapped Entity returns false without producing a new tree. Health alone does not control this decision.

The source Chunk serializer uses that Boolean result and fresh semantic fields. Retained input provenance remains a separate native owner for loading and accounting. It does not supply unknown tags to the source writer. For example, a Creeper whose powered watcher becomes zero omits `powered` in a fresh save.

The separate legacy `mc_entity_save_storage_nbt` overlay API reaches the semantic save first. A live Mob then returns an explicit unsupported overlay policy. Its false Boolean result still returns normally before that step. A general overlay could restore a stale optional key such as `powered`; this API therefore does not merge a Mob's retained input into its fresh result.

## Factory and evidence boundaries

The native factory allocates and constructs the canonical client Mob, then calls the full reader at `JC` site 66, outside the constructor catch. The ordinary World loader supplies its actual World and Registry. Skeleton's first static Bow initialization needs that Registry. A cold NULL or unbound World does not receive a substitute Registry or a manufactured ready class. Native limits, strict floating point admission, immutable input views and counted ownership remain explicit API contracts.

The [original reference recipe](../../tests/reference/entity/mob_nbt/README.md) ran genuine NULLWorld constructors and known-ID factories in ordinary and independent read-only JDI processes on Windows and Linux. Each VM produced 62 binary NBT outputs; each host recorded 3,086 JDI events and 74 original Code pins. No source fields, Random seeds, clocks or debugger state were assigned.

The native fixture exercises actual nonnullWorld factory/import, two full source World saves and two reloads. Its 83 snapshots contain 61 common original cases and 22 additional Mob readbacks from new Worlds. The extra original case writes a Creeper into a preexisting mutable Compound; the native fresh-tree API has no corresponding argument. The fixture uses Z=+2.75 to keep records inside Chunk 0,0, while the original prepared records use Z=-2.75. The comparator records that input difference for full outputs and checks constructor entropy within each receiver. It compares typed fields, Boolean results and within-run committed bytes, without requiring HashMap order or compressed-byte identity across processes.

The comparison recipe and its scope are recorded in [comparison-README](../../tests/reference/entity/mob_nbt/comparison-README.md). The [final E record](../../tests/reference/entity/mob_nbt/comparison-provenance-e.json) identifies the rebuilt products, 33 passing headless CTests and 61 matching common cases on each host. Earlier qualification remains recorded separately. These comparisons do not qualify natural world generation or the complete original World caller.
