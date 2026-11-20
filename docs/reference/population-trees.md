# Tree feature source map

The verified [client and server archives](binaries.md) supply the authority
for all five tree feature classes. Complete paired bodies match after explicit
class/World/Block aliases and the known quiet-setter name mapping; PCs,
opcodes, constants, branches and field order remain part of the comparison.

| Feature | Client / base-server | Complete instructions per side |
| --- | --- | ---: |
| Oak | `yh` / `pn` | 258 |
| Birch | `k` / `h` | 260 |
| Tall taiga | `pw` / `jt` | 278 |
| Dense taiga | `ws` / `on` | 288 |
| Big oak | `ih` / `fn` | 1,475 |

The total is 2,559 instructions per side, including big-oak helpers,
constructors, scale and static initialization. The shared `pg` / `jm` base
constructor has no retained feature fields; its inherited scale method only
returns. The [native interface](../world/population/trees.md) therefore keeps
small feature entries stateless while retaining the actual big-oak owner.

Small-tree clearance queries occur at PCs 141/141/171/165 in table order.
Their soil queries occur at 204/204/234/228. Live grass/dirt comparisons,
independent dirt-write reads, captured opacity references and leaf/log static
reads remain at their distinct original uses. Broadleaf corner draws include
top corners whose later condition rejects the candidate. Dense-taiga radius
and trunk-shortening draws retain their positions after soil and foliage work.

Big-oak construction invokes its actual default Random at constructor PC 9.
Placement stores/retains World `c` at 2, consumes provider `nextLong` at 6,
and draws a height only when retained `e` is zero, at 60. Location, planning,
leaf, trunk and branch calls occur at 68/77/81/85/89. Scale's floating work
begins at 5. Errors retain the outer entry phase separately from the deepest
helper PC; native ownership admission is not relabeled as a source invocation.

The implementation uses the real cache getter and quiet setters. Small trees
retain live named Blocks and captured opacity arrays; big oak retains its
source literal block IDs. Neither substitutes a generated numeric block
palette. Java wrapping arithmetic, signed division, narrowing and floor are
explicit. Big-oak floating evaluation order, exact constants and private
Random ordering are implemented, but host transcendental equivalence remains
unqualified.

Focused native MSVC and sanitized GCC/Clang runs succeed with 661 selected source
inputs and 254 external zlib source pins unchanged before/after. The combined
query/plant/tree/geology result is 6,062 checks. The contracts exercise actual World/Registry/Chunk owners,
source-derived Random budgets, real default-constructor profile prefixes,
binding transfer/disposal, simultaneous allocation limits and partial writes.
This is a focused native run, not the complete declared public suite or an
original replay.

The public contracts inspect the actual private owner definition through its
production-private header. They check known private Random prefixes and selected
zero-density tails, retained node identities/rows and simultaneous replacement
budgets. This finite coverage does not qualify arbitrary private Random tails
or all host numeric results.

Separate original seed0 experiments invoke constructor, scale, actual World
height and whole placement in order for every form on both sides. All ten
return true at height64. They retain full array-image dictionaries, actual
supplied/World/private Random observations, 27 fresh compiled helper classes,
complete raw streams and separate JVM class-load/init logs. An offline audit
rehashes every recorded array image and binds class, stream, log and world-file
inventories. Constructor nanoTime inputs remain unobserved. The inherited
snapshot records E/F field identities, not full scheduler entries or sequence.
No helper-free control or native replay is supplied by these ten calls.

Independent original/control/native comparison,
full successful private-Random tails, cold/reentrant field timing, extreme
numeric cases, all three cache-triggered routes and save/unload/reload
qualification remain pending. The complete mapped bodies and required
callback closure are broader than the finite native fixture corpus.
