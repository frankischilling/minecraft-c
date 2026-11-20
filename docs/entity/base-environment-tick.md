# Local Player base and environment update

`mc_entity_tick_update_in_context` implements actual `U()` dispatch on a
constructed canonical client LocalPlayer: `dc` inherits `ls.U`, which first
invokes `sn.U`. This is the environment and counter update reached inside the
wider `w_()` chain. It does not replace action, travel, animation, containers,
statistics or the World update.

The API receives the actual Registry independently from the current World and
retains the Player for the call. Owners and relationships remain live and
exclusive through synchronous descendants. It never supplies a synthetic World,
resets a geometry pool or advances another tick. The separately named
`mc_entity_tick_base_body_in_context` exposes only the `sn.U` body.

The base body preserves riding-field handling, tick count and previous fields
before the real water query. It uses the existing water acceleration and lava
helpers. Entering water retains the original sound, both particle loops,
float/double boundaries, actual Random draws and live field reloads. Sound and
particles use existing typed WorldAccess dispatch; callback changes affect
subsequent reads. Fire, lava and void delegate to the real Player damage path,
preserving its reached failures. Both nonremote watcher calls execute before
the first-update flag clears.

The typed watcher setter performs the original Byte getter, masked Java bit
operation and separately reloaded setter. An equal Byte leaves both dirty flags
alone. A changed Byte stores the value, then record dirty, then watcher dirty.
Unsupported Object projections fail at their reached boundary. No generic
Object or fabricated Box is supplied, and watcher16 remains unchanged.

The Living continuation consumes `Random.nextInt(1000)` before incrementing its
private sound counter. Its inherited ambient-name method really returns null
on LocalPlayer, so that branch reaches no sound or extra float draws. Alive,
sleep-aware suffocation samples up to eight original World solid-support points and
Player eye height0.12f. Eye immersion reuses the existing material helper,
including floor-to-float-to-floor Y. Air, drowning, damage/death counters and
final copies retain their source order.

Particle-work exhaustion fails before a pending iteration and retains completed
draws/callbacks. Material work uses its independent existing ceiling. Neither
limit reports truncated work as success. Strict x86-64 floating arithmetic is
checked at reached operations. In drowning, the two Random draws precede the
first subtraction's FP admission. The two-NaN addition rule used by existing
geometry preserves the measured interpreter payload choice.

Larger damage reaches the unfinished actual LocalPlayer application/statistics
dispatch. Death after its threshold reaches the real Player.K prefix and
unfinished inventory close. Later Living hurt/armor/death and the full wrapper
chain remain dependencies. The explosion continuation is source-present after
K, but current K fails first; that continuation has not been execution-verified
through a completed close and is not a completed death-update claim.

The native test constructs real Registry, GAME World, loaded Chunk, Session,
LocalPlayer, geometry pools and WorldAccess owners with authored resource/NBT
inputs. It needs no original assets or JDK. It checks callbacks, limits, watcher
dirtiness, source prefixes and repeated lifetime/accounting. Its256 successive
calls reset pools explicitly as a caller operation; production updates never do.

The optional original comparison calls unchanged `dc.U` on genuine constructed
owners. It records31 cases:22 completed calls and nine interrupted prefixes.
The230 exact records include raw state, all199 sound/particle calls, Random
state and AABB/Vec3 cursors. Cases cover flow, splash suppression, fire/immunity,
lava, void, suffocation, drowning, counter wrap, callback changes/failures,
null World, signed zero, infinities and distinct NaN payloads. Scalar fields,
Chunk cells, explicit pool resets and post-constructor Random.setSeed are
authored inputs. Both sides prepare actual statistics before Player construction.
Native translation inputs are authored; original resources stay original.
Registry/resource contents and constructor entropy are not compared.

The nine prefixes stop at actual null-World/null-Minecraft access or authored
WorldAccess failure. Their state is not a completed-method claim, and native
status is not equated with a Java exception type. Full arrays, default startup,
arbitrary inputs, larger damage, complete Player ticks and gameplay parity
remain outside this finite profile.
