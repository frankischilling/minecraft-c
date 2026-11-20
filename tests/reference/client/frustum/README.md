# Source frustum singleton

The original observer executes unchanged `w.<clinit>`, `w.<init>`, `dr.<init>`,
`w.a()/b()/a(float[][],int)` and `dr.a(DDDDDD)` against genuine LWJGL2.9.0 GL
state. It checks distinct zero array rows, real native-order direct buffers,
one shared singleton and actual source matrix/plane/corner operations. No
prepared source receiver or replacement Java class is used.

Four controlled GL profiles cover identity, translation, power-of-two scale
and 90-degree GLU perspective. Captured projection/modelview input bits,
combined matrix bits, six normalized planes, direct-buffer cursors and seven
box results are compared with the native canonical namespace. The original
also rejects an all-NaN-X box. This finite profile does not establish general
cross-driver matrix generation, arbitrary NaN payloads or whole-frame parity.

The GL3.3 native path reads its actual Display camera ledger at the two reached
source matrix queries. It owns one canonical RenderModels `w.e` and real ge
buffers, retaining the same Graphics namespace. GL writes preserve buffer
position; clear/flip/limit/bulk-get follow the original cursor sequence. Math
active use occurs only after the first raw plane stores, at normalization35.
Native controls check earlier Display failure, missing Math after those stores,
FP rejection after both bulk reads, static identity and terminal cleanup.
All28 constructor allocation boundaries check retained partial arrays/buffers,
sticky initialization failure and explicit cleanup. Separate native buffer
controls check raw float payload copies, underflow and invalid limit retention.

Recapture with pinned JDK17 and SDK inputs on Windows:

```text
python -B tests/reference/client/frustum/capture_reference.py \
  --client-jar /private/b1.7.3.jar --library-root /private/launcher/libraries \
  --output /private/new-frustum-capture
```

Capture receipts hash the original source/SDK JARs, JDK tools, authenticated
native archive/extracted DLL, helpers and method pins before and after work.
The observer permits only the exact SDK native link and denies network and
file writes. No swap, frame, World or asset import is part of this test.
