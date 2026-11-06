# Plant placement

Ordinary client placement now executes the original flower-family predicate
for Saplings, Tall Grass, Dead Bush, both flowers, both mushrooms and Crops.
These eight registered Blocks have six runtime classes. Direct Block calls,
the inherited face wrapper and the World placement wrapper retain their
separate source paths.

The predicate first invokes the base Block target-replaceability method. A
false result returns immediately. Otherwise it reads the Block below using
Java wrapping for `y - 1`, then invokes the same receiver's substrate method.
There is no added light, sky, survival or height check.

Sapling, Tall Grass and flowers compare the live Grass, Dirt and Farmland
singleton IDs in that order. Dead Bush reads live Sand; Crops reads live
Farmland. Mushrooms capture the actual opaque boolean array, then perform its
signed element read. The existing plant runtime and retained placement path
share this substrate implementation.

The retained API keeps the invoking Registry independent of the World's
Registry. Each reached World ID query has its own diagnostic: the outer World
target, the base predicate's fresh target and the below-Block read. All three
retain their source and callback causes until checked retirement. Separate
base, substrate, face-wrapper and caller positions preserve the failure path.

## Original comparison and native controls

Two fresh ordinary VMs execute unchanged original methods for 288 profiles
and 864 calls. The [reference packet](../../tests/reference/world/plant_placement/README.md)
contains all eight class rows, all 864 outcome rows and completion. It covers
substrates, target replaceability, signed Y, ignored faces and 24 reached
NULL-World exceptions. Each VM shares one genuine World and four canonical
Chunks across the ordered cohort, with explicit NULL-World operands in those
profiles. Declared arrays reset before each channel; the methods and World
services remain unchanged, and other residual World state stays shared.

Windows MSVC Release and Linux Clang Debug ASan/UBSan match all 873 selected
lines literally. All compared calls preserve the five observed arrays of all
four Chunks. The original packet has 61 corruption controls; the native
comparator has 34. Native controls separately check query ownership, pending
call refusal, live singleton aliases and NULLs, signed mushroom-array indices,
foreign invoking Registry identity, early return and target/below failures.

The original Sapling predicate returns true at Y=128 when Y=127 contains a
valid substrate. The later World setter rejects that height. Native Item-use
controls preserve this distinction and pass 873 checks per host. Seven real
placement prefixes include Sapling metadata 2 and Flower's inherited metadata
0, followed by hooks, sound and count changes before the declared NULL-Player
statistic failure.

A retained pre-change Linux executable reproduces the former native refusal
at `wb.a(World,III)` with 1,551 selected inputs unchanged. The new implementation
passes ten focused tests on both hosts, including placement callbacks,
controller use, plant lifecycle/replay, farming, Dye and the new comparison.
The cohorts take 16.50 Windows and 28.03 Linux CTest seconds. Linux keeps leak
detection, halt-on-error and UBSan stack traces enabled.

## Windows placement and save cycles

Two separate fresh client processes use authored saved Player and terrain
inputs. Each starts with health 5, one selected Stack of count 2 and damage 0,
Dirt at `(3,64,3)` and Air above it. The actual source ray must hit the Dirt's
top face before one ordinary SDK right-click places a Sapling or yellow Flower.
The remaining Stack has count 1, with one used statistic, one sound completion
and one equipment reset. Sound/count/statistic completion proves the earlier
placed hooks through source control flow; no direct hook observer is claimed.

The Sapling profile passes 1,617 checks and the Flower profile 1,627, each in
8.00 CTest seconds. Both return terrain, HUD and held-Item drawing and three
continuing frames. Native compiled geometry contains the exact 24 appended
plant position/UV vertices, alongside the retained 60-vertex terrain prefix.
The screenshots show the planted Block and held Item. This checks native
geometry and presentation without an original-scene pixel comparison.

Ordinary Save and Quit and reselection create distinct World and Player owners.
Independent Region/Player NBT decoding checks the placed Block, metadata,
remaining Stack, health and selected passive save-entry state. Each profile
preserves 1,992 selected inputs and 413 assets. All thirteen saved files retain
their bytes through reselection except the actual eight-byte session token;
unrelated Worlds and preserved bytes remain unchanged. Native cleanup checks
that both windows disappear.

The first wrapper preparation used the reused producer's health 20 while
expecting the declared health 5, so neither client launched. The input builder
now writes health 5 explicitly. The first actual native pair completed, but
the final wrapper compared a slash path against Windows backslash keys. Saved
evidence proves that only `session.lock` changed. The corrected comparison
uses its exact native path and retains all other byte checks. Every failed
record remains preserved; no runtime behavior changed for these fixture fixes.

These results cover the specified predicates and two Windows placement/save
scenes. Other plant Items, growth and survival combinations, original pixels
and audible output, Linux loaded play, unrestricted play and FPS remain separate.
