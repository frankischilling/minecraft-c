# Whole N terrain submission on a device

`finite.json` records seven calls to unchanged `n.a(Lls;ID)I` on genuine GL:
six ordinary profiles and one complete 16-entry occlusion branch. On a supported
native context replay compares all 35 rows, including exact center-pixel RGBA, modelview bits,
source counters, dirty/selected DK identities and all four TG states. Each
TG row includes a defined uint64 fold over every word of its 65536-int capacity,
so preserved suffixes and uncleared buffers remain observable.

Each profile starts a fresh process/context. Both sides use real TG and DK
constructors, a null World, explicit canonical Math initialization before
those constructors, and authored nonempty pass lists emitted by the actual
NW methods. N, App and both cameras have declared prepared fields. This
qualifies the whole N method and its reached children on those inputs; it
does not qualify N/App construction, World geometry compilation, recenter,
rebuild, an issued query/result, or a complete PX frame.

The cases cover empty selection, both passes, two coarse groups, counter
priority, visibility filtering on the ordinary pass 1 path, and null current
App.i after S/counter mutations but before T resets. The outer camera differs
from App.i. Pixel results discriminate the latter camera's actual use. Every
case still executes the ten spatial dirty polls and checks the real queue.

The final profile obtains the same real CX/current-context capability used by
N construction, enables advanced OpenGL, and passes that actual result as w.
It executes the initial 16 entries and the subsequent empty range, including
GL masks, Push/Pop, restoration and another whole grouping call. The final
S is empty and TG h/i reset, while its earlier buffer contents/limit survive.
The occlusion profile requires actual capability support. Native CX uses the
literal `GL_ARB_occlusion_query` token and all eight ARB entry points on its
current Display; core-version promotion does not replace that source policy.
After a successful actual CX query returning false, the native probe records
the Display's actual cached vendor/renderer/version and major/minor/profile,
CX static a=true, status OK, source-returned/capability-read flags and return
PC 20. It then performs checked teardown of CX, its namespace, Display, graphics,
Math, Entity classes and the quiescent Random profile, prints an explicit
`N-not-entered; cleanup-complete` marker and exits 77. This happens before
Settings, cameras, App, N, Ge, TG or DK setup. Introspection or cleanup failure
exits 1 and cannot supply the marker. No N ordinary-path substitute is executed.

The runner accepts exit 77 only for the declared actual-capability `occlusion16`
case, with that complete receipt, empty stderr and no SUBMIT/TG rows or normal
N return. It still requires all 30 ordinary rows exactly and reports
`PASS_WITH_UNSUPPORTED`, `compared_rows=30`, `original_rows=35`, `completed=6`,
`unsupported=occlusion16` and `uncredited_rows=5`. The unchanged five original
occlusion rows remain in `finite.json` and receive no native credit on that
context. This is a successful bounded replay with explicit unsupported scope,
not a CTest skip of the seven-profile test. On a supported context all 35 exact
rows and all seven completed profiles remain mandatory. A concise scope footer
and `run.json` report these counts in both cases.

Configure `-DMC_REQUIRE_TERRAIN_OCCLUSION_TEST=ON` with display tests enabled,
or use `run_native.py --require-occlusion --executable ... --work-dir ...`, for a
required supported-device lane, including the Windows NVIDIA lane. That option
makes the otherwise validated exit 77 path a failure; it cannot pass with 30 rows.
The `tools.submit_capability` controls reject incomplete captures, malformed
capability evidence, missing cleanup and any execution rows on the unsupported path.

Original capture uses a declared RGBA8/depth16 PixelFormat matching the native
Display's requested color/depth profile. Actual driver/format receipts are
retained. Pixels are compared exactly; no tolerance or omitted channel is
used. The measured original format is RGBA8/depth24/stencil0: depth16 is the
request, not an assertion about the chosen format. Native fixture setup first
uses the canonical startup projection to admit a readback frame, then overwrites
its projection/modelview, viewport and explicit GL state before N execution.
This preparation neither supplies an N draw nor changes the capture API.
An additional supported-device native control uses a real zero-capacity Ge
buffer and generated query Names. It checks the source minimum 1 exception and
that missing native Names still fail earlier; it supplies no query result.
These controls are separate from the 35 original rows.
The guard permits only the pinned LWJGL64 native link and reports zero
network/listener attempts. Original game bodies remain unchanged, with no
replacement class, override, transformation or callback supplying results.

The capture driver freshly verifies the archive, selected complete classes
and method Code, JDK17, SDK/native archives, helper and input bytes. Before/
after hashes and bounded per-process receipts are retained. Public replay
needs the executable and an actual GL context, but no JAR/JVM. Full 35-row
qualification additionally needs the source-policy occlusion capability above.
