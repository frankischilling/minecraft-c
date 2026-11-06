# Player death and respawn

The client runs the actual Player death body, displays the genuine Beta death
screen and handles its Respawn button on the same loaded Overworld. Windows
scenarios now cover the ordinary spawn route and valid or missing saved Beds.
They return to healthy World rendering through actual SDK button interactions.
The fixtures supply fatal damage directly; ordinary hazard-driven death remains
outside that qualification.

## Death and screen lifetime

Fatal hurt reaches the inherited LivingEntity death body and the Player's
size, pose, motion, Inventory scattering and statistic stores. Main Inventory
slots precede armor slots, and each slot clears only after its scatter child
returns. Failed children retain earlier effects and their checked cleanup
owners. [Player damage](../entity/player-damage.md) records the source order and
the separate original and authored death comparisons.

The normal health tick invokes `Minecraft.a(NULL)` at 177. A nonpositive health
value constructs CH, releases gameplay focus and initializes its actual
Buttons. The transition graph retains that display invocation and its CH owner
across subsequent frames. Retiring it immediately would destroy the screen
before its first draw.

CH draws the original red gradient (`0x60500000` and `0xa0803030`), the doubled
`Game over!` title, the live Player score and inherited Buttons in source order.
Its score prefix is the original literal `Score: &e`, including the ampersand.
The Buttons use the literal labels `Respawn` and `Title menu`. A null Session
disables only Title menu. CH returns false from the pause query and overrides
the ordinary Escape handler with a no-op.

Title menu follows the existing actual World-leave and main-menu construction
route. The loaded scenario below qualifies Respawn interactions; it does not
qualify a complete death-screen Title menu session.

## Respawn order and ownership

The Respawn Button captures the current Player and calls `dc.p_()`, which enters
`Minecraft.a(false,0)`. The supported path uses the existing Load, World,
controller, Session, Registry and Math owners. It repairs spawn and cleans the
actual World collections, removes the old Player and constructs a replacement.
It publishes the new Player in `h206` and view in `i222`, performs spawn and
controller preparation, then inserts the Player into the World at 303.

Fresh movement input follows at 321. The previous Entity ID is restored, the
controller finishes loading and the ordinary terrain-preparation body visits
all 289 coordinates. No save reload or replacement World supplies that result.
The coordinator closes a current CH through its own `display(NULL)` at 369;
the CH handler then performs its separate source call at 30.

A successful coordinator is retired before that next CH call. This ends its
native Load lease, permitting a second Respawn on the same World. The
transition node keeps a scalar/identity receipt with expired child pointers
cleared. Failed source or cleanup prefixes retain their actual remaining
owners for checked retirement, without replaying the action.

The loaded failure control refuses the WorldAccess publication callback at
303. The new `h` and view stores have already happened, while input 321 remains
unreached and null. The caller's rejected output stays unchanged. The fixture
checks terminal cleanup of this partial graph; it does not fabricate a returned
respawn or resume the failed source body.

## Saved-Bed respawn

With `force=false`, the coordinator captures the current Player's actual saved
Bed coordinate and calls `GS.a(World,br)` at 63. The wrapper acquires its four
corner Chunks through one captured provider, checks the actual Bed ID and calls
the ordered exit helper. The helper reads metadata and the Bed facing table,
then searches two ordered 3-by-3 passes using the source support and air checks.
Overlapping candidates remain repeated visits. Native work limits report an
unfinished call instead of converting a partial search into a successful null
result.

A valid exit still passes through the ordinary spawn repair, World cleanup,
old-Player removal and replacement construction. At 242 the new Player stores a
distinct copy of the original saved Bed coordinate. At 281 it moves to the
selected exit using the original float offsets before widening to double, with
zero yaw and pitch. The saved coordinate remains the Bed position, not the exit
position. The same World, Entity ID, fresh Inventory/input and 289-coordinate
terrain preparation continue through the existing respawn suffix.

A null exit calls the actual LocalPlayer notification at 79 before selecting
the fallback World spawn at 91. The [HUD chat path](chat.md) translates
`tile.bed.notValid` through the configured resource owner and inserts a real SW
message. A failed notification stops before fallback. A successful missing-Bed
respawn leaves the replacement Player's saved Bed null. Forced respawn and a
null saved coordinate skip this validation branch.

Two independent Windows SDK opportunities pass on monitor 2. The valid profile
starts from saved coordinate `(3,64,4)` and preauthored Bed blocks, completes two
Respawn cycles and preserves that coordinate through a full World save. It
passes 1,013 checks in 14.86 seconds. The missing profile starts with the same
coordinate and absent Bed blocks, completes one Respawn and observes the
imported `Your home bed was missing or obstructed` message at age 9. It passes
977 checks in 11.00 seconds and saves no Bed coordinate on the replacement.
The before-Bed and returned notice captures were inspected.

Both cases call the real full-save body directly and independently parse the
saved Player NBT. They check health 20, empty Inventory, the valid XYZ tags or
their absence, and common terminal cleanup. They do not exercise an SDK save
menu or a subsequent restart. Imported resources and all 5,408 captured source
inputs stay unchanged.

The first two SDK records remain failed because the independent reader required
an empty Compound-list Inventory. Their native flows returned with 1,034 and
1,049 checks. The original `SP.a` body writes the empty list with Byte element
type 1; the reader now requires that exact tag instead of Compound type 10.
The fresh opportunities above pass the corrected strict readback. No saved
output or earlier failure record was repaired.

