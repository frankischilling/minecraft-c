# Zombie damage

The client Zombie now handles damage with a NULL attacker through the actual
Monster and LivingEntity methods. Environment damage, including the periodic
fire call, reaches the same production body as a direct damage call.

The body preserves the remote-World and dead-Entity gates, hurt resistance,
stronger repeated hits, health stores, hurt timers, attacked flag and direction.
A full hit consumes the shared Math random stream before the private Random
calls used for the hurt sound. Sound dispatch uses the captured World receiver,
the actual Registry String and ordinary WorldAccess callbacks. A successful
NULL-attacker hit preserves an existing Cow target when the original Monster
branch returns before the target store.

This section records the canonical client Zombie implementation and its
observations. [Skeleton NULL-attacker damage](skeleton-damage.md) now enters the
same inherited body through its own receiver and sound declarations. Damage with a
nonnull attacker remains unfinished. Fatal damage plays its reached death
sound, then stops at the actual death/drop invocation. It does not claim that
the Zombie died or that an Item dropped. Other LivingEntity subclasses retain
their explicit missing damage bodies.

## Verification

Twelve authored CPU profiles use genuine Worlds, 25 stored Chunks, Registry
classes and ordinary Zombie/Cow construction. They cover fresh and repeated
hits, early returns, NULL World and Random failures, fatal and nonnull-attacker
prefixes, a nested periodic-fire Mob tick, callback failure and an existing
target alias. Constructor entropy stays independent; observed original seeds
and field values are never inputs to the native execution.

Windows MSVC Debug and Linux Clang18 ASan/UBSan each pass 2,179 native
checks. Comparison with the unchanged original observations passes 3,544 typed checks, 3,861 identity relations and
84 separate-process stream checks. Six direct calls genuinely return; two
profiles match NULL failures. Fatal, nonnull-attacker and callback outcomes
retain their narrower declared prefixes. The nested fire comparison covers the
sound-entry prefix and named health/timer fields, rather than full tick equality.

Eight comparator corruptions, three original-manifest corruptions and four
build-record corruptions are rejected. An ordinary native sound callback
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
