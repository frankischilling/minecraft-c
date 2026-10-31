# Base Entity environment helpers

These helpers execute the reached original methods on real constructed native
owners. They do not run Living travel, Player tick or an aggregate environment
tick. The runtime remains C17 and has no JVM dependency.

| Native entry | Client source | Server source | Source effect |
| --- | --- | --- | --- |
| `mc_entity_environment_water` | `sn.k_()Z`, Item `hl.k_()Z` | `lq.f_()Z`, Item `ez.f_()Z` | Bounds query and real World liquid acceleration |
| `mc_entity_environment_lava` | `sn.ah()Z` | `lq.ae()Z` | Bounds query and lava Material identity scan |
| `mc_entity_environment_stored_in_water` | `sn.ag()Z` | `lq.ad()Z` | Read existing `bx/bA` |
| `mc_entity_environment_clearance` | `sn.f(DDD)Z` | `lq.d(DDD)Z` | Shifted bounds, collision List size, then liquid scan |
| `mc_entity_environment_relative` | `sn.a(FFF)V` | `lq.a(FFF)V` | Exact relative X/Z velocity accumulation |

Native admission requires a genuinely held READY canonical Item, Falling Sand,
Minecart or client LocalPlayer. The actual class graph resolves every admitted
helper to the base method except Item water. Item's 16-byte override passes its
permanent bounds directly to the World. The inherited 36-byte water body first
expands `(0,-0.4000000059604645,0)`, then contracts `(0.001,0.001,0.001)` using
two objects from the actual shared AABB pool. The 27-byte lava body expands
`(-0.10000000149011612,-0.4000000059604645,-0.10000000149011612)` once. Reversed
bounds are preserved. The source's float-derived double constants and actual
Material static identities remain unchanged.

The code Registry is borrowed explicitly for water/lava. Entry validates its
side/profile and takes a counted native borrow without class active use. The
actual `g/h` Material read follows all reached bounds operations. It does not
initialize Block, including when Block is cold or failed. This context remains
available when the real Entity World is null; no World is invented to obtain a
Material. A nonnull World must use that same actual Registry and Entity classes
namespace. The borrow is balanced after every result, protecting Registry
retirement through synchronous descendants. Source initialization/acquisition
and pooled prefixes remain when a later operation fails.

Water and lava helpers never assign `bx/bA`. The water helper may change the
actual velocity fields through the existing World acceleration body; lava is a
read-only material query. The stored-water helper reads only the existing flag
and needs neither World nor floating arithmetic. The separate assignments in
the original base tick are not simulated here.

Clearance allocates one shifted pooled AABB without moving the permanent box.
It invokes the real World collision collector and reads the current returned
List's size. A positive count returns false before any liquid scan. Otherwise
it reloads the Entity's current World and calls the actual any-liquid scan on
that same shifted object. It preserves List identity/mutation and pool reuse;
it does not take a coordinate snapshot or implicitly reset either pool.

Collision collection now admits canonical LocalPlayer for the two exact
inherited null AABB virtuals. The complete `ls -> sn`, `gs -> ls -> sn` and
`dc -> gs -> ls -> sn` resolution chains prove that neither Living, Player nor
LocalPlayer overrides `f()Leq;` or `a(Lsn;)Leq;`. Both declaring `sn` bodies are
the same two bytes, `aconst_null; areturn` (Code SHA256
`146cb1bd5c639e50589db67598e4a3f423eb736f7911d3b8329f72e0f5cd5fac`).
This admission applies to both moving and nearby-candidate calls. Existing
unsupported concrete dispatch remains explicit: an actual nearby Minecart
still fails at the existing World PC218 rather than receiving a fabricated
null shape. These helpers do not complete every collision descendant.

Relative acceleration borrows the existing initialized immutable trig table
as native arithmetic context; it performs no World access and works on a real
null-World constructed receiver. Future callers must use the table belonging
to their actual shared game-class namespace. The method preserves binary32
square/sum, widened sqrt followed by float narrowing, `<0.01f` early return,
the `<1.0f` magnitude floor, friction division, yaw multiplication by
`3.1415927f` then division by `180.0f`, table sin/cos and X-before-Z stores.
Y and all other base fields remain unchanged. Intrinsic operand-register order
matches the captured x86-64 interpreter for NaN payloads as well as finite
values; comparisons are exact raw bits, without NaN normalization or tolerance.

Owners, contexts and dependencies stay live and exclusive throughout calls.
Native holds preserve storage, not mutable pool contents. Outputs/errors and
input views must be disjoint. Failure preserves outputs and completed source
prefixes; constructor diagnostics are untouched. Optional limits bound the
reached World material scan only. Existing collision collection and descendant
policies remain independent. No runtime budget is raised or replaced.

The reference metadata pins both entire original JARs, 17 class identities,
586 method entries and the original Code hashes. Ten paired-body checks compare
PC/opcode/immediate/branch structure; constant-pool targets, float constants
and dispatch chains were also inspected independently. The client observer
uses actual World/Chunk/Item/Falling/Minecart/LocalPlayer/Session constructors,
real publication and actual pool resets declared as fixture inputs. The
server observer uses actual null-World concrete constructors for relative
arithmetic and water/lava/clearance exception prefixes. No game classes are
replaced, no Unsafe construction is used and the graphical setup is not run.
Authored voxel/metadata/scalar inputs are supplied on genuine owners, not as
captured-answer fixtures.

The exact oracle compares 1,353 client records, 1,027 server records (relative
arithmetic and null-World prefixes), and 51 native server World-helper
crosschecks against client observations. Successful server World environment
execution is not claimed as an original-server process observation. Strict
controllers require completed bounded capture, explicit empty reader errors,
zero exit, empty stderr and a final positive count; matching JDK17 runtime and
compiler and supplied archive/library hashes are checked before original runs.

## Remaining active movement and tick work

`ls.a_(FF)V` remains unfinished. Its water branch now has the real stored flag
at PC1, relative acceleration at17 and clearance at115; its actual movement
call at33 is separate work. The lava branch uses the real lava query at132,
relative call148, movement164 and clearance246. Dry movement later calls the
movement body at571 and has its own friction, ladder and limb-state closure.
The parallel Player movement stage owns movement/effects work; this stage
does not replace it or implement the complete Living travel method.

The actual first-tick chain is also separate. `gs.w_()V` calls `ls.w_()V` at111;
`ls.w_()V` calls `sn.w_()V` at1; that invokes virtual `U()` at1; `ls.U()V`
stores its own previous animation value and calls `sn.U()V` at9. Base `sn.U`
calls the real water helper at81, but its remaining work includes rider
lifetime/virtual handling, splash sound/particle arguments and Random order,
water/fire state stores, actual Player/Living damage at501 and through lava
`ae()` at523, and watcher mutation `b(IZ)V` at565 and582. That setter reaches
`ud.b(ILjava/lang/Object;)V` at27/49 after actual Byte boxing. No generic
watcher write/active damage or tick success is invented by these helpers.
Existing World sound/particle entries are already implemented; reached
presentation callbacks keep their real failures. `ls.U` continues with its
own Random/living state after the base call, and Player tick has additional
sleep/statistics behavior. The successful helpers therefore do not establish a
successful active Player/Living tick.
