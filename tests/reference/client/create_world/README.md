# Create World static original identities

This packet pins identities from the locally supplied Beta 1.7.3 client archive,
SHA256 `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The archive is local input and is not distributed here. Published metadata contains
class and Code hashes, complete declared field/method inventories, hierarchy,
exception tables and selected resolved constant-pool references. It contains no
original Code bytes, classes, assets or disassembly.

The 14 selected classes cover Create World `FJ`, text field `RO`, shared character
mapping `FP`, screen `DA`, drawing base `UB`, translation registry `NH`, SaveFormat
interface `NL`, concrete `HI` and its superclass `TQ`, button `KE`, helper `IN`,
font `SJ`, World Selection `RQ`, and Minecraft. Original archive names are lowercase
except Minecraft. Every selected class has its complete inventory pinned; every
FJ/RO/FP/DA/UB/NH/NL declaration has its Code identity pinned when Code exists.

Selected references cover FJ construction and initialization, translation and
button/text-field owners, focus and length stores, filename trimming/replacement
and metadata collision queries, tick/close/tab, seed Random/parse/hash operations,
screen/controller/load actions, key/mouse dispatch and drawing. RO references
include parent Tab, the actual `DA.d()` clipboard leaf, UTF16 String substring and
append calls, character mapping, focus, rectangles and inherited font drawing.
The packet resolves inherited declarations independently from full inventories:
FJ pause/background come from DA, FJ/RO text and rectangles come from UB, and
`HI.b(String)EI` metadata lookup comes from TQ. Minecraft startup allocates HI and
assigns the actual SaveFormat field; RQ allocates FJ with the actual parent.

```text
python tests/reference/client/create_world/verify_source.py --jar b1.7.3.jar
python tests/reference/client/create_world/test_verify.py --jar b1.7.3.jar
```

The verifier uses the shared offline classfile reader and reads the same immutable
archive bytes it hashes. It executes no game classes and needs no Java runtime.
Missing original input returns `UNAVAILABLE` with exit 77. Invalid archives,
changed identities, omitted declarations, false inheritance/reference owners,
numeric/boolean coercion and duplicate metadata keys are rejected with exit 1.
The control suite includes independent repeated mutations followed by a passing
unchanged baseline; its successful process exit is 0.

Static matching establishes which original methods, fields and references the
authored model describes. It does not establish executed-original GUI, clipboard,
filesystem, Random, drawing or world-load parity. Native model controls and SDK
screen scenarios are separate evidence; this packet neither runs nor substitutes
for an executed-original comparison.
