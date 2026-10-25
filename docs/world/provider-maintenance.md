# World provider maintenance

`mc_world_runtime_finish_maintenance` ports client `fd.p()V` for the actual
normal GAME Overworld/cache constructor cohort. It keeps one entered World
hold across the source loop and calls the existing concrete maintenance path
until its returned provider boolean is false. Errors retain the complete actual
detachment/Entity/TileEntity/save child cause and every completed field, file and
collection effect. Successful calls clear their per-attempt error.

The source sequence is `fd.p -> ok.a -> ld.a -> yf.a`. Each `ok.a` pass performs
100 iterations. A nonempty actual pending HashSet selects its current first key,
reads that cache entry, invokes the real Chunk unload body, main and extra save,
then removes pending, map and the first same-identity insertion-list occurrence.
The region loader's maintenance operation follows the loop, then the actual
underlying Overworld generator supplies the result. Loader extra-save and
maintenance bodies are literal returns; generator `yf.a()Z` is literal false.
They are reached even with no pending entries. The enclosing World method
therefore executes one pass for this concrete generator and can leave pending
entries after 100 removals. It does not drain pending until empty.

The complete captured `ok` class has no pending enqueue method or Set.add call.
Its private Set starts empty and its ordinary acquisition/maintenance methods
only remove from it. Natural constructor/acquisition/post-grid profiles keep it
empty. The native dedicated-only `mark_drop` route remains rejected for this
normal provider. Conditional unload/save/error branches are preserved and tested
separately with declared actual-collection inputs, not an invented normal enqueue
operation or a changed provider policy.

No lighting drain, UI, Player/controller bootstrap, simulation tick or save-all
is added. Minecraft's local 2000 assignment preceding `fd.p` does not itself run
2000 ticks. Composition, dedicated and unqualified server Worlds fail explicitly
at native admission. The supported World and all borrowed dependencies must
remain live, exclusive and disjoint across synchronous reentry.
The supported GAME binding is constructor-bound and immutable: its provider,
cache, pending, Store and generator binding are not replaced during the call.
Arbitrary replacement/null-provider/private-header mutation states are outside
this qualification; no supported current GAME API performs such replacement.

The implementation reuses genuine existing callbacks and ownership: actual
Chunk active=false/TileEntity invalidation/eight Entity slice deferrals, the
real typed Chunk serializer, checked session/region transport, exact collection
removal and actual loader/generator leaf operations. It supplies no replacement
successful callback. Unsupported live serializer bodies remain explicit child
dependencies. Existing native checked I/O failures are fatal; source Java's
caught IOException/Exception printing and continuation are not generally
reproduced. A session-lock mismatch raises source runtime exception `us`, which
is not caught by the main-save IOException handler; that prefix is compared
directly. A missing selected cache entry reaches source NullPointerException
at `ok.a` PC53; the native bounded state error preserves the same collection
prefix without emulating a Java Throwable.

The verified original client JAR SHA256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Exact class/Code pins are in `tests/reference/world/maintenance/source-pins.json`:

| Method | Code bytes | Code SHA256 |
| --- | ---: | --- |
| `fd.p()V` | 16 | `bcd40ab006f5d36c3258b77d507eb6e89b6995ab1ad3d5f1131795672ac81035` |
| `ok.a()Z` | 131 | `8eba5425c0eb6c6b79064e2f71e6fc299e07809a631099e4d206c8db9a60a2d4` |
| `yf.a()Z` | 2 | `42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e` |
| `ld.a()V`, `ld.b(Lfd;Llm;)V` | 1 | `149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d` |

Finite tests construct actual native Worlds/Registry/profile/Store/dimension
owners and unchanged original `mx/fd/ok/ld/yf/lm` owners. Empty and cached cases
use natural source state. Controlled cases call actual original HashSet.add and
native ordered pending.add on their real owned sets, with actual constructed
Chunks. The session-lock case corrupts only its disposable owned file. The
Item case constructs and publishes a real Item/Stack, then verifies unload,
deferred membership, save and native retirement. No fake World, Player, Chunk,
subclass, Unsafe allocation or replaced original method is used.

All six cases match cache/pending cardinalities, retained Chunk flags/save time,
remaining identities and the corresponding success/source exception boundary.
Native diagnostic builds additionally observe actual callback entry order,
including loader/generator on empty pending and the failed save prefix. The
diagnostic recorder changes no game fields or callback results and is absent
from the public runtime patch. Region wall-clock timestamps, arbitrary caught
I/O outcomes and raw compressed-file equality are outside these comparisons.

`world.provider_maintenance` uses authored resources and unique owned runs;
`tools.provider_maintenance_controller` checks eight subprocess failure controls.
The optional `reference.provider_maintenance` runs both products and compares
all six cases after verifying successful native completion and output hashes.
Enable it with `MC_ENABLE_MAINTENANCE_REFERENCE_TESTS=ON`, the supplied client
JAR, `MC_REFERENCE_CLIENT_LIBRARY_DIR` and explicit matching JDK17 tools.
The combined runner is `tests/reference/world/maintenance/compare_maintenance.py`.
