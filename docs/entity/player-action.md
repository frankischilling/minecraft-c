# Player action and nearby contact

`mc_player_action` executes the actual `gs.o()` ancestor body on a held,
ready LocalPlayer. It ticks that Player's Inventory, copies the previous
head-bob value, invokes the real `ls.o()` body, updates head bob, and visits
the actual nearby Entity List. It does not execute the earlier `dc.o()`
tutorial, portal, Input update or push-out prefix. The separate
[active Player entry](player-active-tick.md) composes those bodies with both
`w_()` wrappers. World scheduling remains unfinished.

The context borrows the existing Registry namespace and initialized trig table
through `mc_player_travel_context`. Its optional Inventory limits are passed
to the genuine Inventory tick. The separate action work ceiling counts pending
angle-normalization and List iterations. Reaching a ceiling returns a failure
with the completed prefix; it never completes a shortened action.

`mc_living_action_body` names the exact invoked `ls.o()` body on the same
concrete Player. It performs interpolation, collision correction, the actual
Player immobilization check, conditional input copying and jumping, damping,
Player travel, and nearby pushing. `mc_local_player_action_input` executes
`dc.f_()`: the real `gs.f_()` swing counter first, followed by three separate
reads of the attached Input for strafe, forward and jump. It does not call
Input.update. The earlier `dc.o()` owns that update.

## Source order and ownership

The implementation keeps the World receiver captured before evaluating each
query's bounds argument. The returned collision and nearby Lists stay live:
each source size/get operation rereads the same List identity. A query lease
does not turn a pooled box or mutable List into a snapshot. No pool reset is
implicit. Each selected Entity has an independent local hold through its
virtual calls. Every exit releases that hold, the query lease and the pooled
argument hold while retaining completed game state.

The current native Entity families have these actual descendants:

| Receiver | `i_()` | `h(Entity)` | `b(Player)` |
|---|---|---|---|
| LocalPlayer | Living `!be` | base Entity push | inherited literal return |
| Item | inherited false | base body, if directly reached | real Item insertion, notification and death check |
| FallingSand | inherited false | base body, if directly reached | inherited literal return |
| Minecart | literal true | real Minecart push with Player argument | inherited literal return |

Other concrete descendants remain explicit dependencies at the reached call.
The base push uses the original maximum absolute horizontal delta and its
float-returning square root. The Minecart path preserves its distinct squared
distance threshold, scaling, remote-World return and passenger identity test.
The actual canonical Player argument excludes the original auto-mount and
Minecart-versus-Minecart branches; those branches are not fabricated empty.
Velocity additions include the original vertical `+0.0` store.

Collision candidate `f()` and moving `a(Entity)` have separate admissions.
`yl.f()` is literally null, so a nearby Minecart can reach Player pushing.
`yl.a(Entity)` returns the argument's permanent bounds. A moving Minecart's
different pair-shape body remains unsupported at `fd` PC 260. The regression
checks that boundary rather than treating it as a null box.

Failures retain separate GS action, LS action, DC input, GS swing, private GS
contact, push, pickup, position and angle PCs, together with complete child
Inventory, travel, movement, environment and World-query errors. Success
returns a fresh empty error. Strict x86-64 nearest-even FP with gradual
underflow is admitted at reached arithmetic. Native admission failures do not
stand for Java exceptions outside that execution profile.

## Finite qualification

The ordinary native test creates real Registry, World, loaded Chunk, Session,
LocalPlayers, Minecart, Item and Stack owners using the existing authored
resource/Chunk fixture. It needs no game archive, imported assets, JDK, SDL or
OpenGL. The scene uses actual World/Chunk publication and real nearby queries.
It tests occupied Inventory animation, interpolation and collision correction,
immobilization, swing overflow, input copies, Player/Minecart contact,
resistance and NaN inputs, remote/dead Minecarts, query cleanup, pending loop
limits and delayed FP admission. It also checks the moving-Minecart PC 260
boundary separately.

