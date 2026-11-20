# World propagation reference

This harness runs a new original Beta 1.7.3 client World and a new production
native World, then compares their observed state exactly. It exercises genuine
construction, chunk acquisition, population, light submission and propagation.
The Java helper observes the unchanged supplied game JAR; the C helper calls the
production owners and APIs. Neither helper implements terrain or propagation.

Every profile supplies one explicit diagnostic input: World seed 23 and, only
after the genuine constructor returns, `World.r.setSeed(0x0123456789abcdefL)`.
The native equivalent reseeds that same World's existing `fields.r` owner with
`mc_java_random_reseed`. Generator and cave Random owners remain separate and
are never reseeded by the fixture. Production construction and default entropy
are unchanged. These controlled fixtures establish no uncontrolled startup
transient-parity claim and create no exception to the remaining parity work.

## Profiles

| Name | Acquisition and drain order |
| --- | --- |
| `view` (default) | Construct the World, acquire the inspection's 72 padded section caches in X/Z/Y order, then drain the resulting actual 25 cached chunks. |
| `fresh` (opt in) | Construct the World, then acquire the actual Block at spawn plus X outer/Z inner offsets -128..128 step 16, at Y64, draining after each of the 289 points. |
| `warm` (opt in) | Complete `view`, retain that World, then complete the same 289-point grid. |

The original `view` creates real `ew` caches. Each section's nine chunk getters
run X outer/Z inner. The native observer makes those same production getters in
the same order. Cache hits still occur at each source acquisition site. Drain
calls continue until the actual `fd.j()Z` or native equivalent returns false;
there is no assumed drain count or numerical tolerance.

The native fixture supplies a policy before World allocation: 262144 scheduled
entries, 16 MiB World-direct bytes and 512 cached/provider chunks. The provider's
8 MiB default remains in effect. Java collections receive no native resource
policy. Observation limits are 10000 drain calls per point, 512 MiB of ordered
queue capture and the runner's process timeout. Exceeding one fails the capture.
Each version-tool output stream is limited to 64 KiB; each compiler or game
output stream is limited to 16 MiB. Overflow kills the process, preserves its
bounded prefix and records a failed capture. Timeouts also kill and fail.

## Build the native observer

The observer is CPU-only C17. Build it with the same configured source and
archives as every linked production owner. Its passive generator-Random reads
and store retirement use private declarations, so mixing headers and archives
from different checkouts or ABIs is invalid.

The repository's reference integration can provide
`mc_world_propagation_reference_probe`. Its complete target definition is:

```cmake
add_executable(mc_world_propagation_reference_probe
    tests/reference/world/propagation/native.c
    tests/reference/world/propagation/owner.c)
target_include_directories(mc_world_propagation_reference_probe PRIVATE
    src/world/runtime src/world/storage src/platform/time)
target_link_libraries(mc_world_propagation_reference_probe PRIVATE
    mc_simulation mc_resources mc_platform_time mc_c_atomics mc_build_options)
```

Then build the configured target, for example:

```powershell
cmake --build build-reference --config Release --target mc_world_propagation_reference_probe
```

```sh
cmake --build build-reference --target mc_world_propagation_reference_probe
```

No client display, SDL, OpenGL, OpenAL or original game assets are required.
The runner writes a small authored localization fixture for native Registry
initialization. It does not extract or publish the original resources.

## Run with explicit JDK 17 tools

Use Python 3.9 or later, an explicitly supplied JDK 17 `java` and `javac`, the
supplied client JAR, and the freshly built native observer. The runner checks the
JAR and selected class/Code metadata before compiling the Java helper. It
removes ambient Java option variables, enables `-Xverify:all`, opens only the
JDK Random fields needed for observation and uses a classpath containing the
new helper directory and supplied JAR. No LWJGL or JInput dependency is needed
by these profiles. The loaded `fd` class independently checks its actual JAR
origin and hash.

Windows, from the repository root (replace the explicit installation paths):

```powershell
python -B tests/reference/world/propagation/compare.py `
  --client-jar C:/private/b1.7.3.jar `
  --java 'C:/Program Files/Java/jdk-17/bin/java.exe' `
  --javac 'C:/Program Files/Java/jdk-17/bin/javac.exe' `
  --native-probe build-reference/Release/mc_world_propagation_reference_probe.exe `
  --work-dir .local/world-propagation-runs
```

Linux, from the repository root:

```sh
python3 -B tests/reference/world/propagation/compare.py \
  --client-jar /private/b1.7.3.jar \
  --java /opt/jdk-17/bin/java --javac /opt/jdk-17/bin/javac \
  --native-probe build-reference/mc_world_propagation_reference_probe \
  --work-dir .local/world-propagation-runs
