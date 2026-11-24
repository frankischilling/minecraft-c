# Whole source ambient-occlusion rendering

The observer invokes unchanged `cv.a(Block,III,FFF)Z` on a genuinely constructed
CV and EW over a real World/provider/four saved Chunks. It observes all27 live
light scratch fields,12 AO flags,12 corner RGB fields, the final `m` flag,
the shared Chunk light marker, NW counters/color flags and every newly appended
32-byte source vertex word. One genuine NW batch remains open across the
bounded calls. The fixture does not invoke draw, GL, or a completed terrain
frame. No game method, Block, World, Chunk, cache query or returned vertex is
replaced.

The48 profiles cover `O=-1/0/1/2`, all-air/solid/mixed neighbor flags, actual
stone and grass, render-all and actual side culling, texture override17,
shared CV.a enabled/disabled and horizontal grass overlays. Four profiles
invoke the complete `CV.b -> l -> Minecraft.v -> AO` entry with the actual
published Minecraft static App/Settings, AO enabled, actual PX.a anaglyph
false/true and `O=0/1`. Their canonical IA array is explicitly filled with
constant0x003569a7 through its actual setter, making color/anaglyph differences
visible. This is authored color-array data, not an imported texture.

Two actual failures retain `m=true`: a null xp at the first brightness query,
and a genuine one-Chunk EW boundary at x0 whose center brightness stores n63
before the west query fails at AO78. Neither failure emits later vertices or
performs the normal m=false5177 store. Ordinary fully culled calls return false
and clear m normally. The native helper result remains unpublished on failure.

Every prepared input is declared: block/light/metadata bytes, World.f=3,
old light/color/flag fields, O, render-all, override, class flags and the
Object-only App/Settings operands for full entry. CV and EW constructors,
World/provider/Chunk owners, Block virtuals, IA setter, NW batch/quad expansion
and all invoked game methods remain original. The native fixture uses the
same canonical owners and real application namespace publication/retirement.

`profiles.txt` columns are Block ID, O, neighbor pattern, render-all, override,
CV.a, and entry mode:0 direct,1 null xp,2 full entry/anaglyph off,3 full entry/
anaglyph on,4 direct edge-cache failure. Every profile uses (8,64,8), except
mode4 at (0,64,8). A row begins with the tag/index/profile, result (-1 on throw),
exception and m; then27 float words, the12-bit flag mask,12 float RGB words,
cumulative NW h/o/p, packed k, NW l/m/n mask, shared Chunk light flag, appended
word count and all appended words. Float/word comparisons are exact for this
finite profile, including UV positions, source color packing and quad copies.

The bounded capture pins the original JAR and selected method Code identities,
matching JDK17 tools and existing SDK metadata. It denies network/native links,
confines writes to the disposable authored World, fingerprints inputs/helpers
before and after and requires saved input files unchanged except session.lock.
No original classes, game code bodies, images or saved game assets are shipped.

```text
python -B tests/reference/client/render_blocks_ao/capture_reference.py --client-jar /private/b1.7.3.jar --library-root /private/libraries --output /private/new-ao-original
python -B tests/reference/client/render_blocks_ao/run_native.py --executable /private/mc_test_render_blocks_ao --work-dir /private/new-ao-native
```
