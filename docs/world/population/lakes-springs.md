# Lakes and springs

[`population/lake_spring.h`](../../../include/mc/world/runtime/population/lake_spring.h)
provides two retained target fields, their constructors, inherited empty scale
methods and complete placement bodies. Both operate on actual GAME World,
Registry, Chunk and Random owners. The normal-provider controller is separate
and remains unfinished.

A lake descends through actual air queries, allocates its temporary zeroed
2,048-cell mask and then consumes the ellipsoid-count draw. The first floating
admission occurs at the following double draw. Complete boundary validation
precedes every write. Successful validation permits quiet filling, saved-sky
dependent grass conversion and, for the actual lava material, stone-rim work.
The temporary mask is released on every return. Validation rejection is ordinary
false; native failure preserves prior effects and leaves the result untouched.

A spring queries the actual surrounding stone and air cells. When its condition
matches, the notifying setter runs before World.a becomes true. The feature then
captures Block.m, reloads its target and dispatches that actual Block tick with
the supplied Random. Only normal tick return reaches the final false assignment.
A late failure can therefore retain true, together with earlier synchronous
fluid, queue and lighting effects. A condition miss returns true without
executing this flag sequence.

Nested scheduling uses current World.a and World.r independently of the spring's
supplied Random. A request constructs its scheduled record before any presence
or current-ID gate. In the tested sealed two-cell fluid cavity, recursive added
ticks settle both cells before a later explicit flowing-ID request. That request
finds a still ID, skips its tick and leaves World.r unchanged. All four actual
record constructions still consume sequence values. A separate prepared
three-cell lava cavity reaches a real fifth request and a late null-World.r
failure; both a/o flags and the complete partial cell state remain observable.

Contracts inspect all block and packed-array bytes, actual lighting records,
complete E/F identities and fields, Random state/cache, flags, class state and
balanced lifetimes. Present-radius cases use two real cache publications that
remain in provider history. The later capacity test fills the owner's actual
limit. No queue, sequence, source counter or owner ledger is reset.

Whole water-lake continuation and the concrete spring paths have independent
source-derived expectations. The lake voxel mask and high lava-rim draw count
are not independently reconstructed. Mask-allocation failure, arbitrary
WorldAccess receivers and unsupported descendant ticks remain unqualified. These finite
native contracts do not establish original/native equivalence, natural
population reachability, complete scheduling or save interoperability.

See the [paired source map](../../reference/population-geology.md),
[fluid bodies](../fluids.md) and [scheduler](../scheduling.md).