```

Append `--profiles fresh warm` for the expensive full grids, or
`--profiles view fresh warm` for all three independent World pairs. Each profile
starts with its own new World. `--jobs 1` serializes processes if memory is
limited. The default timeout is 1800 seconds per process; `--timeout` changes
that explicit observation limit. Missing supplied files return 77. Incorrect
versions, changed pins, process errors, nonempty stderr and unequal observations
fail. Each run gets a new output directory; earlier worlds are never reused.

`--tested-source-id` records a caller-provided checkout identity.
`--native-build-manifest PATH` retains the caller's exact source/archive build
record and its hash. The observer's selector-count diagnostic alone cannot
prove ABI consistency. Keep the full build record when comparing cohorts.

## Compared observations

The comparator computes both answers in the current run. It stores no captured
terrain, light arrays, Random tapes or scheduler answers in this package.

- Every observation boundary compares C/E/F/cache counts, M, the actual drain
  return, point/call order, and all three Random owners' 48-bit state, Gaussian
  cache flag and raw cached-double payload. Acquisition events and every
  completed drain call are retained.
- `queues.bin` records every actual C region oldest first at each boundary:
  event/phase/point/call/count, then actual Light tag and all six signed bounds.
  Entire transports must match byte for byte. No bounds are sorted or merged by
  the observer.
- `constructed.chunks`, `view-undrained.chunks`, `view-drained.chunks` and
  `final.chunks`, as applicable, contain every actual cached chunk's Blocks,
  Metadata, SkyLight, BlockLight and HeightMap arrays. They compare byte for
  byte under exact concrete coordinates. Map iteration order is disregarded;
  array contents are never normalized. The constructed profile currently
  acquires one real chunk, while the retained view acquires 25.
- `scheduler.bin` retains every F record's x/y/z/Block, due time and private
  sequence, canonically sorted only for transport. E retains actual structural
  iterator order. Each E entry must also be the same actual owner retained in F,
  checked by Java identity and native pointer identity. HashSet iteration order
  is not a compared property.

All binary integers use big-endian transport. Chunk files use `LIT1`, a count,
then x/z and arrays of 32768/16384/16384/16384/256 bytes. Queue files use `QC01`,
then five u32 event fields and 28 bytes per region. Scheduler files use `SCH1`,
F/E counts, 32 bytes per F entry and 36 per E entry including its alias flag.
`records.py` is the executable definition of these exact comparisons.

The private run directory retains commands, exit status, stdout/stderr, actual
World directories, generated observer classes, raw transports, tool versions
and hashes. `result.json` inventories the files and records measured coverage.
Keep generated files outside this source directory and out of publication.
Before acceptance, the runner checks that the authored C/Java/Python sources,
compiled observer classes, original JAR and native executable are unchanged.

## Original binary pins and paired metadata

`source-pins.json` identifies the client JAR and 22 reached or diagnostic owner
classes, including full classfile hashes, method descriptors, instruction-byte
Code hashes, complete Code-attribute hashes, access flags and stack/local bounds.
The pins are metadata, not classfiles or extracted source. Important entry seams
are `fd(wt,String,long)`, `ew(fd,IIIIII)`, `fd.a(III)I`, `fd.j()Z`,
`st.a(fd)V`, saved-light setter/recheck/exposure, and the concrete chunk/nibble
operations beneath them. The helper uses the real `mx` store and actual World,
provider, chunks, queue and scheduler owners. It uses no fabricated receiver,
Unsafe allocation, replacement provider, injected queue or replacement Random.

`paired-method-audit.json` separately records a reviewed 30-method client/server
instruction audit after symbol mapping. Its selected pairs cover drain, region
propagation, setters, requeue, exposure, presence, acquisition, saved reads,
submission, merge, normal/blank chunk paths, nibble access and Light construction.
Supplying `--server-jar PATH` verifies that audit's supplied server JAR, class and
selected method/Code metadata. The portable runner does not rerun the audit's
normalization or execute a server World. Downstream server diagnostic frames
remain outside this verification.

## Scope

These are finite controlled profiles, not a proof of every within-call event,
every seed, default-entropy transient parity or all World fields. The full-grid
profiles stop after acquisition and lighting drain: they do not invoke `fd.p`,
construct a Player, run simulation ticks or exercise gameplay. The default view
also does not compare meshing, graphics or pixels. Authored propagation unit
checks remain a separate test layer. `validation.json` records the actual
verified configurations without making them oracle inputs.
