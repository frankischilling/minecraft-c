# Selected blocks and World reselection

The loaded World frame now calls the actual damage/placement overlay and block
outline methods after picking. Each call reads the current HitResult and held
stack again. The outline uses the block's state-dependent selected box, expands
it by the original float epsilon, subtracts the interpolated camera position,
and draws two line strips and four vertical edges. Snow uses its selected height
rather than a unit cube or its collision height.

The ordinary Windows SDK tests load authored region files through the title
screen and World selector. They aim at dirt or a snow layer with an empty hand;
the test never assigns a HitResult, mining progress or runtime block bounds.
Four profiles cover the normal and rounded window sizes. Each checks three
returned World/HUD frames, real picking, pooled-box ownership, all eighteen
submitted vertices, requested graphics state, and cleanup. The Python verifier
recomputes the geometry from the actual Player coordinates and partial tick.
It also decodes the saved target chunk independently.

The draw receipt reads the actual shared integer buffer's upload position.
`nw.draw` resets the byte buffer's limit to capacity before returning, so that
reset limit cannot describe the submitted geometry. Receipts preserve the
reset range separately from the submitted span and copy the retained upload
bytes without rebuilding vertices.

## Saving and selecting again

The source loader can retain successive load invocations while the same
RenderGlobal and particle renderer replace their World fields through the
ordinary source calls. Earlier App World and Player slots stay cleared after
Save and quit; native cleanup does not restore them. Completed draw records
are retired before source pool resets or terminal ownership audits.

The Windows `client.world_reload_cycle` test now completes ten saved-World
loads and ten ordinary Save and quit transitions. It first reopens the same
World, then alternates two Worlds. Every World/Player/SaveHandler/input/controller
graph is distinct and remains live. The test independently decodes every save,
compares the reopened Player's stable fields and checks the actual N/DN World
replacement calls. An eleventh selection enters the existing unsupported
conversion body before acquiring storage. Its original failure prefix and the
ten successful graphs survive until checked cleanup.

There is no fixed load or screen-transition count limit. Each native record
is allocated when needed; allocation failure and `size_t` overflow remain
explicit errors before adding a record. Terminal classification follows the
actual reciprocal load links in screen-node order, checks the complete class
count and rejects skipped, repeated or reordered owners. This replaces a
quadratic duplicate search. Each load also retains a pointer to its actual
controller owning slot. Source controller replacement updates that pointer
when it moves the old occurrence into the successor's node; terminal admission
no longer scans every screen for each controller. Neither change adds work
to the frame loop or changes source World publication.

An extra old-Player hold still refuses the passive audit with every World
READY. Releasing it permits a fresh audit and complete cleanup. Separate
effect/controller/screen holds exercise later refusal and retry. Raw disk
snapshots also prove that failed conversion and native cleanup write no save
bytes after the tenth Save and quit. The
[native validation record](../../tests/reference/client/world_reload/native-validation.json)
records the retained and hash-bracketed snapshots separately. The latter passes
6,417 native reload checks in 36.96 seconds. Its nine-test Windows selection
passes in 62.66 seconds, with 1,539 selected execution inputs and 410 selected
resource files unchanged. The inventory covers runtime/build inputs and four
chosen harness files; it does not cover every support file, imported icons or
desktop state. Subsequent runtime changes need fresh qualification.

Old World graphs still remain allocated until shutdown. Reclaiming superseded
Worlds during play, failures after a successor acquires storage, unrestricted
reselection and extended play remain unfinished or unqualified. These native
controls do not establish original whole-session or pixel equality.

## Chunk lookup

Source-owned ChunkProviders now have a private coordinate index. Nested cache
mutations use the generic path; exporting a mutable cache permanently disables
the index. Store lookup has a separate private state-identity index, with the
canonical linked list still defining ownership and order. Allocation or budget
failure falls back to that list. These indexes change lookup cost, not source
Chunk publication, callback order or persistent formats.

Public controls cover cache exports, nested callbacks, acquisition and saving,
index allocation limits, and selected-box dispatch. These native controls and
authored graphical scenarios do not establish original-frame pixel equality,
positive-progress mining/crack rendering, placement, or extended playability.
