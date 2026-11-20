# Actual ARB query surface

Production uses the canonical Display's cached eight ARB functions and exact
context generation. Source Begin fixes target 35092, source End closes the same
active occurrence, and unsigned result calls accept 34919 (available) or 34918
(result). The ID must belong to the live generated-name owner. The caller
passes the actual Ge IntBuffer position..limit span; LWJGL2.9's unchanged
`glGetQueryObjectuARB` bytecode checks **one remaining int**, not four.
No result, visibility, core-name alias, GetError drain or context switch is
substituted by a source wrapper.

An allocated Begin handle owns a Names/context right before the void call and
survives entered native failures. It records issuance separately from source
End. Ended handles still block Names deletion until checked retirement. An
issued, unended handle is closed on its original context by a distinct native
abort during retirement; an unissued pending handle is released without End.
Failure preserves the same occurrence for retry. The existing Names context
lease blocks ordinary and staged context destruction throughout.

The device fixture includes the exact Display body and links the canonical
Ge/NW owner. It tests actual generated names and writable Ge spans, empty and
visible GPU queries, neighboring buffer words and unchanged cursors, a sticky
GL error that the query wrappers leave pending, a real failed list prefix,
an actual different current context, and checked native abort. A separately
declared native `context_lost` admission flag proves that a pending, unissued
Begin cleans up without an unmatched GL End; it is not a simulated GPU reset.
No query callback, capability or result is replaced.

Immediate availability is recorded as actually returned (0 or 1). The test
does not force an unavailable result. Query counter width is also read: the
ARB specification permits width 0, for which zero samples is legitimate.
Result retrieval can wait for completion; AVAILABLE reports whether such a
wait would be needed. These rules follow the primary
[ARB_occlusion_query specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_occlusion_query.txt).

The separate Java observer executes unchanged, hash-pinned LWJGL2.9 wrappers
on real native compatibility GL. It requires no game JAR or game assets.
Its authored geometry is 2730 copies of the same NDC triangle in a 64x64
viewport, with depth/cull/scissor/blend/alpha/texture/lighting/fog disabled and
color writes masked. It records real availability, sample counts, counter
width, direct-buffer guards/cursors, driver identity and native link receipts.
Network/listener/write attempts are denied and inputs/helpers are hashed
before and after bounded execution. No SDK classes or native binaries are
distributed; the existing pinned native archive is extracted only privately.

The captured NVIDIA 581.42 profile returned available 0 for both initial polls,
empty result 0 and visible samples 5503680 with 32 counter bits. These timing
observations remain in the receipt. They are not a portable assertion that a
future first poll must be unavailable, nor evidence of whole N/terrain parity.
The native device fixture records its own values and checks the buffer and
ownership invariants on its actual context.

```text
python -B tests/reference/client/occlusion_queries/capture_reference.py --library-root /private/libraries --output /private/new-query-reference
mc_test_occlusion_queries
```
