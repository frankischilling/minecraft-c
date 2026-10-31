# Original Mob spawn eligibility

This CPU-only package runs unchanged original `gz.d()Z`, `ii.d()Z`, `ls.d()Z`,
`uw.d()Z`, `bg.d()Z` and `ar.d()Z`, plus real World and Chunk descendants. Ordinary and read-only
JDI runs must produce identical deterministic output. Java is reference tooling;
the game runtime remains C17.

```sh
python -B tests/reference/entity/mob_spawning/capture_reference.py \
  --client-jar /private/b1.7.3.jar --output .local/mob-spawning-new
```

Use matching JDK 17 java/javac, optionally selected with `--java`/`--javac`.
The output directory must be new. Generated observations, class-init logs and
helpers stay there. No original classes/assets or generated answer fixtures are
distributed. The wrapper checks archive/class/method pins, actual runtime
origins/resources, tools, authored source and compiled helper fingerprints.

## Genuine World fixture

The target genuinely constructs original Sessions, Overworld Worlds, Mob
receivers, normal Chunks and Blank Chunks. Session initialization prepares the
ordinary uu-before-gm registry context. The actual OK provider cache receives
actual `new lm(world,new byte[32768],0,0)` objects. Blocks and all three Nibble
arrays are authored inputs; normal cached Chunks have n=true. No loader,
generation or population result is substituted. These cases take actual
original cache-hit paths, without exercising generation or persistence.

Primary predicates use genuine FD methods throughout. Each independent input
clears actual Chunk Entity lists and World R/K lists, then explicitly sets
World.r seed 89 and the actual candidate's private Random seed. This is a
declared mutable source input using real Random.setSeed, separate from the
constructor entropy reference. No default-constructor clock profile is rebound.
Entity constructors still execute normally; their unrelated Math/private
entropy is not claimed equal across VMs. The source position setter updates
the real bounds, and genuine pool-reset utilities prepare independent inputs.

World fields i/j/k/l are zero unless thunder is selected. Position is
(8.5,8.5,8.5), World.q=1 and World.f=0 unless stated otherwise. Actual normal
Chunk coordinates and source seed reads remain authoritative. World seed 23 is
used for GZ and the rejecting Slime-chunk case. Seed 987234911 makes the normal
Chunk (0,0) yield a first nextInt(10) of zero with Slime's fixed salt.

## Finite predicate cases

Spider cn, Zombie uz, Skeleton fr and Creeper gb each run scenarios 0..10,
in that order: 44 genuine GZ/II/LS calls. Their own seed is zero except scene 0,
which uses 4096. Source sky/block light below means the actual Nibble cell
(8,8,8), with all other cells zero.

| Scene | Authored input | Original result |
| --- | --- | --- |
| 0 | Sky 15; seed4096 gives draw32=3 | false before block-light |
| 1 | BlockLight15 | false before II/LS |
| 2 | Dark empty Chunk | true |
| 3 | Stone at (8,8,8) | false at solid collision |
| 4 | Still water ID9 at (8,8,8) | false at liquid gate |
| 5 | Nearby live Spider, aF=true | false at no-Entity-collision |
| 6 | Nearby dead Spider, aF=true | true |
| 7 | Two duplicate nearby live Spider occurrences, aF=false/bq=true | true |
| 8 | Sky15; thunder i/j/k/l=1; old f=11 | true, f restored11 |
| 9 | Same thunder, old f=0 | false at late GZ path weight, f restored0 |
| 10 | Candidate itself in actual Chunk Entity list | false; source exclusion is null |

Nearby membership is installed through actual `lm.a(sn)` calls. Source World
queries and inherited null collision-shape methods run normally. The target
does not replace World collision, liquid, path-weight or light answers.

Slime runs scenes 0..11 at indices44..55, followed by two controlled failure
rows, then scene12 at index58. Size uses actual `uw.e(I)` and its real Byte16
watcher update. Own seed is zero except scene1, which uses 1.

