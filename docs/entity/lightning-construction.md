# Client Lightning construction

The client implements the actual `c(World,DDD)` Lightning constructor through
`c -> yu -> sn`. The public API is
[`mc/entity/lightning/construction.h`](../../include/mc/entity/lightning/construction.h).
Its concrete native allocation contains Entity fields and the separate C fields:
long `a`, int `b` and int `c`. It uses the existing canonical
`MC_ENTITY_CLASS_KIND_C` literal and the application's Entity classes and Random
profile. Lightning has no Living fields or constructor Math draws.

`mc_lightning_allocate` initializes the Entity class before allocating the
concrete header. Allocation publishes one RAW occurrence without consuming an
Entity ID or entropy. `mc_lightning_construct` accepts that occurrence once and
requires a nonnull World to use the same Entity and Random-profile owners.
Construction ends READY only after the real C return at PC262. A reached failure
ends FAILED_CONSTRUCTION and preserves the completed source prefix.

The authority is the unchanged Beta 1.7.3 client JAR, SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The implementation follows these source operations in order:

| Source PC | Operation |
| --- | --- |
| YU2 / SN constructor | Construct the real Entity, consume its ID, allocate its bounds, create its ordinary instance Random, bind the nullable World and register Byte0. C's early virtual watcher hook is an empty return. YU returns at 5. |
| C7 | Store own long `a=0`. |
| C18 | Invoke inherited `sn.c(DDDFF)` with the supplied coordinates and zero yaw/pitch. It stores current and previous coordinates, adds the actual Entity offset to Y, then positions the same bounds. |
| C23 | Store own int `b=2`. |
| C31 / 34 | Call the instance Random's `nextLong`, then store own `a`. |
| C43 / 48 | Call that same Random's `nextInt(3)`, add one and store own `c`. |
| C52 | Read World difficulty `q`. A null World throws here, after pose and both own Random calls. Difficulty below 2 proceeds to return262. |
| C76 | Floor the original coordinate arguments and invoke the actual radius10 area-presence check. False proceeds to return262. |
| C109 / 115 / 125 | Read the center block. Only air reaches Fire static active use and the real Fire can-place predicate. |
| C138 / 141 / 144 | When placement is allowed, reload the Fire static, read its actual ID and invoke the notifying World setter. |
| C166 / 184 / 202 | For each of four neighbor attempts, independently floor each original coordinate and draw `nextInt(3)` for X, Y and Z, adding the draw and subtracting one with Java int semantics. |
| C217 / 223 / 233 / 246 / 249 / 252 | Read that sampled block; air reaches Fire active use, can-place and, when allowed, the notifying setter. |
| C262 | Return normally. |

The Random calls above use Lightning's instance Random. They do not draw from
World Random or the shared Math stream. `nextLong` preserves Java's signed low
int addition; each bounded draw preserves Java rejection sampling. No new seed,
replacement stream or work cap is inserted between source operations.

Fire can-place executes its actual below-block solid-support query first. If
that returns false, it checks encouragement at +X, -X, -Y, +Y, -Z and +Z in
order. Each test captures the real Fire object's encouragement array before
acquiring the neighbor block ID, then indexes that same array. The notifying
setter retains its ordinary World, Chunk, hook and callback behavior. A reached
missing Fire body remains an explicit failure at the corresponding constructor
call; an earlier block write or Random advance is retained.

`mc_lightning_inspect` borrows the actual own fields, source progress, sampled
coordinates and failure record in every phase. Construction cannot be replayed
after a reached failure. Generic Entity retain/release supports RAW, READY and
failed Lightning objects. `mc_lightning_retire` consumes only a final inactive
occurrence and leaves a BUSY slot unchanged.

A failed notifying mutation can carry a WorldAccess callback receipt. The
failed Lightning object retains its own occurrence of that exact receipt and
releases it during generic Entity final release. Error outputs and inspection
views borrow this occurrence; copying an error adds no hold. A caller that
needs the receipt after Entity release must retain a separate occurrence.
The World random-Chunk tick does so for its retained failure. Native ownership
tests check that receipt retirement stays BUSY through the Entity's lifetime
and through an independent external capture afterward. Native limits and
unsupported children remain distinct from source exceptions.

The mandatory native test is
[`tests/entity/test_lightning_construction.c`](../../tests/entity/test_lightning_construction.c).
It needs no JAR or Java. It checks actual concrete storage, source phases and
pose, the early typed watcher, one ID per reached base constructor, own Random
progress from fresh declared clock inputs, absence of Math initialization,
null-World failure at52, allocation prefixes, namespace admission and terminal
receipt ownership. Run it from a configured build:

```sh
ctest --test-dir build -R '^entity\.lightning_construction$' --output-on-failure
```

For a Windows multi-configuration build, add `-C Debug` to CTest. The optional
native probe also runs the mandatory contracts, then transports five actual
constructors from declared RAW GAME World scopes:

```sh
python -B tests/tools/entity/test_lightning_construction.py \
  --executable build/mc_test_lightning_construction \
  --work-dir .local/lightning-native
```

Use `build/Debug/mc_test_lightning_construction.exe` for the corresponding Windows
executable. The [original reference package](../../tests/reference/entity/lightning_construction/README.md)
constructs a complete original World and executes five unchanged constructor
calls in each of an ordinary VM and a read-only JDI VM. Its cases are difficulty
0, difficulty 1, difficulty 2 with the radius outside the vertical World range
above and below, and a null World. Four return; the last throws at C52. It pins
176 initialized original classes, seven live method bodies, 84 checkpoints,
ten actual default-Random clock returns and five observed private-Random profiles.

```sh
python -B tests/reference/entity/lightning_construction/capture_reference.py \
  --client-jar /private/b1.7.3.jar --output .local/lightning-original-new

python -B tests/tools/entity/test_lightning_construction.py \
  --executable build/mc_test_lightning_construction \
  --work-dir .local/lightning-native \
  --observations .local/lightning-original-new/observations.json
```

The original capture needs matching JDK17 tools and a new output directory.
The native comparison needs only the resulting observations. It compares
constructor fields, pose, bounds, typed watcher, relative ID consumption and
zero Math draws within the two stated World contexts. It does not establish
equivalence between a complete original World and a native RAW World.

Both Windows MSVC Debug and Ubuntu Linux Clang runs passed 440 checks and
transported five actual native constructors. Against original capture
`lightning-construction-original-20261030b`, each host matched 594 independent
deterministic field comparisons, including 66 comparisons from the failed
object observed only through JDI at C52, plus 35 separately labelled scalar
checks. The ordinary failed object is unreachable and has no fabricated field
row. Inputs and executables retained their before/after fingerprints in each
comparison run.

Independent native and JVM constructors have independent entropy. Actual
native Random progression is checked against fresh declared clock controls.
Observed original private pre-seeds drive a separate plain local Random for
scalar replay of `a` and `c`; that local value never replaces an Entity Random
or rebinds a profile after World setup. The comparison excludes those stochastic
fields from direct constructor equality and records scalar results separately.

The constructor can still reach unfinished Fire addition or neighbor bodies
through its setters. Those selected paths retain explicit failures and their
completed effects. Fire ticking also remains unfinished. The five original
cases above do not qualify fire placement, notifying mutation failures or
weather publication. Lightning's own tick, rendering and NBT behavior remain
unfinished. These constructor checks do not establish a complete Lightning
lifecycle or a complete World tick.
