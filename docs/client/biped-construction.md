# Independent Biped construction

`mc_client_biped_owner` owns the genuine normal `fh(0)` / `ko` model graph. Its
nine parts, direct positions, original/UV-copy vertices, quads, mirrors and UV
arithmetic use the same concrete graph body and allocation ledger as the
ItemRenderer's existing Biped. The shared body is compiled once in the CPU
`mc_client_notice` library. Each owner has its own graph/class holds; Notice's
private model and a standalone Biped remain distinct. Construction does not
reset the Entity Vec3 pool, consume a DefaultRandom or use SDL, GL or resources.

Allocation publishes a RAW native owner. Construction is one-shot and moves
through CONSTRUCTING to READY or FAILED, preserving reached field/child effects
and complete model/Vec3 causes. Failed candidates remain inspectable until
released. A successful release consumes/nulls that occurrence even when another
hold survives. BUSY preserves it. Retirement is allocation-free and retryable;
source fields become unavailable during retirement. The default native quota
and Entity vector ledger are separate policies, not Java allocation sizes.

The public entry admits the normal zero profile. It does not publish App.x,
construct a complete application, accept general float models, animate or draw.
The old delayed FP admission remains: an unsupported MXCSR can be reached only
after real model/part/array effects, at the existing Vec3 child. Native tests
check that source prefix without hardcoding a Windows allocation charge on
Linux. The historical retained-old versus extracted-body proof remains in the
private author qualification rather than becoming a second production body.

Author qualification covered strict MSVC Debug with default 1 MiB stack and
fresh Clang18/GCC13 Debug ASan/UBSan/leaks on the same 1830-file source cohort.
Each selected profile passed 12 cases plus 9 process controls, 4381 native checks,
435 allocation prefixes and 838 fresh original Biped rows. Existing Notice
regressions and 291 original Notice rows also passed in that bounded cohort.
The subsequent combined public graph passes 389 Windows and 381 cases on each
sanitized Linux compiler. All prior 376/348/348 cases remain. The new driver
passes 128 controls normally and with Python optimization; fresh originals
match all 838 records on each host. Source and configured products remain
unchanged, and every Windows executable keeps its default 1 MiB stack.

Default tests include standalone contracts, Item/Biped FP prefixes and bounded
native-record/normal-and-optimized rejection controls. Original Biped CPU
comparison is OFF by default. Enable `MC_ENABLE_BIPED_REFERENCE_TESTS` with
explicit `MC_REFERENCE_CLIENT_JAR`, `MC_REFERENCE_CLIENT_LIBRARY_DIR` and matching
JDK17 `Java_JAVA_EXECUTABLE`/`Java_JAVAC_EXECUTABLE`. The original CPU profile runs
on Windows or Linux without a native graphics library. The existing Notice
original driver's Windows-only option/refusal policy is unchanged.
