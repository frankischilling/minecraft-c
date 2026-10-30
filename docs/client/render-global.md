# RenderGlobal and chunk-renderer ownership

The canonical `n` owner constructs RenderGlobal on the actual App, Engine and
Display. Startup calls it at625, publishes `g628`, applies the viewport and
constructs the particle renderer before publishing `j660`. Selected-World
loading now invokes the actual `n.a(fd)` attachment. This constructs the source
chunk-renderer grid and its bounding-box lists. Terrain mesh compilation and a
complete World frame remain unfinished.

The interfaces are [RenderGlobal](../../include/mc/client/world/render_global.h)
and [ChunkRenderer](../../include/mc/client/world/source_chunk_renderer.h).
Cold allocation is separate from source construction. Failed arrays, buffers,
batch owners and list definitions retain their reached stores and typed causes.

## Constructor state

Construction creates direct buffers, reserved display-list ranges, optional
occlusion-query names and four batch owners. It defines stars and the upper and
lower sky planes through the same `nw` tessellator. Names come from one Display
allocator shared across Engine namespaces. A null App fails after the earlier
constructor effects; the control does not substitute another App.

Each accepted star contributes four source vertices. The original tessellator's
`b=true` profile expands each quad to six triangle records in order
`[0,1,2,0,2,3]`. Each sky plane has 169 source quads and 1,014 submitted triangle
vertices. Upper sky uses 169 batches; lower sky uses one. Compiled-command
inspection borrows those actual records. Device controls check copied XYZ and
the position-only array profile; disabled attribute slots are not asserted equal.

## World attachment and the DK grid

`n.a(fd)` removes the old WorldAccess occurrence at12, resets the three distance
fields, calls the shared RenderManager World setter at40, stores `k45`,
constructs a new `cv` with that World at54 and publishes `u57`. A nonnull World
then receives the same RenderGlobal interface at66 before grid rebuild70.

Rebuild updates the actual Leaves graphics setting, reads render distance,
clears each old DK's World reference, allocates `o/n`, resets the range fields
and clears the existing dirty and TileEntity Lists. Construction loops X, Y, Z
in source order. Array indexing, queue insertion and list offsets retain their
separate source order.

| Render distance H | Grid p × q × r | DK owners |
| --- | --- | --- |
| 0 | 26 × 8 × 26 | 5,408 |
| 1 | 17 × 8 × 17 | 2,312 |
| 2 | 9 × 8 × 9 | 648 |
| 3 | 5 × 8 × 5 | 200 |

Each genuine `dk` owns its two-element boolean array and distinct List `B`,
borrows the actual shared List `F`, retains the World and constructs its
position fields and bounding box. Its display-list base is `n.s + 3*ordinal`.
Positioning defines `C+2` with inherited `bw.a(eq)`: 24 source vertices become
36 submitted triangle vertices through the same `nw`. This method does not
construct a `bb`, create another Random, compile terrain or increment the
chunk-update counter.

The initial selected-World path reaches N before App `i` is published, so the
camera recenter/sort branch is genuinely skipped. A nonnull camera reaches the
explicit unfinished branch at675. Same-name list replacement supports later
positioning, but does not complete the N recenter/sort caller, visibility,
update scheduling or `dk.a()` terrain compilation. World cell/range callbacks
now mark the actual DK owners and dirty List. Appearance callbacks use the same
graph; see [image downloads](image-downloads.md).

## List replacement on OpenGL 3.3

The source-list bridge stores commands behind the original reserved logical
names and executes them on the actual OpenGL 3.3 context. `glNewList` in the
selected `GL_COMPILE` profile starts a separate pending definition. The old
definition stays associated with the same name until `glEndList` publishes the
replacement. Empty replacement is supported. This follows the
[OpenGL display-list association rule](https://registry.khronos.org/OpenGL/specs/gl/glspec21.pdf#page=256).

Old and pending storage both count against the unchanged 32 MiB command quota
and range command limits. There is no early eviction or replacement list ID.
A native failure retains the committed definition and exact pending prefix.
Native abort ends compilation without acknowledging source `glEndList`; it
retains the pending commands for checked range cleanup. The committed definition
can still execute afterwards. Inspection exposes committed records and pending
counts, bytes and batches. Borrowed views expire on successful replacement of
that name or range retirement.

The 786,432-name source range remains one range. The N constructor arena keeps
its default 8 MiB limit. Display metadata, compiled commands, the tessellator
and the shared Entity bounds namespace have separate accounting. The N arena
count is not total process or driver memory.

## Ownership and retirement

Source `n.k`, RenderManager World, `cv.c` and each `dk.a` require independent
native World occurrences. Source `n.a(NULL)` removes the WorldAccess occurrence
and replaces `k/u`, but leaves the old DK arrays and their World references
intact. Those references end at a later source DK clear or checked native cleanup.

The owner retains its graphics namespace and context independently of App
publication. Optional query names keep their original context generation and
buffer alive until checked deletion. Failed cleanup retains its rights and the
same slot for retry. Strict retirement refuses external occurrences and active
callbacks before revocation. Terminal coordination detaches WorldAccess users
and retires DK/CV references before their dependencies die. It does not replay
World attachment or invent source disposal calls.

`mc_client_render_global_terminal_detach` adds a one-way native preparation
policy. Once admitted, it marks N RETIRING and closes source attachment,
marking, culling and retain admission. It ends only N's local WorldAccess,
DK/CV, World and Manager occurrences. Every TH field and TH's independent
World occurrence survives, even when TH.g equals N's former World pointer.
The policy remains selected through BUSY failures and legacy detach or
owner-retire retries. A legacy detach already entered cannot upgrade to this
policy.

The qualified device control uses two N owners of one genuine App. A real
captured callback failure and foreign WorldAccess membership force separate
cleanup refusals before final N retirement. TH.g, the other N and the READY
GAME World/provider/cache/Store remain live. This control does not establish
a two-App graph or authorize World destruction. The Load/Screen/menu prefix,
qualified checks and remaining external-root and dispatcher work are described
in [terminal retirement](terminal-retirement.md).

## Chunk-update counter

The shared `dk` class initializes its actual `D` from the same `nw` singleton,
then stores zero in `b`. Get, reset and wrapping increment use that field. The
[menu runtime](menu-runtime.md) reads and resets it for the FPS String. DK
construction and bounding-box replacement leave it unchanged; mesh compilation
has not run merely because the grid exists.

## Qualification and limits

The selected Windows native controls pass World-attachment CPU coverage in
0.11 s and the device profile in 1.60 s. The device profile has 23,403 checks
covering the source grid, optional query assignment, repeat/null attachment behavior,
failure prefixes and cleanup. H0 uses 3,954,984 bytes in the actual N arena.
Separate source-list device controls pass in about 0.54 s: all 5,408 bounding-box
definitions, a rebuild using the same IDs and stable accounting, empty
replacement, real framebuffer results, sticky errors and quota failure with
the old definition retained.

Earlier constructor-only evidence includes a 16,723-check device profile after
genuine startup, independent namespace allocation and failed-owner cleanup.
Earlier selected Clang sanitizers covered the CPU constructor and affected
owners, excluding that device profile and default-directory startup. These
results retain their original scope. The complete local Windows selection now
passes 482 cases. The updated strict syntax checks cover both Linux compilers;
full Linux graphical play remains unqualified. See [the earlier qualification](../testing.md#startup-scene-construction-and-typed-settings-save).

These controls do not establish original pixels, star/trigonometric/driver
correspondence, query execution, terrain rendering, HUD/hand drawing or a
playable frame. The finite inspection renderer is separate from this owner.
