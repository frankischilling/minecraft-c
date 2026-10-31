# Active LocalPlayer tick

`mc_player_tick` executes the inherited `gs.w_()` body on a genuine, ready
LocalPlayer. Its ordinary portal-false path now runs the actual `ls.w_()` and
`sn.w_()` wrappers, virtual Living environment update, `dc.o()` action prefix,
Inventory tick, Input update/copy, jumping, travel, nearby contact, animation,
container eligibility and play-time statistic dispatch. The actual application,
statistics writer and achievement notice remain the same owners used by the
Player constructor.

This entry supplies an Entity tick body. The surrounding World update,
chunk-membership/riding rules, outer Minecraft tick, render loop and interactive
frontend are separate work. A complete call in the qualified scene does not
establish a playable client or general tick parity.

## Calls and source order

The three entries deliberately name different source bodies:

| Entry | Body |
|---|---|
| `mc_player_tick` | inherited `gs.w_()` |
| `mc_living_tick_body` | the invoked ancestor `ls.w_()` on the same actual `dc` |
| `mc_local_player_action_tick` | actual `dc.o()` |

Virtual descendants remain the concrete Player's actual implementations.
`dc.o()` reads the live application's statistics writer, tests achievement
presence, and invokes the real reminder when absent. Presence includes a
zero-valued entry. It then updates portal counters, invokes the attached Input's
actual update, applies sneak height, performs four ordered World push-out
queries, and invokes `gs.o()`. Input copies and swing advancement still belong
to the later `dc.f_()` call; the Input algorithm is not duplicated here.

The Living wrapper invokes the existing actual environment tick before the
local action. Its animation tail preserves source float operations, angular
normalization order and previous-angle updates. The speed test uses the actual
`fcmpg` branch: NaN enters the moving branch. Its private atan2 is independently
qualified against JDK17. The Player wrapper preserves sleep-counter overflow,
real bed/Block identity reads, container eligibility, coordinate smoothing and
actual play-time statistic dispatch. Smoothing uses the deltas captured before
any distance reset, as the original does.

The Player container's eligibility method is a proved literal true return.
Workbench eligibility reads its actual World block, the live Block static and
coordinates, then the Player's squared distance. Unsupported concrete
containers remain explicit reached dependencies. Closing an ineligible
Workbench publishes `e=d` through the real superclass prefix before stopping
at the missing GUI call.

## Ownership and failures

The context borrows the actual code Registry and initialized trig table used
by existing travel/action code. Native holds protect the Player and Registry
through the synchronous call. All owners, contexts and policies must remain
live and exclusive across callbacks. Source World and Container fields are
reloaded where the original reloads them. There is no copied World, substitute
statistics callback, implicit bounds/vector pool reset or save.

Each call allocates one fixed-size private native diagnostic frame. Allocation
failure returns before the source body; all successful allocations retire on
every exit. This keeps nested genuine wrappers within the normal Windows
Debug stack without sharing a workspace between calls. Caller output is
written only at final return, including during real WorldAccess reentry. The
large environment/action child is tagged; read only the active union member.
Success returns a fresh empty error. Failure retains the completed source
prefix, wrapper PCs and full reached child diagnostics.

Strict x86-64 nearest-even arithmetic with gradual underflow is admitted at
reached floating operations. The Input extension checks its first actually
reached axis/sneak arithmetic after prior source stores. Literal-empty base
Input and an arithmetic-free jump-only update remain valid without that guard.
These native admission failures do not assert a Java exception outside the
qualified FP profile.

The optional animation ceiling counts pending normalization iterations. The
other policies pass directly to actual action/environment children. Exhaustion
fails before a pending iteration and retains prior effects; it never returns
successful shortened work. Large finite angles that cannot change after a
360-degree float addition are covered by a native failure-prefix regression.

## Reached frontiers

The ordinary complete path does not hide missing descendants:

