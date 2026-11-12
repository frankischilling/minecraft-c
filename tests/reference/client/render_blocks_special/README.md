# Crossed-plant CPU observations

This packet captures unchanged Beta 1.7.3 `cv.a(Block,int,double,double,double)`
and its actual `nw` calls on registered Block, constructed CV and static NW
owners. The supplied client archive has SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The original capture completed on Windows with Temurin JDK 17.0.17+10:
125 rows, exit 0, complete bounded pipe capture, and zero network, native-library,
filesystem-write and draw calls. `provenance.json` records the capture hashes.
Native replay remains a separate check through `run_native.py`.

The Java observer invokes the original geometry method through reflection.
It does not copy its algorithm, replace game classes or methods, instrument
bytecode, or compute expected geometry. The compile output inventory contains
only the authored observer and its security guard. Every CV, NW, base Block
and selected concrete Block class must come from the pinned original archive.
The original JAR, dependency JARs, original class files and disassembly remain
local inputs. Published files contain authored preparation/observation code,
input profiles, hashes and numerical observations.

## Finite inputs and observed state

The first 117 rows are the product of 13 registered Block/metadata profiles,
three override values `-1`, `0`, `302`, and three double-coordinate tuples:
`(0,0,0)`, `(-3,50,39)`, `(-16777215.75,65,256.75)`. The last tuple distinguishes
double addition followed by NW float narrowing. Block/metadata profiles are
Sapling ID 6 with metadata 0/1/2/3, TallGrass ID 31 with metadata 0/1/2,
DeadBush 32, flowers 37/38, mushrooms 39/40 and Reed 83. Metadata texture dispatch runs on those actual
concrete objects before the renderer applies its override.

Eight further rows enter the same original body with a NULL Block, NULL NW
backing, or an actual backing array of length 0/4/8/16/32/47. They capture NPE
or AIOOBE and the completed NW prefix. Both implementations begin a real
unfinished quad batch, set translation `(16,-48,-32)` and color
RGBA `(31,127,233,255)`, and initialize the observed backing extent with
`0x5a6b7c8d` before each call. A successful call emits 16 source vertices as
24 triangulated vertices in 192 backing words. The observer never ends or
draws that batch.

Each row records the declared profile and index, CV AO flag, all 27 light
scratch floats, the 12 boolean scratch flags, all 12 color scratch floats, the
six live Block bounds, NW vertex/source counts and cursor, packed color,
current UV, translation, texture/color/normal flags, color-disable flag,
drawing flag, primitive mode, backing capacity, exception category and every
word in the observed extent. Floating values use raw 8-digit or 16-digit
hexadecimal bits; normal/padding slots remain observable through their
sentinels. Success observes all 192 words. Short arrays observe their complete
extent, and NULL backing uses extent `-1` with no words.

`profiles.txt` has five decimal integers per line: Block ID, metadata,
override, position index and failure kind. Success uses kind `-1`; kinds 0/1
select the NULL inputs, and kinds 2 through 7 select the six short lengths.
The replay requires the exact profile inventory, row shape and raw values.
On mismatch it records both rows and reports the first differing cell.

## Scope

The captured method is the direct geometry helper. Whole type1 dispatch
`cv.i(Block,III)`, World/EW brightness and color queries, TallGrass coordinate
jitter, anaglyph conversion, DK compilation, source scheduling, GL and pixels
are outside this comparison. `cv.i` has a static Code identity pin for its
caller relationship; that pin is not an executed-original observation.
The helper itself does not read World/EW, so this packet constructs no World.

The native executable also checks a nondefault floating-environment refusal
and a NULL Block prefix in whole `cv.i`. Those two controls have no original
row here. Native runtime completion or graphical scenarios retain their own
scope regardless of a passing helper replay.

The static pin file binds complete CV/NW/UU class hashes and the selected
constructor, geometry, metadata-texture and NW Code hashes. Capture verifies
the whole archive and the four existing SDK metadata JAR pins. It requires
matching JDK17 tools, clears Java tool-option injection variables, verifies
helper class inventory and origin, and rejects network, native links and
filesystem writes. The shared controller capture helper bounds output, times
the child, drains both pipes and retains exit, timeout, reader-error and hash
receipts. All source, tool, archive and helper inputs must match after capture.
`finite.json` binds its observation-source hashes; changing those inputs
requires a new original capture.

Seven control tests exercise independent row identity/order/extent changes,
failure-category changes, one-bit changes in scratch/bounds/UV/vertex/padding
values, source-pin changes, duplicate metadata keys, numeric/boolean coercion,
and original class/method pin corruption. The local run with the supplied JAR
passes all seven without skips and rechecks the unchanged baseline after
mutations. Without a JAR, the executed static-identity test reports a skip.

## Run

The original capture needs the locally supplied client JAR and the existing
LWJGL2.9.0/JInput/JUtils dependency layout. It does not load their native
libraries. Use a new private output directory:

```powershell
$jdk = 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin'
$libraries = "$env:APPDATA/.minecraft/libraries"
python -B tests/reference/client/render_blocks_special/capture_reference.py `
  --client-jar b1.7.3.jar --library-root $libraries `
  --java "$jdk/java.exe" --javac "$jdk/javac.exe" `
  --output .local/crossed-plant-original-repeat
python -B tests/reference/client/render_blocks_special/test_packet.py --client-jar b1.7.3.jar
```

Copy a newly accepted capture's `finite.json` into this packet only after
reviewing its original completion and immutable-input receipts. Native replay
needs the focused client test target:

```powershell
cmake --build build-windows --target mc_test_crossed_plant
python -B tests/reference/client/render_blocks_special/run_native.py `
  --executable build-windows/mc_test_crossed_plant.exe `
  --work-dir build-windows/crossed-plant-reference
```

The path above uses the repository's single-configuration Windows build.
For Visual Studio, add `--config Debug` and use the executable under `Debug/`.
The replay itself needs no original archive or Java installation. It retains
the native stdout/stderr, immutable input hashes and comparison result in a
new artifact directory on every invocation.
