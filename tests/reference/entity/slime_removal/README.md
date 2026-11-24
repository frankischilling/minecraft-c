# Direct Slime removal reference

This packet executes unchanged client `uw.K()V` from the verified Beta1.7.3
archive (`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`).
Its144 Code bytes have SHA256
`d3fa87376c230baf75e13c90f711bed5b071d09ca2890e8576dc66a3dd060cd5`.
No original game class, bytecode or assets are included here. The Java helper
authors operands and observes actual original methods; it never replaces them.

Each profile constructs an actual `fd(new wy(),name,xa.a(0),23L)` and nine
actual cached `lm` Chunks. A genuine Slime constructor and `e(int)` setter run
before the declared pose `(8.5,64,8.5)`, health, removed flag and private seed38
are supplied. Nonnull-World parents are inserted through actual `fd.b(sn)`.
The `pm` observer records each distinct child only after actual World and Chunk
insertion. Every child callback includes the parent's then-current removed flag
and Random state; it precedes the final inherited `sn.K` store.

`profiles.tsv` columns are name, full size-setter argument, health, World mode
(ordinary0/remote1/actualNULLconstructor2), call count, initial removed flag,
and observer mode. Observer modes0/1 record or throw after the second callback;
2 changes parent pose to `(10.5,65,9.5)` after child1; 3 calls actual `e(4)`;
4 reseeds the actual parent Random to12345; 5 clears the parent World; 6 changes
it to another genuine cached World; 7 supplies Integer watcher16 to prove the
actual Byte cast precedes the null-World read. All changes are declared inputs
inside actual reached callbacks. There is no post-call repair, retry or tick.

The first11 profiles are the native comparison inventory. Additional original
controls cover sizes3/4/5/127, signed Byte wrapping, negative health, repeated
calls on the same removed receiver, an initially removed receiver, null World
with size0, and fresh constructor/publication World capture. Repeated calls
retain their actual stream and published children. Independent constructor
private Random and shared Java Math entropy are retained separately, excluded
from deterministic comparison. Parent yaw draws and callback ordering remain
exact binary words in the compared records.
Private-stream snapshots cover the parent and actually published children;
they do not expose an unpublished child's private constructor stream.

```powershell
python -B tests/reference/entity/slime_removal/capture_reference.py --jar PATH/b1.7.3.jar --work-dir PRIVATE_OUTPUT
python -B tests/reference/entity/slime_removal/test_packet.py
python -B tests/reference/entity/slime_removal/compare.py --original-capture CAPTURE_DIR --native-run NATIVE_RUN_DIR --profiles direct-split --output NEW_COMPARISON.json
```

The collector requires matching JDK17 tools and the existing pinned LWJGL/JInput
metadata archives. It clears JVM option injection, denies network/native loads,
restricts writes to a private output root, verifies class origins and source
Code hashes, retains bounded process receipts and `javap`, and fingerprints all
collector/comparator inputs plus authored helper classes before and after.
The comparator only reads completed native controller artifacts. All11 named
profiles are required by default; any smaller inventory must be explicit.
Native-only damage-then-K controls with independent constructor entropy are
excluded. No native executable is launched by this packet's tools.

This is direct removal-call evidence. It does not qualify delayed Living/Mob
death ticks, the particle tail, recursive execution-owned gameplay parents,
save/reload, rendering, original audiovisual behavior or FPS.
