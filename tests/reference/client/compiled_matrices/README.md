# NW translation and compiled matrix controls

`CompiledMatrixObservation.java` invokes unchanged `nw.b(DDD)V` on the genuine
original singleton. It observes five raw offset triples, untouched flags,
counters and buffer cursors, offset preservation through the real `nw.b()`,
and the real vertex method's double addition before float conversion. The
constructor is genuine; no source fields, methods, classes or GL calls are
replaced. The NW observations need no native GL operation. The final nonempty
vertex buffer is retired with the bounded observer process; it is not drawn.

The two `DK_SEQUENCE` rows are **authored matrix-call controls**, extracted
from DK.a264..332 with local origin zero and dimensions 16. They run on a real
compatibility GL context, compare immediate execution to actual GL_COMPILE
and replay, and verify deferred Pop restoration. They do not invoke DK.a,
render blocks, or establish full original chunk compiler parity. The finite
matrix values qualify the recorded NVIDIA driver and selected operands;
other driver arithmetic is not silently substituted.

`source-pins.json` pins the unchanged class bytes and selected method Code
identities. `capture_reference.py` requires the original JAR, pinned existing
LWJGL 2.9.0 SDK metadata and native archive, and matching JDK17 tools. It records
input/helper hashes before and after, the real driver identity, native link
receipt, bounded output and network-denial guard. Run from the repository:

```powershell
python -B tests/reference/client/compiled_matrices/capture_reference.py --client-jar "$env:USERPROFILE/b1.7.3.jar" --library-root "$env:APPDATA/.minecraft/libraries" --output .local/compiled-matrices-original-new
```

The checked-in `finite.json` contains all nine rows from a successful capture.
The CPU source-resource test checks the five raw triples, source preservation
and vertex result. The source-list device test checks the two matrix vectors,
immediate/compiled pixels, selected-matrix replay, deferred stack/FP failures,
and committed/pending list ownership. The latter failure controls describe
native bridge admission; they do not synthesize source Java or GL errors.
