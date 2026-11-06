# Original client World random ticks

This package runs the unchanged client `fd.n()V` and actual `wp`, `bk`, `nk`, and `ac` Block tick bodies from the pinned Beta 1.7.3 JAR. It prepares genuine Session, World, provider, Player, normal Chunk and installed fallback Chunk objects. The authored `pm` observer records actual notifications; it never supplies World query answers. The source bodies are neither replaced nor rewritten.

Run from the repository root with JDK 17 and Python, using a fresh ignored output directory:

```powershell
python tests/reference/world/random_tick/capture_reference.py --client-jar b1.7.3.jar --output .local/world-random-tick-original-20261030a
```

`source-pins.json` pins the JAR inventory and whole classes with every method Code digest. `operation-map.json` lists symbolic source references. The driver admits exact helper/source pins before running, checks actual runtime JAR origins and class-resource bytes, compares ordinary execution with a read-only JDI run, and preserves before/after hashes. Captured outputs and original bytecode dumps remain private ignored artifacts. No original assets are needed.

## Complete World profiles

One actual Player at `(0.5,64.5,4.5)` yields the real radius-nine HashSet containing 361 coordinates. The source executes all 80 samples per completed Chunk. The helper never truncates the source loop. Chunks are authored saved/cached data; terrain generation is outside this qualification. Their `n` flag is true, but it does not suppress sampling.

Actual `World.r.setSeed`, mutable `g`, `Q`, weather scalars, array contents and the specified exceptional null inputs are declared fixture inputs. Default constructor clocks and shared Math entropy are not rebound. Final `h` stays `1013904223`: pinned `FD.n` has no read of that field and uses its own literal addend.

| ID | Inputs and source route |
|---|---|
| 0 | No Player, no cached Chunks; `g=17,Q=1,r.seed=0`. |
| 1 | Air, `g=17,Q=6001,r.seed=0`. |
| 2 | Raw Stone throughout, height bytes/minimum 128; `Q=0`. |
| 3 | Air, `g=10,Q=1,r.seed=0`; real cave sound and cooldown. |
| 4 | Same as 3; authored sound observer throws after recording the call. |
| 5 | Same as 3; observer clears the actual captured Set during the sound callback. |
| 6 | All 361 keys alias the provider's actual installed `li`; its raw array contains Grass, `World.B=true`. |
| 7 | First acquired normal Chunk's raw array is null, `Q=6001`. |
| 8 | Stone array, actual `Block.n[1]=true`, `Block.m[1]=null`; restored after observing the exception. |
| 9 | Actual `hd(kd.k,0.5,0.5)`, rain 1, thunder 0; Stone at Y63, height 64. |
| 10 | Same Tundra manager, dry weather; isolated still-water cells at the setup oracle's weather-selected columns. |
| 11 | Actual `hd(kd.i,0.5,0.5)`, rain/thunder 1, difficulty 1; `r.seed=107038380838084`, genuine Lightning creation/publication. |
| 12 | All keys alias one actual normal `lm(0,0)` containing Stone; `Q=0`. |
| 13 | Air and null `World.r`, restored after observation. |

Profiles 9–11 include a one-Chunk cached padding ring: 441 distinct normal Chunks while the captured source Set remains 361. This keeps actual notifying mutations within prepared data. The genuine HD manager is an explicit fixture replacement of the ordinary manager. The source performs its actual biome, light, material, metadata, placement and mutation queries. Profile 10 prepares isolated water without replacing those queries. The ordinary Overworld manager remains in all other profiles.

## Actual Block profiles

Each leaf uses a new actual World and normal Chunk `(0,0)`, World Random seed 89, weather 0, a distinct supplied Random seed 0, and position `(8,64,8)`. Grass-decay profile 1 uses supplied seed 4096. Leaves absent-area profiles use X12, so the radius-five area genuinely crosses into an absent neighboring Chunk. The shared Leaves object's scratch array persists across these calls.

