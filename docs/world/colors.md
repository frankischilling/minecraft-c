# World sky, fog and cloud colors

The client World exposes the original `fd.a(Entity,F)`, `fd.d(F)` and `fd.c(F)`
color methods on its canonical Dimension, WorldInfo, weather fields and shared
Entity-class Vec3 pool. Calls retain the reached owners and return one native
vector occurrence. They do not implicitly reset the source pool.

Sky color reads the actual Dimension manager again at each source access,
samples raw temperature, selects the actual Biome and dispatches its sky
method. Ordinary `kd.a(F)` preserves the JDK Color HSB arithmetic, narrowing,
clamps and temporary value allocation. Sky Biome `ry` returns its constant
before FP or Color work. Nether/Sky managers use the actual fixed-temperature
override without mutating their `a` arrays.

Raw `xv.b(II)` uses promoted `.025f` scales and amplitude `.5`, preserving or
replacing the actual manager.a buffer. It does not reuse the processed climate
formula. Overworld fog uses source time and the real Dimension method;
Nether/Sky fog dispatches the original constants. Cloud color uses World.H.
Rain, thunder, lightning and celestial factors follow the original float/double
operation order. Math ownership survives effectful Biome callbacks.

The [leaf fixture](../../tests/reference/world/sky_leaves/README.md) compares 386
original/JDK rows: 60 raw-temperature, 299 Biome, 25 HSB and two fixed-manager
observations. The [World fixture](../../tests/reference/world/colors/README.md)
compares 432 exact returned-vector/pool rows across all three Dimensions,
weather/lightning histories, extreme times and camera coordinates. Original
manager/Dimension construction runs; World/camera fields use prepared original
receivers. These finite bodies do not qualify complete World construction,
weather ticks or frames. Both public native replays require normal completion
and additionally check failure ordering, ownership and unchanged source state.

The [source renderer](../client/renderer-frames.md) consumes these methods in
its complete `px.h(F)` clear-color body and actual Sky draw. Further
[World/provider sky leaves](sky.md) supply celestial, rain, star and sunrise
values. Terrain compilation and drawing remain unfinished.
