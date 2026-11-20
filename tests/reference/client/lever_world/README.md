# Lever World rendering

This packet records 32 complete calls to the unchanged Beta 1.7.3 client
`cv.b(Luu;III)Z` dispatcher and its type 12 child `cv.e(Luu;III)Z`.
Two fresh ordinary VMs produce identical rows for every metadata value 0
through 15, both with the default texture and with texture override 17.

Each call runs the original `xr` bound update, standard flat base rendering,
brightness and emission selection, eight actual pooled `bt` factories,
MathHelper rotations and six handle quads. The literal rows retain final
Block bounds, override/AO state, pool cursor advance and every appended
Tessellator word. The original default triangulation appends 576 words per
call: 48 source vertex calls become 72 encoded vertices.

The World, its four Chunks, ChunkCache, registered Lever and Tessellator are
genuine constructors. The application and settings are explicitly allocated
without constructors, as CPU input state for `Minecraft.x()`. Ambient
occlusion is false; the setting and static application reference are authored
before observation. This does not qualify Minecraft construction. The Block
cell is at (8,64,8), World skylight subtraction is 3, the local Chunk's Sky
light array is 15 and Block light array is zero. All base faces are requested.

`source-pins.json` identifies the verified archive and complete class hashes.
`provenance.json` freezes the executed helper, capture driver, source pins,
two original outputs, compiled helpers and JDK inputs. Private compiled
helpers, Worlds and raw logs stay under `.local`; they are not distributed.
The helper checks each loaded original class's archive origin and rejects
network, native-library and process activity. No OpenGL context is created.

To repeat the original comparison on Windows with the pinned local libraries:

```powershell
python -B tests/reference/client/lever_world/capture.py --output .local/lever-world-repeat
```

The output directory must not exist. This command requires the verified local
archive, the pinned Launcher libraries and a matching Java/Javac installation.
Missing prerequisites fail explicitly. They do not produce a parity pass.
Do not replace this packet merely because a later native replay differs.

The public `client.lever_world` test creates a new authored World, enters the
real C dispatcher and compares the same 32 complete rows without Java or
original assets. It separately checks native Cobblestone-null failure,
override bypass, retained base-child failure state and failure after three
pooled acquisitions. The allocation control retains the base quads and pool
cursor while retiring every acquired native hold. These extra controls
are native assertions, not original exception-comparison claims.
`tools.lever_world_packet` rejects damaged rows and modified captured helpers.

These are finite CPU geometry results. They do not establish ambient
occlusion, custom callbacks or class initialization schedules, arbitrary
coordinates, original pixels, loaded interaction, audio or FPS.