| IDs | Actual source controls |
|---|---|
| 0–5 | Grass dark/opaque no-decay, dark decay, dark/air no RNG, bright spread onto Dirt at `(7,64,8)`, light 4 boundary, notification exception after mutation. |
| 6–10 | Leaves flag 0/no allocation, fresh absent-area zero scratch, Log distance 4, isolated decay, absent-area reuse of the previous negative center. |
| 11–13 | Ice saved Block light 8, light 9, remote light 9; actual opacity is 3 and zero-drop melting produces still Water. |
| 14–17 | Snow Block light 11, remote light 12, local light 12 producing four real Snowball Items, and first Entity-add observer failure. |

Item positions, Stack ID/count/metadata and pickup delay are observed. The JDI stream observes the actual World float draws and actual shared Math doubles. It replays those observed Math scalars without claiming equal default constructor entropy in another process. Private Item phase, motion and constructor-clock fields are outside the deterministic cross-process rows.

## Read-only observations and source order

`FD.n` is 995 Code bytes. Source entry clears P at 4, gathers Player coverage, decrements positive Q at 149, captures the iterator at 156 and acquires each actual Chunk at 207. The ambient route updates g at 229, reads the captured Chunk virtually at 275, performs light/RNG/nearest-Player gates, emits sound at 443, then writes the cooldown at 461. Sound failure therefore leaves Q at zero. Clearing P in that callback preserves the current Chunk's remaining work before the captured JDK iterator fails at source invocation 172.

Every Chunk reaches Random tickets 470 and 589 in order. Conditional weather g stores are 500 and 605. Lightning construction/publication/flash precede the snow/ice branch. Ice selection is independent of rain. Sample g is stored at 887; the fresh captured raw array is read at 927, `baload` is 944, the active-flag read is 951/956, and the selected receiver is invoked at 982. Counter increment is 985, Chunk tail 991, source return 994. Null-array, null-selected-receiver and null-Random exceptions are pinned to actual JDI BCIs 944, 982 and 470, not SourceFile line numbers.

The debugger records the first Chunk's 80 raw samples, actual arguments/results of its Random calls, actual returned Chunk identity, the real JDK Set order, conditional branch counts, all Chunk tail checkpoints and outer final fields. It disables detailed sample breakpoints after that Chunk. It observes every selected tick invocation for snow/ice profiles 9 and 10; this seed reaches zero such invocations. Complete final `g`, `Q` and Random states are checked against an independent literal/RNG replay using the read-only observed Set order. No reported sample count is used to cap target execution.

## Transport and scope

The private `observations.json` contains `frame_records` (all 14 full original JSON records), `frame_rows`, `hash_rows`, numeric `sound_rows`, `cell_rows`, `range_rows`, `block_records`, `block_rows`, live-observation coverage and limits. The callback name is independently checked as `ambient.cave.cave` before numeric sound transport is formed.

`RANDOM_FRAME` fields are ID, scenario, outcome, g hex, Q, Random seed hex or `-`, flash m, P size, cache size, unique Chunk count, loaded Entity count, weather Entity count, sound count, cell count, range count. `RANDOM_HASH` is ID followed by SHA-256 of raw arrays, nibble arrays, heights, key/identity membership and final P iteration. `BLOCK_TICK` is ID, concrete class, Block ID, scenario, outcome, supplied Random state, World Random state, center ID/metadata, target ID/metadata, loaded Item count, scratch-present flag and scratch center.

Hash input visits requested keys in ascending X then Z. First-seen actual Chunk identities receive one-based ordinals. For each unique identity, raw and height arrays are int32 big-endian length plus bytes (`-1` for null); nibble input is the same framing for metadata, Sky and Block arrays in that order. Membership records six int32 values: requested X/Z, actual normal/fallback kind, actual Chunk X/Z and identity ordinal. Final P input is the actual iterator's int32 X/Z pairs. Leaves scratch is every int32 big-endian cell of the actual 32³ array. Provider insertion-list construction history is outside these hashes.

Authored observer failures retain their actual Java exception classes. A native IO failure can pair the reached source effects, but it does not prove the same Java Throwable implementation. Cached Chunk inputs, fixed manager assignment, aliases and fallback semantics are finite declared controls; these observations do not claim arbitrary generation, arbitrary callbacks, or arbitrary entropy-profile equivalence.