The optional reference controller compiles a source-only observer against the
supplied unchanged client JAR. Its actual `dc` receiver is constructed with a
real World, Chunk and Session. A JVM special invocation selects `gs.o()` while
every virtual descendant still dispatches on that real receiver. Direct input
cases call the actual `dc.f_()`. The helper defines no replacement game class,
uses no Unsafe allocation, and verifies actual game-class origins.

There are 36 exact raw-state records: 32 complete calls and four interrupted
prefix comparisons. Two original calls fail in `dc.a` because their Minecraft
field is null. Those two rows compare the completed state prefix and the canonical statistic
descriptor/amount. Separate full-App tests cover completed statistics through
the concrete application bridge. Two other calls fail at the actual null Input access in
`dc.f_()` on both sides. The original helper verifies the top failure frame.

The additional zero-delay Item case uses a null-App fixture. It retains real
Inventory insertion, sound and Random effects before the notification fails
at `hl` PC123 / `dc` PC4. That case remains outside the 36 original records.
Separate real-App tests now complete notification, enqueue the particle and
reach the later Item death check. The nonzero-delay Item still returns after
reading Stack count. See
[Item insertion and pickup](item-pickup.md) for the connected closure and its
separately qualified original observations.

The original profile explicitly prepares real `jl` statistics before Player
construction, authors source fields and Chunk arrays, and sets the existing
Entity Random to seed 38 after construction. Constructor entropy and the full
startup/tick sequence are outside this profile. The controller compares every
reported position, velocity, angle, control, swing, head-bob, counter, bounds,
scene velocity and Random-state bit without tolerances or captured answers.

## Math component and provenance

The private atan port originates from permissively licensed
[Netlib `s_atan.c`](https://netlib.org/fdlibm/s_atan.c). Its Sun permission
notice is retained, with attribution in
[`licenses/fdlibm-action-atan.txt`](../../licenses/fdlibm-action-atan.txt).
The kernel was separately audited against
[OpenJDK17.0.17+10 `s_atan.c`](https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/java.base/share/native/libfdlibm/s_atan.c).
Its post-include operator/coefficient tokens match exactly. The OpenJDK source
is an audit input and is not redistributed here. The native port uses alias-safe
word extraction and exact coefficient words; it does not call host atan.

The exact binary32 remainder by360 uses significand reduction and preserves
the qualified JVM NaN and signed-zero results. A separate component probe
compares 8192 atan inputs and 8192 remainder inputs with actual JDK17 operations,
including subnormal, threshold, signed-zero, infinity, NaN and authored
xorshift inputs. It also requires Math.atan and StrictMath.atan to agree in
that original `-Xint` profile. This component evidence is distinct from the 36
actual game-method records; floating flags and errno are outside its scope.

`tests/reference/entity/action/source-pins.json` pins the supplied client JAR
and 20 whole classes with every selected class's method Code hashes. The
dispatch audit resolves 24 actual current-family virtuals and verifies the
literal empty/null bodies separately. `math-origin.json` records the two
upstream source identities. Both audit scripts accept explicit local inputs.
No original archive, class, asset, captured answer or generated world is part
of the source package.

All subprocess captures have independent stdout/stderr byte ceilings,
deadlines, retained exit/capture metadata and strict terminal-marker checks.
The original controller pins runtime, compiler, dependency archives, helper
sources and native executable before/after execution, and pins compiled helper
classes immediately after compilation and after the runs. Reports and fresh
world directories stay in the caller-selected private output directory.

Travel statistics use the [application bridge](../client/player-statistics.md).
The [active Player entry](player-active-tick.md) supplies dc.o and both wrappers;
[Item pickup](item-pickup.md) supplies the connected notification/death path.
Each has separate qualification. World Entity scheduling, remaining concrete
Entity descendants and a playable client remain unfinished.

Travel and Item pickup diagnostics now share a tagged union in the Action error.
A failed travel returns before pickup; reaching pickup requires completed travel
with a fresh child. Read `.pickup` for `MC_PLAYER_ACTION_CHILD_PICKUP`, and
`.travel` for NONE/TRAVEL. Successful calls restore the legacy fresh travel
sentinels and NONE. Independent meaningful siblings/PCs remain separate.
