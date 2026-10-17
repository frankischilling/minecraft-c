# Normal density and base terrain

`mc_world` contains the allocation-free C17 module at
`src/world/terrain/terrain.c`, with its interface in
`include/mc/world/terrain/terrain.h`. It reproduces the normal-dimension density
pass and the following base-block interpolation. It initializes no world,
files, graphics, assets or Java. [Original evidence](../reference/density.md)
records the actual methods and observation setup.

## State and normal shape

`mc_terrain_normal_init` builds eight Perlin/octave groups from one explicit
seeded Random: 16/16/8/4/4/10/16/8 layers, 82 total. Surface and later-consumer
groups remain in constructor order even though this pass does not sample them.
The state retains the Random continuation immediately after construction.
Sampling leaves the entire state unchanged. It contains no internal pointers
or allocations, so callers can copy it or share it between independent readers.

The normal shape is fixed: density dimensions X/Y/Z are 5/17/5, and base-block
dimensions are 16/128/16. Both flat layouts put Y fastest:
`(x*5+z)*17+y` for density and `(x*16+z)*128+y` for blocks. This interface follows
the actual normal consumer; arbitrary private-method shapes remain outside its
contract. Surfaces, bedrock, caves, population, lighting and complete chunks
require subsequent generation passes.

## Climate and workspace

The caller supplies processed 16×16 temperature and rainfall arrays, each with
its own length. Before normal generation, sample the biome manager at separately
wrapped Java int origins `chunk_x*16` and `chunk_z*16`, then pass its current
`a` and `b` arrays. Density samples climate positions 1,4,7,10,13 on each axis.
It does not refresh climate or recover an earlier biome request after a point
lookup changes that manager's retained fields.

Density separately wraps `chunk_x*4` and `chunk_z*4` before converting origins
to double. Deriving those origins from an already wrapped climate origin would
change large-coordinate results.

`mc_terrain_normal_workspace` borrows five double spans. `mix`, `low` and `high`
need at least 425 values each; `column_scale` and `column_bias` need at least 25.
Every density request clears each full scratch span, including its suffix,
then executes the original five octave requests. Column channels use the
improved-noise height-one path, and the other channels use its general bulk
path. [Perlin bulk behavior](noise.md) remains part of this result.

## Sampling operations

| Function | Result |
| --- | --- |
| `mc_terrain_normal_density` | Fills the first 425 density doubles and retains a larger output suffix; scratch spans are fully cleared |
| `mc_terrain_normal_base` | Interpolates an existing density grid and temperature grid into all 32768 base bytes, retaining a larger byte suffix |
| `mc_terrain_normal_generate` | Composes density and base interpolation after validating every argument; keeps the density output available |

Density preserves the original operation order, strict branch comparisons and
float top fade before promotion to double. Base interpolation updates values
through repeated additions; replacing it with a closed weighted expression can
change a solid/air decision near zero. The base output contains air 0, stone 1,
stationary water 9 and ice 79. These are observed original block IDs. The sea
surface is at Y=63; cold temperatures strictly below 0.5 can produce ice there
when positive density does not select stone.

## Ownership and failures

Callers own the state, climate, five workspace arrays, density and block output.
The module allocates nothing. Initialized state and actual backing storage are
preconditions. Mutable spans must be mutually disjoint and disjoint from state,
descriptors and read-only inputs. Read-only climate arrays may overlap each
other, as may the two read-only inputs to the standalone base operation.
Descriptor values remain unchanged.

Null pointers, short spans and double-span byte overflow return false before
any output changes. The composed operation validates the block output before
sampling density, so rejected calls preserve workspace, density, blocks and
constructor state. The complete declared double spans must be representable;
only the normal prefixes of read-only climate/density inputs are consumed.
Pointer-range overlap is a caller precondition, not a runtime check.

Exact finite comparisons assume binary32/binary64 round-to-nearest without
floating contraction on the tested x86-64 targets. Arbitrary double inputs are
accepted, but exact NaN payload identity is not promised. See
[testing](../testing.md) for fixed regressions and local original comparisons.