| Scene | Size/difficulty and input | Original result |
| --- | --- | --- |
| 0 | size2/q0 | false after Chunk acquisition, no own draw |
| 1 | size1/q0; own nextInt10=5 | false before Chunk Random |
| 2 | size1/q0; Worldseed23 | false at Chunk draw9 |
| 3 | size1/q0; Worldseed987234911 | true |
| 4 | size2/q1; same World seed | true |
| 5 | scene4 with y16 | false after both draws |
| 6 | scene4 with yNaN | false after both draws |
| 7 | actual size helper -1, q1 | true; signed Byte and independent UW path |
| 8 | position(40.5,8.5,-23.5), actual new OK(world,null,null) | true via real Blank fallback |
| 9 | x1048584.5, natural YY32 key alias for requested Chunk65536,0 | true via cached actual LM0,0 |
| 10 | actual candidate World null | actual UW.d NullPointerException at PC18 |
| 11 | actual World.x Info null | actual FD.s NullPointerException at PC4 |
| 12 | Worldseed186642153918107, size2/q1 | false after real seeded-Random rejection |

Scene8 uses the exact Blank object created by the genuine OK constructor.
Its own coordinates are (0,0), despite the requested coordinates (2,-2).
Scene9 is a real source YY key alias, not an acquired-coordinate inference or
an authored World override. Both exercise original FD/OK acquisition.

Scene12 selects a World seed whose xor with UW's fixed salt987234911 creates
the desired fresh Random. Its first31 bits2147483647 are rejected by actual
nextInt(10); a second advance returns9. Own nextInt(10) still returns0. The
validator checks both actual return states and the independent rejection rule.

## Explicit controlled virtual-callee boundary

Indices56/57 are separately labelled `controlled_cn`, not genuine-FD leaf
qualification. Authored `FailureWorld extends fd` overrides only n(III),
forwarding to the real super method until its configured second or third call
throws an authored IllegalStateException. All original GZ/II/LS bodies remain
unchanged. Second-call failure witnesses World.f=10 persistence; third-call
failure witnesses the already restored f=11 during late brightness. There is
no source finally. These rows prove the outer source prefix and side effects,
not execution of the overridden failing FD.n body. They do not establish an
equivalent native child-failure mechanism.

The genuine null-World/Info failures remain separate. Fixture restoration of
Info/provider references occurs only after recording the failed source call,
to prepare the next independent input; it is not source failure cleanup.

## Appended animal and Squid profiles

The original capture-e records and their indices remain unchanged. New animal,
water and path-component records are appended after them. All use genuine FD,
OK and LM objects and real constructor-created actors; no virtual World callee
is replaced for these profiles. They retain the same explicit source Random
inputs, coordinates and independent-input cleanup described above.

Pig wh, Sheep dl, Cow bx, Chicken ww and Wolf gi each run BG scenes0..9.
Wolf declares no lowercase d override: its actual eligibility dispatch is BG,
then II/LS and the actual virtual BG path-weight method. Grass is actual block
ID2 at (8,7,8), below bounds minimumY8.5. Light cells are (8,8,8).

| Scene | Authored input | Original result |
| --- | --- | --- |
| 0 | No grass and no light | false before plant-light/LS |
| 1 | Grass and Sky8 | false at plant-light threshold |
| 2 | Grass and Sky9 | true |
| 3 | Grass/Sky15 and stone at (8,8,8) | false at solid collision |
| 4 | Grass/Sky15 and still water9 at (8,8,8) | false at liquid |
| 5 | Grass/Sky15 and nearby live Spider aF=true | false at no-Entity-collision |
| 6 | Same with dead nearby Spider | true |
| 7 | Two duplicate live nearby aF=false/bq=true Spiders | true |
| 8 | Grass/Sky0/BlockLight9 | true |
| 9 | Grass/Sky9 and World.f15 | true; FD.m ignores subtraction |

BG checks the actual block below first, then actual FD.m plant-light >8, then
enters original II/LS. Only after LS succeeds does II invoke virtual BG.a(III).
With unchanged grass data, BG.a returns float10 without a brightness query.
The validator checks these real branch checkpoints and late virtual dispatch.

Nine Squid xt profiles invoke actual AR.d, which only delegates to genuine
World no-Entity-collision. Scenes0..3 use dark air, stone, still water9 and lava11
at the candidate cell; all return true. Scene4 has a nearby live aF=true Spider
and returns false; scene5 makes it dead and returns true; scene6 uses two live
duplicate aF=false/bq=true occurrences and returns true; scene7 publishes the
candidate itself and returns false. Scene8 places it at y200.5 and returns true.
JDI verifies that no II/LS, plant/block light, collision-list or liquid gate is
called. These are predicate inputs, not proof that the earlier World spawn
caller would select those locations or materials for a Squid.

