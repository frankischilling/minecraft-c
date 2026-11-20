# SoundManager constructor ownership

`mc_sound_manager_allocate` reserves the unique native manager and a hold on
its actual shared SoundManager namespace. It performs no source child
construction, Random construction, settings operation or backend activation.
`mc_sound_manager_construct` then runs the single canonical `yo.<init>()V`
body. The existing `mc_sound_manager_create` is the eager wrapper over these
operations and retires an unpublished failure before returning its original
`mc_sound_error` cause.

The split allows the actual Minecraft constructor to retain the receiver
allocated at `new yo` PC104 if `yo.<init>` PC108 fails. Minecraft publishes its
`B` field at PC111 only after that constructor returns. The later settings bind
is a separate operation at startup PC488. This module does not compose those
Minecraft bodies or start a device, audio thread, save or settings owner.

The canonical constructor builds the three actual pools in order. Each pool
constructs its ordinary default Random at `hr` PC9, publishes it at12, makes
the existing embedded map/list representation ready at23/34, stores count0
at39 and `strip_digits=true` at44. The manager publishes only a completed pool
at `yo`12/23/34. A failed pool candidate remains a separate native owner; its
corresponding source field remains null. Embedded zero storage does not stand
for a published Java Map, List or Random object.

After all three pools, `yo` stores counter0 at39, constructs its fourth ordinary
Random at47, publishes it at50 and calls the unchanged `nextInt(12000)` at61
before publishing the initial music delay at64. No seed or clock is supplied
through this API. The existing shared initialized/backend fields remain the
same namespace when a replacement manager is constructed or retired.

The new constructor error contains exact `yo` and reached `hr` frames plus the
complete existing cause. Allocation limits and Random-profile failures are
native policy outcomes; these frames do not fabricate a Java Throwable or
claim equivalence between native allocation failure and Java OOME. The old
factories keep their existing status, operation, source PC, out preservation
and argument/allocation-budget priority.

Operational manager methods admit READY owners. The construction view also
exposes RAW and FAILED prefixes as borrowed observations. A pending failed
pool is never a mutable source pool. Publication flags describe completed
source stores; during partial native retirement a retired child pointer is
null while its historical flag stays set.

`mc_sound_manager_retire(&slot,error)` consumes and nulls the occurrence only
on success. An active pool registration makes retirement BUSY before any child
is freed. Failure preserves the owner and its shared hold for retry; completed
native child retirements are not repeated. Retirement performs no source
cleanup, save or static-namespace reset. Exclusive owner access and the
existing borrowed settings/backend-context lifetimes still apply.

The constructor-split controls pass focused native qualification on MSVC,
Clang and GCC; see [development status](../status.md). They cover all four default-Random failure positions, real
profile continuation, publication/error prefixes, shared lifetime, the eager
wrapper endpoint and error priority, active-pool retirement and the actual
production backend's cold state. Declared private constructor-input fixtures
are distinct from unknown original JVM entropy. Original observations retain
their separate declared scope; these native results establish no device equivalence.
