# Owned World material lookup

`mc_world_runtime_material_get_in_execution` shares the existing World material
lookup body. The ordinary and detailed wrappers keep their existing behavior.
The owned entry calls the actual Block-ID query once at material PC4, then follows
the same air return at14 or Block array/index/material reads at18/23/24.

The caller keeps the already evaluated World scope and its real provider, Store,
import and resource graph alive. It explicitly borrows the defining World code
namespace and passes caller-stable storage for the material error, lower collision
error, exact returned query status and runtime execution result. This entry
neither creates a scope nor infers code ownership from a mutable World field.
Current support requires the evaluated World's same defining namespace.

The two status channels remain separate. A native ownership refusal stops the
material body before Registry or output work, even if the lower query returned
OK. A non-OK query result also stops the body. The caller retains the exact lower
diagnostic and result through pending-attempt and child-close unwind. Ownership
OK alone does not classify a lower failure as a source exception; protected typed
evidence is still required. Native admission preserves material/output/lower
scratch and records its refusal in the required runtime result. Source NULL
receiver classification belongs to the actual caller invocation.

The implementation and lifetime contract are in the
[shared body](../../src/world/runtime/registry.c) and
[private interface](../../src/world/runtime/registry_execution_internal.h).

The [native controls](../../tests/world/runtime/world_access/test_collision_query_execution.c)
reuse the real Registry, World, stored Chunk, loader and checked teardown. Four
success calls select out-of-domain air, stored air, stone and dirt. Ten admission
calls cover missing required arguments, a genuine nested NULL scope and a cold
other code namespace without Source work. One malformed-root call retains the
actual loader status and protected result through scope close and execution
retirement. The [controller](../../tests/tools/world/test_world_query_controls.py)
selects these three material modes and the nullable mode below, verifies the real setup banners and
call counts, retains bounded complete process streams and checks all authored
files through teardown. Only the real eight-byte session token may be added.

The new `material-nullable` control uses the genuine already-READY GLOWSTONE
class. It allocates unused ID200 and passes an actual NULL Material to the existing
constructor. The constructor writes and publishes its real prefix before failing
at PC148; the class stays READY and the escaped object remains Registry-owned.
The pre-bootstrap stored ID200 cell is read once cold and once through the cache;
both material calls must succeed and publish NULL without field or owner repair.
The earlier BASE fixture did not pass initialization admission and remains invalid.

Private Windows and Linux Clang ASan/UBSan opportunities each pass 52 checks
and two nullable calls. The public four-mode material controls subsequently
passed on both hosts together with the eight scanner, collision, Base and nearby
runtime changes. Later Ray, alert and callback changes need their own checks.
GET-OK/native completion refusal,
Blank/foreign Store graphs, parent frame classification and full Arrow tick
remain separate work. The complete private scanner bodies pass on both hosts;
their prospective public controls and remaining limits are described below.

The controls use the configured private profile type through `sizeof`. A coherent
prior three-mode build with the current recorder Core/private header passes on
Windows and Linux Clang ASan/UBSan: 166 new material checks per host, within
1,079 total native query checks. The [combined validation](../testing.md#constructor-recording-and-owned-material-lookup)
records the full Windows and targeted Linux scopes. These controls establish no
frame-time, FPS or complete client/server result.

## Owned material scanner controls

The private entered scanner shares the complete MATCH, LIQUID and ACCELERATE
source bodies, including recursive fluid queries. It requires caller-stable full
error/lower-query/result storage and an already-open explicit World execution
scope. Native-only ownership refusal stops the body without publishing a false
Source return. The last query receipt is not an active failure tag. No owner or
Source exception is inferred from an unrelated diagnostic.

The compact scanner fixture includes the genuine collision/SourceSaving fixture
through a configurable main-symbol hook. Existing 20 query modes, case bodies and
assertions remain intact. New scanner modes are match, liquid, accelerate,
admission, scratch and loader-error. Their 3/3/4/6/1/1 calls are scanner C entry
attempts, including admission refusals, rather than Source method call counts.
Authentic inputs are authored before bootstrap through the existing producers;
only the actual eight-byte session token may be added during execution.

Private Windows and Linux Clang ASan/UBSan opportunities each pass 398 checks
(66/67/71/79/60/55) and 18 entry attempts with complete captures, checked cleanup
and unchanged literal files. The compact public projection then passed on both
hosts: all 26 query controls, ten related legacy regressions and two controller
checks passed. The query controls contain 1,529 native checks, including the
398 scanner checks. Windows took 256.18 seconds; Linux Clang ASan/UBSan took
123.73 seconds. These results describe that composition, before the later Ray,
alert and callback changes.

A later Windows selection passes all 56 tests in 222.28 seconds after those
runtime changes and single-Entity insertion. Its 39 query controls pass 2,282
native checks, including the thirteen Base/collision/nearby modes below. All
captures, literal inputs and source/product guards remain intact; no selected
test is skipped. This also checks the affected legacy damage, alert, publication
and raytrace paths. A subsequent Windows selection passes the seventeen direct
Ray, insertion and callback-counter-failure controls plus both controller checks
in 93.86 seconds, adding 891 native checks. Linux Clang Debug with ASan/UBSan
passes the combined 73-test selection in 142.60 seconds: all 56 query modes,
3,173 native query checks, fifteen legacy regressions and two controller checks.
No selected test fails or skips. These are targeted selections, not full-suite,
original-VM or gameplay performance results.

Separate private Windows controls exercise the owned Base/collision and nearby
entries directly. Eight Base/collision modes pass 488 checks, including the full
23-callback splash sequence and 57 Random draws. Five nearby modes pass 265
checks, including duplicate list occurrences, live result leases, independent
Math refusal and registered callback failures. These thirteen controls also pass
in the public selections on both hosts above. These component checks establish
neither a complete Arrow tick nor original full-method parity.

GET-OK/native completion refusal, foreign graphs, parent classification and full
Arrow remain separate work. There is no frame-time or FPS result from these checks.
