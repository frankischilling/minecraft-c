# Saved Lever circuit

`native-validation.json` records one post-fix native Windows SDK opportunity.
The prelaunch scene ledger is `tests/client/gameplay/lever_scene.json`. The
fixture authors a fresh 441-Chunk save before launch, preserving all 440 other
compressed Chunk records. It supplies two Lever Items, health 5 and a wall
attachment beside Redstone Dust. Ambient occlusion keeps its ordinary true
default. No live World, Player, clock, Random or input state is repaired.

Four transported SDL taps place the Lever, turn it on, turn it off through the
actual left-click mining-start hook, then turn it on again. Dust follows
0/15/0/15. Actual inventory count changes from two to one, used69 increments
once, and the original camera ray picks the Lever. Six returned terrain batches
match the active arrays of the separate 160-row original scene packet; six
outlines match all 18 independently derived vertex positions. The comparator
also checks 150 surrounding Block, metadata and light cells per phase.

Ordinary Save/Quit and reselection construct distinct World/Player owners.
Independent Region/Player NBT readback finds Lever69 metadata 12, Dust55 power 15,
health 5, selected slot 0 and Item69 count 1. All 441 saved Chunks remain, with no
Item Entities. Save time is 18; reselection reaches 20. On disk, only the actual
eight-byte session token changes during reselection.

The run passes 1,945 native checks and 19 controls that corrupt copies of actual
input, geometry, cell and outline evidence. All 2,143 selected source/product
inputs and 413 resource files remain unchanged. Passive observations find
natural READY Redstone particles in DN during states 11 through 20. This is
membership evidence during completed frames, without a per-particle draw
identity receipt. The powered screenshot was visually inspected locally.

Earlier evidence stays separate. The first pre-fix run passes its older scope.
A second run fails when ordinary powered Dust reaches the missing `im`
allocation at `N.a:554`. A deterministic callback regression reproduces that
failure twice before implementation. The successor implements the constructor,
tick and draw; it retains the same scene, input and frame limits. No seed,
emission, budget or assertion change avoids the reached dependency.

The public receipt contains metadata and hashes. Screenshots, saved Worlds,
imported assets, executables and DLLs remain local. Its exact source hashes
describe the captured worktree, separately from historical original evidence.
This finite native scene does not qualify general focus reliability, Linux
loaded play, arbitrary circuits, original whole-frame pixels/audio or FPS.

With imported assets and the built fixture, repeat in a fresh retained child:

```powershell
python -B tests/tools/client/test_lever_loop.py --executable BUILD/mc_test_lever_placement.exe --assets IMPORTED_ROOT --artifacts BUILD/lever-placement-captures
```

The optional CTest requires `MC_ENABLE_CLIENT_DISPLAY_TESTS`,
`MC_APPLICATION_STARTUP_IMPORTED_RESOURCES`, `MC_CLIENT_ASSET_IMPORT_DIR` and
`MC_ENABLE_CLIENT_LEVER_PLACEMENT_TESTS`. Public author/decoder/geometry controls
need no original archive, imported assets, Java or display.