Six additional component records invoke the unchanged protected BG.a(III)
body on an actual Pig through ordinary reflection. Grass returns10. Without
grass, dark/Sky15/Block11/Block12/Sky15-with-f15 inputs exercise the real
FD.c brightness-minus-.5 path, including both signs around the threshold.
The target separately observes actual World brightness after the component
return; the validator checks the float subtraction. These are explicitly
component entries, not successful bypasses of BG.d's grass requirement.

## Real Chunk Random and World leaves

Thirty direct rows invoke actual LM/LI `a(J)Random`. The first28 cross normal
and Blank with seven coordinate pairs and salts0/987234911. Coordinates include
negative, 46341, 65536 and int extrema, preserving the source mix of wrapping
int products, signed widening, long products and long sums. The Chunk's actual
World and actual Info seed are read by the original methods. Each returned
Random must have a distinct actual identity, held by an authored identity set;
the first bounded draw and before/after 48-bit states are recorded.

The last two rows use Worldseed186643064605892, Chunk(0,0) and salt0 to force
the same real rejection path independently in both LM and LI. A Blank's method
is its own original override, not an assumed no-op or normal-Chunk substitution.
These direct leaves do not establish FD acquisition at their large coordinates.

Four additional failure rows cross LM/LI with null Chunk.World or null
World.Info. Read-only JDI records actual failure PCs8 and4 respectively; no
bounded draw returns. Three actual FD.B leaves use .9f, nextUp(.9f) and NaN.
The original results are false/true/false because the comparison is against
double .9 after float thunder strength, not against rounded .9f.

## Observations and evidence

`source-pins.json` binds the complete supplied JAR, full class hashes and method
Code identities. `operation-map.json` records static field/call PCs. JDI
independently verifies 19 live original method bodies, records reached branch
checkpoints and source exceptions, and reads actual own/temporary nextInt
returns and states. It never writes a target field, invokes a target method or
rewrites a source class. Watching authored activeCase only enables/disables
observer event requests. Constructor callbacks outside active predicates are
not represented as predicate effects.

The validator compares ordinary and JDI transport exactly, independently
replays the 48-bit LCG and all seeded-Chunk width rules, checks actual light,
collision, liquid and late path-weight branch reachability, and checks temporary
thunder writes/restores at their real checkpoints. The exact original
initialization closure, actual runtime resource hashes and JAR origins are
checked. Device/Minecraft bootstrap remains excluded.

Generated `observations.json` contains `state_rows`, `chunk_rows`,
`thunder_rows`, `animal_rows`, `water_rows`, `path_rows`, `original_rows`,
`checkpoints`, `random_events` and `exceptions`.
The raw normal/debugger NDJSON and class-init logs remain alongside it.

`ELIG_STATE` has 13 cells: prefix, index, kind, scenario, outcome(-1/0/1), actual
own seed48, actual World.r seed48, World.f, AABB cursor, Vec3 cursor, actual
World.R size, World.K size and actual provider cache size. Seed words are 12
hex digits. Controlled rows are explicitly tagged in `original_rows`.

`ANIMAL_STATE` and `WATER_STATE` use the same 13-cell shape. Animal indices are
kindIndex*10+scene, with kinds wh/dl/bx/ww/gi in that order; water index equals
scene. Their read-only checkpoint IDs are respectively4000+index and5000+index.
`PATH_WEIGHT index wh scene resultHex` component indices0..5 use checkpoint
IDs6000+index. These separate namespaces preserve all original e row IDs.

`CHUNK_RANDOM` records index, Blank flag, actual j/k, salt64, actual World
seed64, actual fresh seed48, nextInt10 and postseed48. `THUNDER` records index,
raw input strength bits and actual result. Failures preserve actual exception
class/top frame and JDI source PC rather than fabricated successful outputs.

Finite qualification covers these prepared cache/data predicates and leaves.
It does not establish arbitrary generation/population, concurrent World-field
mutation, custom virtual Worlds, spawn publication/postspawn/group limits,
arbitrary passive/water inputs, gameplay spawning or server behavior.
