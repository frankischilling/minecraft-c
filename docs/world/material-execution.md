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
admits exactly these three material modes, verifies the real setup banners and
call counts, retains bounded complete process streams and checks all authored
files through teardown. Only the real eight-byte session token may be added.

Nullable material remains open. The unshipped fixture attempted a private Block
allocation outside its admitted initialization flow, so it did not reach material
lookup and supplies no nullable coverage. GET-OK with native completion refusal,
Blank and arbitrary foreign Store/Chunk graphs, the fluid scanner, parent frame
classification and full Arrow tick remain separate work.

The controls use the configured private profile type through `sizeof`. A coherent
build with the current recorder Core/private header passes all three modes on
Windows and Linux Clang ASan/UBSan: 166 new material checks per host, within
1,079 total native query checks. The [combined validation](../testing.md#constructor-recording-and-owned-material-lookup)
records the full Windows and targeted Linux scopes. These controls establish no
frame-time, FPS or complete client/server result.
