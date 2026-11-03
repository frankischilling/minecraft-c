# Fluid World rendering

The type 4 World path renders flowing and still Water and Lava through
`cv.k(Block,III)`. It keeps the original face order, repeated cache reads,
weighted corner heights, pooled flow vectors, float arithmetic, atlas
coordinates and per-face brightness. Section compilation uses the enclosing
Tessellator batch and publishes Water in pass 1 and Lava in pass 0.

The initial visibility check precedes render-all. An enclosed fluid therefore
returns false even with render-all enabled. Top and side texture lookups ignore
the CV texture override; the bottom face uses its ordinary face helper. Live
Block texture fields remain observable. Successful visible calls set the lower
and upper Y bounds to 0 and 1; hidden and failed calls retain their entered
bounds and Tessellator state.

## Original evidence

The [reference packet](../../tests/reference/client/fluid_world/README.md)
contains 104 calls in three cohorts. Each cohort runs two fresh ordinary VMs
and a third fresh VM under read-only JDI. All nine executions complete, with
identical stdout within each cohort. The supplied client archive has SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.

Genuine saved Worlds, loaded Chunks, caches, RenderBlocks, Vector pools and the
original 2,097,152-word Tessellator backing provide the owners. Each cohort has
one begun batch, declared translation changes and no draw or GL context.
Producer and compiled-helper fingerprints remain unchanged through capture.
Public products contain observations and fingerprints, without original classes,
Code bytes, assets or archives.

The matrix covers all metadata nibbles for registered IDs 8 through 11;
isolated faces, hidden fluid, render-all, weighted and upper-fluid corners,
Ice/Air/Sand/Stone neighbors, horizontal and falling flow, mutable textures,
overrides, partial bounds and null Block/cache prefixes. The trace records
48,731 events, 12,209 cache queries, 774 selected static reads, 688 selected
field stores and two reached null failures. Appended output totals 9,504 words.

## Native verification

`client.fluid_world` replays the three cohorts in fresh native processes.
Its 416 exact rows compare inputs, actual cells and complete Chunk-array
fingerprints, CV/NW fields, bounds, return/failure prefixes, reported backing
words and Vector pool history. Separate native checks audit 218,094,304
non-emitted backing words against each process's own prior memory and verify
buffer identities, World bindings and cleanup. These outer comparisons do not
establish native per-query trace equality.

`client.fluid` also limits native Vector allocation after the initial pooled
zero vector. The next addition must fail at the reached factory invocation,
retain the acquired slot and cursor, and leave CV, Block bounds and NW unchanged.
Both client `BT.c(DDD)` and server `BA.c(DDD)` bytecode invoke that factory at
PC 19 and return at PC 22.

`client.chunk_compile` projects eight original profiles into real section
compilation: still/flowing Water/Lava at metadata 0 and horizontal-flow
metadata 3. It compares all active position, UV and color words, retains its
own inactive words, reuses immutable definitions and recompiles after actual
WorldAccess light notifications. A remaining unsupported Fire child still
tests the failed compilation prefix.

The Windows `client.world_selection_water` and `client.world_selection_lava`
scenes load their fluid, Sand support, Stone standing platform and Player pose
from independently decoded saved data. Returned World/HUD frames use that same
Player camera, select Sand through the liquid, reuse their compiled section and
recompile after a real light notification. Each scene compares all 30 fluid
vertices with the matching original coordinate/translation profile. Position
and UV words match exactly. Loaded lighting advances normally, so raw colors
and their match count remain separate observations. Both scenes preserve every
pre-existing save/resource file and create only the actual session lock.

Run the CPU checks with `ctest --test-dir build --output-on-failure -R
'^(client.fluid|client.fluid_world|client.chunk_compile|tools.fluid_world_packet)$'`.
Loaded scenes require the Windows imported-resource/display configuration in
[build setup](../building.md).

Original whole-frame pixels, native per-query trace equality, unrestricted play,
automatic Tessellator flush, other specialized World families and FPS remain
unverified by these scenarios.
