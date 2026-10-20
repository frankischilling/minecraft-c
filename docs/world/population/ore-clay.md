# Ore and clay population features

[`population/ore_clay.h`](../../../include/mc/world/runtime/population/ore_clay.h)
provides the ore and clay constructors, inherited empty scale methods and
complete placement bodies. Their retained signed target and size fields remain
separate from the shared Registry. Clay construction reads the current named
clay Block; ore construction stores its supplied target without validation.

Placement uses the actual World getter, initialized class trig table, supplied
Random and quiet setters. Clay first queries the starting material and compares
the actual shared water identity. Nonwater returns ordinary false before any
Random draw or floating-profile check. Ore begins with its first float draw.
Both retain the source endpoint, inclusive segment and candidate traversal
order. Each candidate query precedes the late replaceable Block read.

Ordinary false setters do not stop either feature. Native failures preserve the
ordinary result output and every earlier Random, class, block, metadata, hook
and lighting effect. Diagnostics retain the feature PC and fresh lower
Registry, material or mutation cause. Negative and zero sizes retain their
source arithmetic and draws; the interface adds no size or coordinate clamp.

The native contracts exercise every normal-provider target/size, whole-chunk
single-cell expectations derived independently of the feature geometry, bounded
volume properties, supplied/World Random aliasing, real Gaussian-cache priming,
late missing dependencies and a failure after an earlier completed write.
Source-derived invocation budgets check continuation without regenerating the
feature's voxel traversal.

Ordinary gravel placement still reaches an unsupported added hook when World.B
is false. A separate labeled World.B=true input bypasses that source hook and
tests the geometry path. Its successful result does not qualify ordinary gravel
population. Original/native output comparison, complete numeric edge coverage,
callback closure, full controller qualification and populated persistence remain
unfinished. The [installed controller](normal.md) now reaches the ordinary
gravel hook and retains its real partial-write failure prefix.

See the [paired source map](../../reference/population-geology.md) and
[test scope](../../testing.md).
