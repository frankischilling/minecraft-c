# Held World named MapData comparison

The `named-map-data` profile retains the seed23 origin World and publishes one
real MapData value through its World put method. Its constructor name is
`named_map_value`; the cache key is `named_map_lookup`. The original inherited
dirty method marks the value before two actual full World saves. A separate
manager, constructed against the same handler, then loads the value by its name
and performs a second lookup. The World keeps its original SavedData manager.
This is a held-World experiment, not unload or gameplay restart.

The Java observer composes five unchanged public helpers. It checks exact
original constructors, declaring classes, field descriptors and method
signatures. Reflection reads the game state; only project measurement flags are
written. Expected state never comes from a native result. The native producer
constructs its own World, MapData and reader manager and uses the actual put,
dirty, full-save and load paths.

## Complete observations

Each side retains nineteen boundaries around the nine post-population
operations, plus five unchanged origin numerical states. The comparison checks
all twenty-four cache/numerical states, seventy-two copied Random records and
360 array descriptors. Each descriptor includes its complete payload, length,
type and alias relations. The constructor-seeded World Random remains an
independent input in each run; its state must stay unchanged within that run.

Every boundary includes all fifteen held Info fields, a complete disk inventory,
the actual Player list and both reached manager graphs. MapData observations
include the inherited name String and dirty flag, centers, signed dimension and
scale, update counter, every colors byte and three distinct tracker collection
owners. One reference bijection spans the entire reported semantic trajectory,
including constructor/load results, String arguments, map entries, ordered
occurrences, Class requests and array owners. It is never reset per boundary.

Original managers retain one shared handler object. Native managers each own a
copied path binding, reported and checked separately as `native_storage_binding`.
The native `handler_store` identifies the supplying World Store for source
handler correspondence; it is not presented as the manager's actual storage
field. Both native bindings must remain stable and distinct. Numerical arrays
use the probe's separate complete identity namespace. The check does not claim
one combined JVM/native address graph.

The first save writes a fresh gzip file under the value's name, with exactly
seven typed tags under lowercase `data`. All 16,384 default colors bytes are
zero. Dirty clears after that save, so the second save preserves the complete
named file. Reload returns a fresh clean MapData and a fresh decoded colors
owner while retaining the supplied name String. The next lookup returns that
same cached value without changing any captured state or file byte. Read calls
are not instrumented; the original source's early return and the complete
retained effects supply the cache-hit evidence.

Full saves also check thirteen typed metadata tags and all eleven saved Chunk
Level tags, including five complete arrays in each of four records. Session,
region timestamps and metadata LastPlayed must lie within their own measured
intervals. The first metadata samples SizeOnDisk zero before live growth reaches
24576; the second samples 24576. The second `level.dat_old` must exactly equal
the first `level.dat`. Complete region and session bytes remain unchanged after
the first save. Both final directories are reread after their processes exit.

## Public oracle and execution

`named_data_fixtures.json` losslessly retains both original-only observations and
their independently rederived projections. Only the two declared archive-URL
maps use logical supplied-archive names. Six Java source fingerprints protect
the compilation inputs. The package rejects changed source pins, incomplete
streams, invalid capture attribution or a stored projection that does not
derive from its original observation.

The initial package is 7,073,730 bytes with SHA256
`d308df2e8e21a1f8d0729053936c8a605cd960ca7fe93c77f64fe31eda22b32f`.
Its capture records a dirty development checkout and no tested commit.
That attribution stays intact after later compiler qualification.

Public replay runs the production native probe using declared project-authored
minimal translation resources. It needs neither Java nor the JARs. The optional
fresh comparison instead runs both supplied originals and uses their extracted
translation resources for the native side. Neither route imports expected game
state into production.

```text
ctest --test-dir build -C Debug --output-on-failure -R "^(tools.population_named_data|world.population_named_data_replay)$"
```

With `MC_ENABLE_POPULATION_REFERENCE_TESTS=ON`, the existing JAR options and
Java17 enable `reference.population_named_data`. Its capture records commands,
complete streams, source/tool fingerprints and both final Worlds. A supplied
tested commit must match clean source. Selected executable/source fingerprints
do not themselves attest a build or its transitive libraries; that linkage
belongs to the separate compiler qualification audit.

## Current limits

Development comparisons pass on MSVC against both original runtimes and the
public oracle. Clean-source GCC/Clang/MSVC qualification is in progress.
Constructed tests additionally cover shared-array lifetimes, arbitrary colors
lengths, signed scale, exact tag types, resize row spills and partial failures,
key aliases, first-identity removal, partial load caching and dirty clearing.
Those constructed cases are not fresh-original qualification of nondefault
maps or failure behavior.

This profile covers one default MapData value and one seed in the same held
World. Map item terrain updates, player trackers, map packets, other subtypes,
nonempty Player serialization, original filename and stream/error prefixes,
printed caught diagnostics, JVM allocation timing, unrestricted collection
ordering and gameplay restart remain unfinished or unqualified. Authentication
is the only approved full-game exception. The broader rewrite remains open.
