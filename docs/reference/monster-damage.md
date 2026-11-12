# Original Monster melee parent

The unchanged client `GZ.a(Entity,float)V` is a 66-byte protected method inherited by Zombie. It checks the current Living attack cooldown, its supplied float distance and two independently read vertical bounds comparisons. It then stores cooldown20 at52, reads the current Monster damage field at58 and invokes the actual target's virtual hurt method at61. Either normally returned Boolean is discarded at64; the method returns void at65. An actual child exception prevents that return and leaves the earlier cooldown store visible.

The canonical `UZ` constructor first invokes Monster, whose inherited damage default is2, then stores5 at19 and returns22. The new packet constructs that actual Zombie. Its healthy calls pass source5 to the genuine Player damage continuation and produce health20→15 with statistic2021 increased5 at difficulty2. Row31 is the separately declared actual damage-field0 control. No captured constructor result or random seed becomes an execution input.

The [32-row packet](../../tests/reference/entity/monster_damage/README.md) covers unread cooldown/distance/null operands, both touching edges, a gap, all four unordered vertical comparisons, reached target/bounds nulls, accepted negative cooldown/distance, normal Player damage, false child returns and representative failures after cooldown20. It uses an actual App/Session/statistics graph rather than a null-App prefix. The six genuine Wolves expose the actual false-mode ally alert, including partial earlier assignments before a later watcher failure.

The actual source call sites are:

| Source operation | Bytecode |
| --- | --- |
| Read Living attack cooldown | 1 |
| Compare supplied float with2 | 9 |
| Read target bounds / maxY | 14 /17 |
| Read attacker bounds / minY | 21 /24 |
| First strict vertical comparison | 27 |
| Reload target bounds / minY | 32 /35 |
| Reload attacker bounds / maxY | 39 /42 |
| Second strict vertical comparison | 45 |
| Store cooldown20 | 52 |
| Read actual current Monster damage | 58 |
| Invoke virtual target hurt | 61 |
| Discard actual Boolean result | 64 |
| Return void | 65 |

Four independent original VMs agree on 46,903 selected scalars: 32 calls, 19 normal parent returns and 13 actual exceptions (11 NullPointerExceptions, 2 ClassCastExceptions). Of 19 actual hurt invocations, 9 return (4 true, 5 false) and 10 throw. Two passive JDI journals record the executing Code, entry/return order, actual cooldown writes and first exceptions. The normal and optimized portable controls each pass 713 stream checks, 2,718 passive events and 1,025 corruption refusals.

Default private constructor streams remain independent. Each successful full hurt consumes exactly the two actual Player private float draws used for hurt pitch; all other entity, World and shared Math streams remain unchanged. Constructor `LS.F`, `LS.G` and `LS.aj` fields are checked within each receiver. Raw private outputs remain retained. Shared selected values exclude independent seed/pitch values only after the within-run checks succeed.

The observer prepares ordinary fields and genuine World publication before setting its active marker. During the actual source call, JDI performs only reads; no `invokeMethod`, field write, result replacement, artificial clock or Class-body substitution occurs. The original producer's exact six-file identity and receipt are separate from postprocessing identities. The first compiler failure and corrected preflight are retained alongside the canonical four-VM opportunity. Historical `monster_attack` producer/provenance bytes and its unqualified capture status are not relabeled.

Independent Windows and sanitized Linux native comparisons each pass all 32 calls and 19 normal returns, with 46,828 shared scalar positions, 75 original Java message/stack positions explicitly unpaired, and 10,026 fixture checks. The Linux focused selection passes 7/7 in 4.34 seconds with ASan, UBSan, leak detection and halt-on-error.

The Windows receipt is `build-windows/monster-damage-original/monster-damage-comparison-82ahqnx4/run.json`, with complete native readback SHA256 `909e5068c28d7bac2543866ac8dcaf1269f1f998c788cec8eb915e6074a26de3`. The Linux receipt is `/home/frank/minecraft-damage-wake/build/monster-damage-original/monster-damage-comparison-onuphf59/run.json`, with SHA256 `14f4dde8e23720226c19ee7b9b29c12290be4a91cf016146920a11fe77ff7308`. Independent private entropy and host fields remain in each output. These are separate guarded opportunities, not expected matching whole-output hashes.

The initial Windows opportunity is preserved as a failed exact comparison: its finite observer used an exact Monster identity test for a genuine Zombie and omitted the actual `gz.c` field in both snapshots. A source-versus-retained-native replay narrows the mismatch to those 64 missing positions across all 32 cases. The observer predicate alone is corrected, the target is rebuilt, and the fresh complete comparisons above pass. Original producer/data identities and runtime semantics remain unchanged.

This component evidence concerns an entered protected parent with a supplied distance argument. It does not execute `II.f_`, target acquisition, visibility/path finding, the loaded World entity scheduler, Windows SDK gameplay, original presentation or sustained FPS. The first separate monitor-2 opportunity stops on external `msrdc.exe` focus before World selection; both windows are placed correctly, but it supplies no loaded-combat credit. Its failed receipt remains at `build-windows/zombie-damage-scenes/zombie-damage-jzpnsb_h`. The saved-Zombie encounter must qualify the caller chain separately.

The complete configured cohorts replay this comparison again on their frozen runtime source. Windows receipt `monster-damage-comparison-vxa7ox4u` retains native readback SHA256 `a7007b4eccedb5113b83012d60e9a8709037c0ac4889a14a1ea8e0f28b39f12c`; Linux receipt `monster-damage-comparison-qqqah94i` retains `fcae98be3006aa9ead0ce70babd541679c376ded375fae136040e761a96ef425`. Each again passes all 32 calls, 19 normal returns, 46,828 shared scalars and 10,026 checks. [Testing](../testing.md#monster-melee-damage) records the complete cohorts, separate actual World controls and publication-only wrapper newline normalization.
