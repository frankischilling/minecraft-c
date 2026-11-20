# Grass and Leaves tick diagnostics

The random Block tick helper `rb_get` calls the existing internal Block and
metadata failure helpers. It reads their acquisition diagnostics only after a
failure, so successful queries no longer initialize an unused complete record.
The public detailed getters still produce their complete success and failure
outputs.

Both contracts use the same Block or metadata query body. The change preserves
the actual provider lookup, current Chunk and array reads, leases, source call
positions, callbacks, Random consumption and output preservation. Reached
failures still carry their complete acquisition and nested error records.
The helper performs each query once and retains its preceding effects.

The measured candidate changes only the two getter callees in `rb_get`.
The raw random-Chunk accessor and Registry `n_get`/`n_at` failure helpers were
already in use. This result does not measure those earlier changes again.

## Windows CPU comparison

The [Windows receipt](../../tests/benchmarks/random_blocks_windows.json) retains
eight fresh processes in detailed/failure, failure/detailed, detailed/failure,
failure/detailed order. Each process runs two warmups and seven measured samples
for each of three workloads. The final formatted cohort passes all 168 measured
samples, with 28 samples per variant and workload.

| Workload | Detailed median ns/call | Failure median ns/call | Reduction |
| --- | ---: | ---: | ---: |
| Bright Grass | 3706.097 | 3366.246 | 9.170% |
| Leaves with decay flag clear | 1995.621 | 1841.682 | 7.714% |
| Connected flagged Leaves | 367278.711 | 229325.781 | 37.561% |

Reductions use the unrounded medians and include every prescribed sample.
An earlier cohort also passes 168 measured samples. Its captures remain separate
from the final formatted cohort; the table does not pool the two cohorts or
select a favorable subset.

Each sample uses a fresh genuine GAME World and one saved, cached normal Chunk.
Bright Grass receives 32,768 calls at a valid Grass center. Its first westward
spread changes the authored Dirt target; later calls continue with the resulting
terrain and supplied Random. The flag-clear Leaves workload makes 32,768 real
metadata queries and returns without allocating scratch. The connected workload
visits 512 distinct flagged Leaves near a real Log plane. Every call executes the
actual 729-cell scan and propagation loops, then clears its own flag naturally.
The workload never rearms those cells. One Registry retains the actual Leaves
scratch identity and contents across sample Worlds.

The timer includes the native World call wrapper, mutation-error initialization
and selected leaf body. Construction, loading, input authoring, scalar replay,
inspection, assertions, serialization and retirement are outside it. Callbacks
append ordered records to a prefaulted 4,096-entry memory buffer during the call.
Serialization occurs after the timer stops; overflow rejects the whole attempt. No stdout or
file operation occurs in the timed wrapper.

## State and build evidence

The comparison checks full Block, metadata, SkyLight, BlockLight and height
arrays, every Leaves scratch cell, ordered light regions, scheduled collections
and their identity joins, and ordered callback events. Native guards retain
actual owner identities, dirty flags, holds, leases and provider state. The real
positive-SkyLight class flag is observed before and after each sample and is
never restored between source calls. Supplied and World Random trajectories
remain exact.

The first control attempt fails its artifact comparison because independently
constructed cave Random state words differ across processes. That failed
attempt remains retained. Each cave stream comes from the real default
constructor and stays unchanged during these cached queries. The comparison
normalizes only that unreached 48-bit word after checking the complete generator
and cave triples before and after the call. The cave Gaussian flag must remain
false and its cached value must remain positive zero. Raw state JSON and hashes
retain the actual values. Every other scalar compares exactly through canonical
JSON, and all plane, scratch, queue and event bytes compare exactly. No source
entropy, clock or World state is repaired.

The build record binds 172 selected product and dependency hashes, actual
dependency records and one shared workload object. The detailed adapter selects
the same production bodies compiled with four private leaf symbol aliases and
two getter aliases; the failure adapter selects the ordinary production leaves.
The six aliases, source projection, compiler arguments, selected symbols,
objects, libraries and executables are checked explicitly. The detailed body
uses the same production translation unit; its getter aliases restore the
preceding call sites. Existing original-reference Block profiles
retain their separate correctness scope.

These measurements test cached Grass and Leaves bodies on the declared Windows
inputs. Natural acquisition and loading can still contribute to ordinary frame
cost. A new six-process frame cohort is unqualified: two baseline processes have
208 and 81 foreground-invalid frames. All six attempts and unchanged input,
asset and product guards remain retained. No passing subset or aggregate FPS
gain is claimed.

Full Windows and Linux builds pass. Windows passes 16 focused tests in 23.40
CTest seconds. Linux Clang Debug passes the same 16 tests in 74.70 seconds with
ASan/UBSan, leak checks and Xvfb/software GL. Both runs retain 3,215 unchanged
source, helper and selected product guards. These checks include three native
Leaves failure prefixes, complete nested diagnostics and exact provider GET
counts. The existing eighteen leaf profiles remain unchanged; these edits add
no new original-game coverage. Hosted checks retain their own revision scope.
Reproduction details are in the [benchmark guide](../../tests/benchmarks/random_blocks.md).