* Portal true preserves the actual World/dismount prefix and stops at `dc.o`
  PC70, the missing live `Minecraft.r` screen owner. Screen closure, sound and
  dimension transition are later dependencies.
* Nonremote sleeping performs the real bed lookup and daytime test. Missing
  wake behavior stops at `gs.w_` PC53 or PC73.
* Ineligible Workbench closure preserves `gs.r`'s `e=d` store, then stops at
  `dc.r` PC9, the missing `Minecraft.a(Screen)` descendant.
* Map tracking/terrain updates, later Living hurt, wake/GUI operations and
  unsupported concrete Entity descendants retain their reached failures.
  Item pickup now has its own completed insertion/notification/death path;
  particle drawing and normal RenderEngine startup remain separate work.

Writer service belongs to the outer `Minecraft.k` PC18. It is not moved into a
Player tick. World area admission, Entity update/riding wrappers and frontend
input/timing/render composition are outside these entries.

## Finite evidence

The combined runtime passes 332 Windows Debug CTests and 310 cases on each
fresh sanitized Clang/GCC Debug build with leak detection: 952 completed cases.
All prior cases remain enabled, and all 859 C/header files pass clang-format 18.
The current products retain the active, allocator and Input counts below; the
original active-App execution retains its Windows scope. See the
[full test record](../testing.md#current-public-runtime-integration).

The native active-body suite passes 678 checks, its allocator suite 247, and the
Input suite 1,222. The private atan2 component compares 8,676 JDK results. These
counts describe their tests rather than a complete World update or playable
session.

The JDK/asset-independent native suite constructs real Registry, World, Chunk,
full application, Session, writer, notice, LocalPlayer and Input owners. It uses
authored resource files and actual loaded Chunk arrays. It exercises ordinary
and consecutive movement ticks, jumping, sneaking, swing/counter overflow,
sleeping, interpolation, portal decay, Workbench eligibility, occupied Stack
animation, NaN animation, statistics, delayed Input FP admission and exact
failure prefixes. A separate allocator suite verifies native allocation
failure, repeated retirement, the portal/work frontiers, and an actual
WorldAccess splash callback reentering the local action with independent
scratch and untouched caller output.

A native-only second-corner regression acquires a separately stored Chunk
containing a genuinely serialized Item. Its actual Entity-added WorldAccess
callback temporarily clears the Player's bounds after the first corner's
arguments were evaluated. The first push-out completes its velocity store;
the second corner then fails at the original bounds getfield PC349. The
test checks the retained World/Chunk publication and first velocity effect,
restores the real bounds allocation on every exit, and performs normal
terminal retirement. It replaces no getter/provider or source method.

The optional unchanged-original Windows profile constructs actual game owners
and calls the inherited `p.w_()` on genuine `dc`. It compares 39 raw-state rows:
34 complete calls and five actual null-reference prefixes, including 16
consecutive forward ticks. Every row includes position, velocity, bounds,
angles, animation, controls, counters, flags, seven real statistic totals,
notice identity/flags and Entity Random state. Actual top failure frames and
21 runtime class origins are checked. Ten further direct calls to the actual
private push-out body compare lower/upper occupancy, all four directions,
closed neighbors and strict nearest-direction ties on real World data. These
are separate from the 39 inherited tick rows. The native-only GUI-close frontier is
excluded from those 39 rows.

The original profile authors fields/Chunk arrays and reseeds existing Entity
and World Random owners after construction with the same explicit inputs on
both sides. It does not compare constructor entropy or full startup. Source
pools are reset explicitly by the harness between calls, never by production
runtime. Wall-clock notice timestamps are bounded independently around each
call rather than compared as equal elapsed time.

The separate atan2 component compares 8,676 raw results from actual JDK17
`Math.atan2` and `StrictMath.atan2`, including signed zero, infinities, NaN
payloads and deterministic finite words. This component has Windows and Linux
qualification; the genuine application profile is qualified only with the
pinned Windows LWJGL native. Floating exception flags and errno are outside
the component's value-result scope.
