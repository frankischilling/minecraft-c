# Player death and respawn

The client now runs the actual Player death body, displays the genuine Beta
death screen and handles its Respawn button on the same loaded Overworld. One
Windows scenario completes two ordinary SDK button interactions and returns to
healthy World rendering after each. Its fatal damage is an authored direct
100-damage call; ordinary hazard-driven death remains outside that qualification.

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

## Verification

| Scenario | Result | Scope |
| --- | --- | --- |
| Original Player hurt packet | 28 unchanged original calls; 21,247 scalar comparisons, 529 native checks and 64 reader controls per Python mode | Windows and Linux Clang ASan/UBSan replay. The completed fatal profile has empty Inventory, non-Notch username and null attacker. |
| Native Player death and scatter | 663 checks on Windows and Linux | Authored body, alias, failure, Notch Apple, attacker, Container-close and actual Cactus movement controls. Broader original runtime death comparison remains open. |
| Native World respawn leaves | 111 checks on each native host | Actual client spawn, repair, cleanup, removal and provider predicates with authored inputs. No original World runtime comparison. |
| Original CH components | Two original VMs agree on 72 rows; Windows and sanitized Linux replay each pass 369 native checks; readers pass 32 controls in normal and optimized Python | Genuine CH/DA/KE and Session constructors with a prepared original `kq` App. Initialization, literal Buttons, keyboard and nonpause behavior; no original drawing or full App construction claim. |
| Loaded Windows death/respawn | First opportunity: 1,049 checks in 14.83 seconds; strengthened opportunity: 943 checks in 21.11 seconds, both on monitor 2 | Two SDK Respawn cycles, fresh Players and Inventories, same World and Entity ID, health 20, fresh input, 289 terrain samples, returned frames and a separate publication refusal. Full terminal cleanup succeeds. |

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

Full native builds pass on both hosts. The frozen Windows selection passes
14/14 tests in 21.15 seconds; Linux Clang Debug with ASan/UBSan passes 13/13 in
4.33 seconds. Subsequent full runs retain their failures: Windows 750/755 in
519.34 seconds and Linux 731/735 in 735.36 seconds, with no skipped registered
tests and all 5,343 source inputs unchanged. Two older fixtures still expected
the formerly unavailable Container close. The remaining failures are strict
inherited-fixture hash guards, including the Lever scene's prelaunch refusal.
The [test record](../testing.md#player-death-death-screen-and-respawn) records
their corrections and separate reruns.

The corrected selection passes 16/16 on sanitized Linux and 17/18 on Windows.
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

## Remaining branches

A nonnull saved Bed with `force=false` still stops at the reached `gs.a` child
at 63. A Dimension that rejects respawning, including the Nether path, reaches
the unavailable `Minecraft.m` call at 23. The centered KX ChunkProvider's
recenter child also remains unavailable. These are explicit dependencies;
they are not substituted with the supported Overworld path.

Ordinary environmental death, extended death waiting and dropped-Item
lifetimes, wider combat, loaded Linux respawn, original drawing/audio equality
and sustained frame times remain unfinished or unqualified. This bounded
scenario does not establish complete survival or respawn parity.
