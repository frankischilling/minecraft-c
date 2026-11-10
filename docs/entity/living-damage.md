# Zombie damage

The client Zombie handles damage through the actual Monster and LivingEntity
methods, including a nonnull attacker. Environment damage, including the
periodic fire call, reaches the same production body as a direct damage call.

The body preserves the remote-World and dead-Entity gates, hurt resistance,
stronger repeated hits, health stores, hurt timers, attacked flag and direction.
A full NULL-attacker hit consumes the shared Math random stream before the
private Random calls used for the hurt sound. A nonnull attacker supplies the
actual direction and knockback; a coincident horizontal position reaches the
original shared-random retry loop. Sound dispatch uses the captured World
receiver, the actual Registry String and ordinary WorldAccess callbacks. A successful
NULL-attacker hit preserves an existing Cow target when the original Monster
branch returns before the target store.

After Living damage returns true, Monster compares the attacker with its
current mounts and itself before storing that same attacker as its target.
The owning store retains the new target before replacing the old occurrence.
False Living returns and earlier failures do not change the target.
The [Monster retaliation contract](monster-retaliation.md) records the four
concrete receivers, owning-store behavior and separate fresh original comparison.

This section records the canonical client Zombie implementation and its
observations. Skeleton, Creeper and Spider enter the same inherited damage
body through their own receiver and sound declarations. The earlier
[Skeleton NULL-attacker observations](skeleton-damage.md) retain their finite
scope. Fatal Spider, Zombie, Skeleton and Creeper damage now completes its
[death/drop continuation](monster-death.md), including Player callbacks and
actual Item publication. The earlier sound/frontier observations below retain
their recorded scope. [Animal damage](animal-damage.md) now
executes the inherited NULL and non-NULL-attacker body for client Sheep, Pig,
Cow and Chicken. Other concrete overrides retain their explicit dependencies.

The [shared damage execution entry](damage-execution.md) now carries an actual
Arrow hit at 741 through these same bodies, with the current nullable Living
shooter and amount 4. Knockback, sound and Mob death use the caller's defining
runtime, Registry and shared Math context. Player armor/statistics, ally alert
and retained fatal drops/death join that same dispatcher. Raw Entity status,
gameplay result and native continuation remain separate.

## Recorded Native close

The owned Living sound leaf records its actual World
token, the initial checked pop's exact failed operand and the point where its
ordinary local calls have drained. Native-only close uses that same token and
operand. An open token must be the checked current token; pending children and
unrelated scopes refuse. Positive same-generation consumption prevents another
pop, including after a non-OK close or later slot reuse. Source completion, raw
status, first Native failure and Math remain separate; cleanup never promotes a
previous false continuation or reruns sound.

Entity sound checks its exact initial World token and the same idle lease before
each next List reload and Entity argument read. A listener that leaves a descendant
scope open stops the call before a later listener; detailed raw status can remain
OK while native continuation is false. A null World listener List records its
typed Source fault at the actual receiver read. These current contracts are
separate from the earlier controls recorded below.

The earlier four direct controls used genuine constructors and one actual setup GET:

- `sound-pop`: real Skeleton sound and an entered callback's Native binding fault
  retained the original token with a recorded failed operand of false.
- `sound-capture`: a real callback IO/Throwable and immediate Native capture
  refusal preserve their separate causes and the actual protected handoff.
- `ally-success`: a real Player/Skeleton and loaded Class/Wolf query return an
  empty List, followed by recorded cleanup and genuine token reuse.
- `ally-unreached`: a real Player target takes the helper's F=false return63;
  World/query/Math work remains unreached and no close is invented.

Windows x64 MSVC Release and Linux Clang 18 Debug with ASan, UBSan and leak
detection each passed all four modes: 175 checks, four leaf entries and four
setup GETs. Their two sound callbacks and one protected capture used the real
WorldAccess and receipt paths. Both hosts also passed the affected eighteen-test
selection, with no failures or skips.

