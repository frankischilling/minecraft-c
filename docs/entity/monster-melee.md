# Monster melee damage

The actual `GZ.a(Entity,float)V` parent now invokes the target's damage body at
61. It stores attack cooldown 20 at 52, then reads the current Monster damage
field at 58. Either normally returned Boolean is discarded at 64, followed by
the void return at 65. A failed child keeps the earlier cooldown store and its
actual first cause. The canonical Zombie constructor supplies damage 5.

The [original source comparison](../reference/monster-damage.md) covers 32
prepared parent calls, including cooldown, distance and vertical gates,
unordered comparisons, null bounds, real Player damage, false returns and
failures after cooldown. Windows and sanitized Linux each match 46,828 shared
scalar positions; 75 original exception message/stack positions remain
unpaired. This is a protected-method comparison with a supplied distance.

Monster and Wolf share the entered damage adapter. Each caller retains its own
source sites and amount calculation. The actual damage producer protects its
failed wake, alert or death work independently of an optional diagnostic output.
A new rejection caused by already pending target work does not transfer that
older producer to the new Mob error. Checked retirement ends only the new
copied borrows. Nine direct controls cover real failed wake ownership, native
admission refusal and cleanup retry. Together with two actual World controls,
the fixture passes 916 checks on Windows and sanitized Linux.

World dispatch copies a small value receipt immediately after the Mob call,
before reusing its tagged child diagnostic. It contains actual caller/return
sites, child return/result, the health store and damage-statistic completion.
Health values come from the same source read and write; the statistic amount
comes from the actual GS152 dispatch. Entity identities are comparison-only,
with no ownership or later dereference. The client accumulates the receipt
once before success/failure handling and cleanup, including a refused cleanup
retry. Observation introduces no source getters, callbacks or gameplay decisions.

The actual World controls load 25 normal terrain Chunks and publish the genuine
Player and Zombie through the existing World insertion body. A normal Entity
tick deals 5 damage and retains the GZ61/65 value receipt after the successful
top-level diagnostic reset. A separate real failed wake preserves its first
cause and observation through refused retirement, then consumes pending work
on retry with the null Player List still installed. The initial fixture kept
an external Zombie occurrence during terminal World cleanup; checked retirement
refused it. Releasing that caller occurrence before World disposal fixes the
fixture without changing source fields or runtime checks. Both failed hosts'
outputs remain retained.

The separate saved-Zombie fixture prepares one genuine Zombie and 289 lit
Chunks before startup, then observes ordinary Timer, loaded Entity, AI and
damage work. It requires returned terrain/HUD, SDK Escape and Save and Quit,
independent saved NBT/statistics and checked cleanup. Its first visible
opportunity places both windows on monitor 2 but stops before World selection
because the Remote Desktop client retains foreground focus. That failure
remains retained. After the desktop is idle, a fresh native process completes
the hit/frame/pause/save/cleanup path; two independent readback assumptions in
its controller fail. Source-backed regressions distinguish authored empty
Inventory type 10 from serialized type 1, and require exactly one saved Zombie
while retaining ambient Entity and Chunk inventories.

The corrected fresh monitor-2 opportunity passes 509 native checks and every
independent readback. Actual creature behavior deals 5 damage, health 20 becomes 15 and
statistic2021 increases by 5. Time advances naturally to 13004, the surviving Zombie
has health 20, and paused Entity tags match the committed save. Actual hit,
pause and title screenshots pass; both windows close after checked cleanup.
The native product is unchanged from the complete Windows suite. The
[testing record](../testing.md#saved-zombie-monitor-2-encounter) preserves the
failed controller, processing-only revalidation and fresh execution separately.
This qualifies the prepared saved hostile encounter. Natural spawning,
unrestricted combat, sustained FPS and original audiovisual equality remain
unqualified.
