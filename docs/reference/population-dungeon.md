# Dungeon source map

The [prepared whole-placement experiment](dungeon-placement.md) separately
captures complete original calls and matching native inputs. Its comparison
boundary differs from the direct loot/name helper observations below.

The verified [original archives](binaries.md) contain the stateless Dungeon
generator `er` on the client and `da` on the base server. Their complete
constructors, placement body and two private helpers have the same instruction
schedule after the explicit side-specific class mapping.

| Body | Client | Base server | Source positions |
| --- | --- | --- | --- |
| Constructor | `er.<init>()V` | `da.<init>()V` | Empty generator base call1, return4 |
| Placement | `er.a(fd,Random,III)Z` | `da.a(dj,Random,III)Z` | Dimensions5/14, validation89-250, masonry388-493, Chests541-751, Spawner781-812, true816 |
| Loot | `er.a(Random)iz` | `da.a(Random)fy` | Selector3, source allocations11-250, Item reads15-254, constructors18-259, NULL264 |
| Mob name | `er.b(Random)String` | `da.b(Random)String` | Draw2, Skeleton10, Zombie18/26, Spider34, fallback empty37 |

Placement draws the two horizontal half-widths before its first World query.
Validation traverses absolute X, Y, Z in ascending order. Every visited cell
queries Material, including cells that do not need a solid predicate. Floor
and roof require actual `Material.a`; perimeter openings require air at both
Y and Y+1. The complete scan precedes the one-through-five opening admission.
Six openings do not cause an early exit from the scan.

Construction uses ascending X, descending Y, ascending Z. It starts at Y+3,
validates but does not ordinarily rebuild the roof at Y+4, and clears four
interior layers. Boundary support below is queried before current solidity.
Only a solid floor cell consumes the moss/cobble draw. Late Block statics
`ap`, `x`, `av` and `at` provide the actual moss, cobble, Chest and Spawner IDs.
Their current numeric values are not a replacement palette.

All writes call the **notifying** `fd.f(IIII)Z` / `dj.e(IIII)Z`, whose quiet
setter call6 is followed by notifier18 only on true. The feature discards the
ordinary Boolean result while retaining all synchronous WorldAccess, neighbor,
lighting, removal and TileEntity effects. A native failure stops at its real prefix.

Each of two Chest groups tries three coordinates. Candidate X is drawn before
Z. An air candidate tests all four adjacent materials in -X,+X,-Z,+Z order;
exactly one solid neighbor admits it. A successful candidate retains the same
nullable Chest after its real TileEntity lookup and cast. Eight loot calls follow.
NULL loot skips both Chest size and slot RNG; nonnull loot queries logical
size27 before its slot draw and actual slot assignment. Repeated slots replace
the old Stack identity. An accepted group does not retry because its loot was
empty or some slots were overwritten.

Loot first draws a selector with bound11. Every allocating branch creates its
RAW Stack before later Item static reads and count draws. The source Item
overload reads actual `bf` at2, then invokes the numeric constructor at7.

| Selector | Actual Item static | Further draws | Count / damage |
| --- | --- | --- | --- |
| 0 | `ay` | None | 1 / 0 |
| 1 | `m` | bound4 | draw+1 / 0 |
| 2 | `S` | None | 1 / 0 |
| 3 | `R` | bound4 | draw+1 / 0 |
| 4 | `K` | bound4 | draw+1 / 0 |
| 5 | `I` | bound4 | draw+1 / 0 |
| 6 | `au` | None | 1 / 0 |
| 7 | `ar`, only after gate success | bound100 must be0 | 1 / 0 |
| 8 | `aA`, only after gate success | bound2 must be0, then bound4 | draw+1 / 0 |
| 9 | Captured `c[bd.bf + draw]` | bound10 must be0, then bound2 | 1 / 0 |
| 10 | `aU` | None | 1 / 3 |

Selector9 captures actual Item.c224 before reading bd227/bf230 and drawing at235.
Only then does it index that captured array239. A NULL bd fails before the last
draw; a NULL array fails afterward. A NULL selected element reaches the Item
constructor's receiver failure. Static and array reads perform actual Item
active use; diagnostic failed-prefix snapshots are not successful replacements.

The final Spawner lookup and cast precede the mob helper. Its real Random draw
selects the shared Skeleton, Zombie or Spider literal. Literal resolution
precedes even a nullable Spawner setter failure. Assignment changes only the
name; no spawn, tick, save or deferred-registration drain belongs to this body.

Absolute integer expressions and loop increments use Java wrapping. Extreme
inputs can reverse bounds, skip loops or fail to terminate; a fixed offset-box
walk or native work clamp would change those semantics.

[`DungeonLootObservation.java`](../../tests/reference/world/DungeonLootObservation.java)
binds the real constructor/private descriptors in isolated original loaders.
It records actual returned Stack fields, UTF16 names, Throwables and five
state-consuming post-call Random operations. Its bounded independent JDK seed
search chooses coverage inputs; it never supplies expected helper outputs.
Six fresh captures preserve cold failures and two explicitly Block-initialized
contexts per side. The latter compare with the same native production helpers
on MSVC and sanitized GCC/Clang: 924 case comparisons match. Whole placement,
natural reachability, class-context identity and JVM/native exception text are
outside those helper comparisons.

See the [runtime, ownership and executed checks](../world/population/dungeon.md).
Whole-call original comparisons and natural population qualification remain
separate from the source map and finite helper evidence.