The current per-reload guard stops `sound-pop` after its callback invalidates the
native binding count, before another Source List read. The detailed World return
remains raw OK at callback PC53, but sound continuation is false and the saved
close operand is now `failed=true`. The first native cause is the guard that checks
the current scope; restoring the projected native count permits only recorded cleanup.
The historical 175-check record above keeps its earlier operand and capture scope.
Closing with that saved failure retains the exact World slot and marks execution
failed. New scopes refuse until checked retirement; repeated recorded close is
idempotent. The current same-slot generation reuse control belongs to the successful
ally mode, whose returning scope is freed normally.

That record does not retroactively qualify the later damage join. The current
22 damage-execution modes pass, including the typed null-List and both Entity-sound
scope controls. Canonical ally initial-pop/capture callback faults and consumed-nonOK
retain their separate limits. Complete GS35 wake, delayed Slime splitting,
arbitrary cold/foreign graphs, original full-method comparison, audiovisual equality
and FPS remain OPEN.
The [Player ally ownership contract](player-ally-alert.md#recorded-native-close)
keeps retained-call retirement separate from its World-token close.

## Verification

Twelve authored CPU profiles use genuine Worlds, 25 stored Chunks, Registry
classes and ordinary Zombie/Cow construction. They cover fresh and repeated
hits, early returns, NULL World and Random failures, a fatal prefix and one
complete nonnull Cow-attacker call, a nested periodic-fire Mob tick, callback
failure and an existing target alias. Constructor entropy stays independent; observed original seeds
and field values are never inputs to the native execution.

Before the nonnull-attacker extension, Windows MSVC Debug and Linux Clang18
ASan/UBSan each passed 2,179 native checks. Comparison with the unchanged
original observations passed 3,544 typed checks, 3,861 identity relations and
84 separate-process stream checks. Six direct calls returned; two profiles
matched NULL failures. Fatal, nonnull-attacker and callback outcomes
retained their narrower declared prefixes. The updated case 8 requires a
successful full return, unchanged shared Random, two private sound draws,
actual direction and knockback, and the new Cow target's owning occurrence.
Its optional original comparison selects the unchanged original's full return
and compares all available deterministic fields and reference relations.
The nested fire comparison covers the sound-entry prefix and named health/timer
fields, rather than full tick equality.

Earlier controls rejected eight comparator corruptions, three original-manifest
corruptions and four build-record corruptions. Five additional corruptions
exercise the new target, direction, knockback and returned Boolean checks.
An ordinary native sound callback
returns a storage error with no Source invocation receipt. Its IO failure is
kept distinct from the original fixture's Java exception.

Build `mc_test_zombie_null_damage`, then run:

```sh
ctest --test-dir build -R '^entity.zombie_null_damage$' --output-on-failure
```

Add `-C Debug` for a Windows multi-configuration build. Public tests need no
original assets, JAR or JVM. The [reference packet](../../tests/reference/entity/zombie_null_damage/README.md)
describes the optional original comparison and its build/output receipt.

A fresh Windows loaded-Zombie profile passes 4,918 checks and four World/HUD
frames. It compiles and reuses seven model parts, with 42 faces, 168 source
vertices and eight shadow quads in the observed drawing. World ticking returns
at `Minecraft.k:1328`; rendering returns through World1013, N569 and HUD2101.
Ten random/display passes finish 288,800 Block samples and 10,000 display
samples. Checked shutdown succeeds. The profile is registered as
`client.world_selection_zombie` when Windows display tests and imported
resources are enabled.

The [LivingEntity overlay body](../client/living-overlays.md) now handles the
hurt/death tint reached after damage. A development registered run passes in
33.65 seconds before the framebuffer correction. The preceding run with the
original SDK framebuffer request stops at the missing fire billboard body in
23.78 seconds, after progressing past the overlay. The subsequent
[fire billboard implementation](../client/burning-billboards.md) returns in
the same registered profile; its later run after the rotation correction
passes in 25.59 seconds. These runs and the earlier
focus-release and missing-overlay failures remain separate records. General
Canvas/focus transitions are still unfinished; a successful finite run does
not establish reliable extended play.
These results do not establish complete combat, original/native full frames
or a playable client. General Item/Block interactions, combat, Inventory UI and
extended save/reload play still need work.
