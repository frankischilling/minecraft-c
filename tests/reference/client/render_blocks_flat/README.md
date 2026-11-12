# Flat standard block geometry

`finite.json` records 25 whole calls to unchanged `cv.b(uu,III,FFF)Z` from
the pinned original client. The helper constructs a genuine saved World, four
Chunks, the real inclusive EW cache and one CV per profile. It starts the
canonical NW quad batch once and observes the actual appended integer buffer.
No game method is replaced, instrumented or subclassed. There is no Display,
native-library load, network access or NW draw in this bounded CPU profile.

The first calls exercise real class initialization. Prepared inputs then cover
stone and the actual grass object, visible and fully occluded faces, mixed
neighbor blocks and lighting, partial and out-of-range bounds, glass and fluid
Materials, all four face rotations, horizontal flipping, override textures,
grass overlays, a nongrass Block whose texture is 3, disabled overlays, zero
RGB and RGB outside 0..1. The cache and source World hold the same actual Chunk
identities throughout. Each profile restores its deliberately prepared shared
Block fields after observing the source call.

Three rows fail at source null dereferences: null CV access, a null Block
receiver, and a null Material reached at the partial top after a completed
bottom face. Another row has the same null Material with full-height bounds
and returns normally. Failure is recorded separately from ordinary `false`.
Both ordinary fully occluded calls return `false` without emitting vertices.

Each row contains `FLAT`, its index, eleven profile integers, the return value
(`-1` for an exception), exception category, CV.m, all 27 brightness scratch
floats as raw hexadecimal bits, twelve boolean flags packed into one integer,
all twelve corner-color floats, six live Block bound doubles, NW.h/o/p/k,
the NW.l/m/n mask, the actual Chunk class light flag, the number of appended
words, and those exact words. The helper also checks the remaining CV scalar
fields, unchanged bounds, open quad batch and retained translation. A source
face makes four genuine NW vertex calls; NW's original quad expansion stores
six vertices, or 48 words, per face.

The eleven profile columns are Block ID, neighboring-block pattern
(0 air, 1 solid, 2 alternating), render-all, override texture, CV static overlay
flag, null mode (0 none, 1 access, 2 Block, 3 Material), bound profile, prepared
Block base texture (`-1` preserves it), rotation, horizontal flip and RGB
profile. Bounds are full cube, partial, out-of-range and a half-height top;
RGB profiles are `(0.3,0.6,0.9)`, `(-0.25,1.5,2)` and `(0,0,0)`.

`capture_reference.py` checks the original JAR, selected class/method identities,
four SDK metadata JARs, JDK17, helper inventory and all input hashes. The authored
World generator is shared with the picking fixture; no saved World or imported
assets are checked in. The receipt verifies unchanged World bytes except the
ordinary session lock.

`run_native.py` generates the same authored World and resource inputs, runs
`mc_test_render_blocks_flat`, and compares every row exactly. The C helper also
checks source frame PCs, retained failure prefixes, one cache World hold and
strict floating-point admission after the source m store and NW read. These
observations qualify the entered flat body and CPU vertex storage; they do not
claim a rendered terrain frame or the other specialized CV geometry families.

To capture privately:

```text
python -B tests/reference/client/render_blocks_flat/capture_reference.py --client-jar <original.jar> --library-root <SDK-libraries> --java <JDK17-java> --javac <JDK17-javac> --output <new-private-directory>
```

To replay the native component:

```text
python -B tests/reference/client/render_blocks_flat/run_native.py --executable <mc_test_render_blocks_flat> --work-dir <new-private-work-directory>
```
