# Concrete normal provider generation

The private `mc_world_provider_generate_game` operation uses the normal
provider constructed by an actual GAME World. It reseeds that provider's
existing Random, constructs a store-owned Chunk, samples actual Biome
references, generates terrain, replaces the surface, carves caves and calls
the [full Chunk finalizer](chunk-objects.md). Success returns that same Chunk
before cache publication. The public [numerical generator](generation.md)
retains its separate preflight and failure contract.

Ordinary GAME acquisition now calls this operation before publishing and
[activating the same Chunk](chunk-activation.md). Its population gates call the
[complete normal controller](population/normal.md) on that same owned provider.
Reached unfinished descendants retain real partial writes and failure flags.
Complete startup, natural population, propagation and gameplay persistence
remain unqualified.

## Live state and source order

The provider owns one generator and both real Random continuations. Generation
mutates them in place. It does not call the numerical generator, stage a Random
copy, convert numerical biome IDs into objects or install a material palette.
Branch-specific Block reads perform the actual Registry active use, then read
the selected receiver's full `bn`. Signed byte comparisons and later narrowing
remain separate operations.

Direct sampling captures the current World/Dimension manager and publishes
the returned actual `v` only after success. Base interpolation independently
captures the current temperature array. Density independently captures current
temperature and rainfall arrays, fills its five noise channels, then reaches
their element reads. NULL captures therefore fail after those fills, rather
than at an early climate preflight. Provider `q` publishes only after density
returns. Surface captures its actual Biome array and each column's receiver;
the three column draws precede the first `p` dereference.

The fixed provider reservation backs nullable source-array views. Constructor
`r/s/t` already have their 256-value spans; `q/d/e/f/g/h` initially remain
unpublished. Reused arrays retain completed writes on failure. This native
reservation does not reproduce Java object sizes or individual allocation
failure timing.

## Lifetime and failure

The entire operation holds the World and leases the exact constructed Chunk
across callbacks. Each climate capture separately retains its actual allocation
and supplying manager. Resizing a field can replace its array while an earlier
capture still reads the old array. Both requests stay charged to the original
memory owner until their last holds retire. A manager hold alone would not
preserve replaced storage.

The provider retains the supplier of its current `v` so its Registry arena
remains live. Each entered generation also retains its own captured supplier
through reentry. No array owns its Registry cyclically, and no owning manager
descriptor is copied.

Failure preserves the ordinary output and every reached Random, class,
climate, scratch and lighting effect. After releasing local holds, cleanup
uses checked retirement on the exact unpublished child and captured store.
A child retained by legitimate reentry stays store-owned; diagnostics expose
its identity/store and cleanup failure separately from the first cause.
Nothing force-frees a referenced Chunk or rolls back shared state.

Diagnostics retain the outer generation PC and separate base, density,
surface, cave and Chunk frames, together with full Registry, sampling and
lighting causes. Native admission is distinct from source receiver/element
failures. These records are diagnostics, not Java Throwable identities.

## Wrapped cave scheduling

The cave scheduler reads current World seed initially and for every reached
neighbor. Its signed loop bounds and increments use defined Java wrapping.
A wrapped lower bound greater than the upper bound produces no neighbors,
after the initial reseed and two long draws.

Some extreme coordinates make the original wrapped loop nonterminating.
Native scheduling has an explicit limit of 65,536 work units, counting entered
outer iterations and entered neighbor iterations. Ordinary range-eight
scheduling uses 306 units. Exhaustion reports a late native LIMIT at
generation119/scheduler123 and retains preceding terrain/surface effects.
It does not report an original Java exception or reject coordinates before
the generator reseed. The numerical API keeps its existing finite domain.

## Verification boundary

All 50 selected native cases pass on MSVC, GCC and Clang, including 212 new
provider checks. Linux runs use AddressSanitizer, UndefinedBehaviorSanitizer
and leak detection. The three builds retain identical before/after source
inputs and their exact products. These selected checks do not constitute a
fresh complete local suite or a new original JVM observation.

Run `ctest --test-dir <build> -R '^world.provider_generation$' --output-on-failure`.
The native test uses real World/store/Registry owners, an empty provider cache,
checked retirement, resource failures, Dimension rebind, actual table-cell
mutation with restoration, native budgets and captured-array lifetimes.

The driver compares observed opacity, thirteen actual Biome `p/q` pairs and
warmed climate arrays with both retained original seed23 inputs. It compares
all 82,176 generated block, metadata, skylight, block-light and height bytes
for chunk `(1,0)`. It does not replay the original cached baseline's queue,
height-query trace or World constructor route, and does not execute a new JVM
observation. Existing numerical fixture replays separately cover all four
seeds and nine-call sequences. The [reference notes](../reference/generation.md)
describe that retained corpus and its limits.
