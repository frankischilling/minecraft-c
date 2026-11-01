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

The Windows `client.world_reload_cycle` test selects one saved World, saves and
quits, selects it again, then selects a different World. It checks three distinct
World/Player/SaveHandler/input/controller graphs, independent before/after save
decoding, and refusal to retire while an old Player has an external hold.
Releasing that hold permits the complete ownership audit and cleanup.

The current native invocation owner admits at most eight loads per application
session and retains old graphs until shutdown. Failed successor loads and
unrestricted reselection remain unfinished. This is a finite integration
checkpoint, not a complete playable session.

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