## Verification

| Scenario | Result | Scope |
| --- | --- | --- |
| Original Player hurt packet | 28 unchanged original calls; 21,247 scalar comparisons, 529 native checks and 64 reader controls per Python mode | Windows and Linux Clang ASan/UBSan replay. The completed fatal profile has empty Inventory, non-Notch username and null attacker. |
| Native Player death and scatter | 663 checks on Windows and Linux | Authored body, alias, failure, Notch Apple, attacker, Container-close and actual Cactus movement controls. Broader original runtime death comparison remains open. |
| Native World respawn leaves | 111 checks on each native host | Actual client spawn, repair, cleanup, removal and provider predicates with authored inputs. No original World runtime comparison. |
| Original CH components | Two original VMs agree on 72 rows; Windows and sanitized Linux replay each pass 369 native checks; readers pass 32 controls in normal and optimized Python | Genuine CH/DA/KE and Session constructors with a prepared original `kq` App. Initialization, literal Buttons, keyboard and nonpause behavior; no original drawing or full App construction claim. |
| Loaded Windows death/respawn | First opportunity: 1,049 checks in 14.83 seconds; strengthened opportunity: 943 checks in 21.11 seconds, both on monitor 2 | Two SDK Respawn cycles, fresh Players and Inventories, same World and Entity ID, health 20, fresh input, 289 terrain samples, returned frames and a separate publication refusal. Full terminal cleanup succeeds. |
| Saved-Bed components | Both full native runs pass all 14 focused cases | Coordinate/NBT, World Bed validation, original component readers and genuine native chat. Separate headless coordinate/World-Bed and GUI constructor checks pass without SDL/GL/audio dependencies. |
| Loaded Windows saved-Bed respawn | Valid: 1,013 checks, 14.86 seconds; missing: 977 checks, 11.00 seconds | Two valid-Bed SDK Respawns and one missing-Bed fallback, imported notice, independent direct-save NBT readback and common cleanup. |

The [CH packet](../../tests/reference/client/death_screen/README.md) keeps its
original construction limits explicit. The loaded
[scenario reader](../../tests/tools/client/test_death_respawn.py) requires both
completed cycle receipts and the separate failed-publication receipt. Captures
were inspected for the actual death screen, red gradient, title, live literal
`Score: &e0` and returned healthy frame. These are native scene observations;
they do not compare complete original frames or establish an FPS improvement.
The strengthened opportunity also checks that the menu's borrowed display
view ends before terminal dispatchers can consume its owner. Check totals can
vary with the number of returned frames; both exact records remain retained.

Before the saved-Bed extension, full native builds passed on both hosts. The
frozen Windows selection passed 14/14 tests in 21.15 seconds; Linux Clang Debug
with ASan/UBSan passed 13/13 in
4.33 seconds. Subsequent full runs retain their failures: Windows 750/755 in
519.34 seconds and Linux 731/735 in 735.36 seconds, with no skipped registered
tests and all 5,343 source inputs unchanged. Two older fixtures still expected
the formerly unavailable Container close. The remaining failures are strict
inherited-fixture hash guards, including the Lever scene's prelaunch refusal.
The [test record](../testing.md#player-death-death-screen-and-respawn) records
their corrections and separate reruns.

That corrected selection passed 16/16 on sanitized Linux and 17/18 on Windows.
The later Windows death/respawn assertion fails after one complete cycle. A
subsequent three-opportunity diagnostic cohort retains pass/fail/pass with
unchanged source inputs. Its instrumented failure completes replacement with
health 20, fresh input and matching World/ID, while Pause is open and Remote
Desktop owns foreground focus. Both successful opportunities complete both
cycles and cleanup. The failed cohort remains a focus-reliability limit; it
does not qualify general interactive survival play.

The asset-independent component cases are `entity.player_death`,
`world.respawn`, `client.death_screen`, `tools.death_screen_packet` and
`tools.death_screen_packet_optimized`. The loaded Windows case needs the
existing display/startup imported-resource configuration plus
`MC_ENABLE_CLIENT_DEATH_RESPAWN_TESTS=ON`. Build `mc_test_death_respawn`, then run:

```text
ctest --test-dir build-windows --no-tests=error --output-on-failure -R '^client.death_respawn$'
```

The scenario authors its own private save. It preserves imported resource bytes
and uses the existing checked loop, World and bootstrap cleanup.

The saved-Bed cases use
[their own driver](../../tests/tools/client/test_bed_respawn.py) and the opt-in
`client.bed_respawn_valid` and `client.bed_respawn_missing` registrations, enabled
with `MC_ENABLE_CLIENT_BED_RESPAWN_TESTS=ON`. Their
preauthored NBT and Region inputs establish the Bed state before bootstrap;
they do not obtain it through sleeping or live field repair.

## Remaining branches

A Dimension that rejects respawning, including the Nether path, reaches the
unavailable `Minecraft.m` call at 23. The centered KX ChunkProvider's recenter
child also remains unavailable. These remain explicit dependencies.

Sleeping and Bed-use interaction, loaded obstruction scenarios beyond the
finite helper cases, ordinary environmental death, extended death waiting and
dropped-Item lifetimes, wider combat, loaded Linux respawn, a complete original
restart, original drawing/audio equality and sustained frame times remain
unfinished or unqualified. These bounded scenarios do not establish complete
survival or respawn parity.
