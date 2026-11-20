# Redstone particles

Client Redstone particles use the original `im` Entity, its `xw`/`sn` ancestry
and DN layer 0. The `reddust` World notification now allocates and constructs
that concrete owner, then publishes it to the actual effect manager. RGB
arguments narrow from double to float before construction. In particular, a
tiny double red value can become zero and trigger the constructor's red=1
fallback; overflow remains infinity. No color clamp is added.

The default constructor delegates to scale 1. The scaled constructor consumes
the inherited five shared Math doubles and four private Random floats, then
five further shared doubles for color and lifetime. Each operation preserves
its original float or double width. Both signed red zeros become one. Scale
changes size and lifetime, including zero and negative lifetimes. Constructor
observations retain all ten Math samples without supplying game entropy.

Ticking stores previous coordinates, postincrements wrapping age and marks an
expired particle dead before continuing. Texture animation uses Java integer
division. Zero lifetime throws after those earlier stores. The genuine Entity
mover runs with the current velocity; the override adds no gravity. Unchanged
Y boosts horizontal velocity before ordinary and grounded damping.

Drawing grows size from the saved constructor size using
`clamp((age+partial)/lifetime*32, 0, 1)`, preserving NaN. It then enters the
inherited XW billboard. The complete manager dispatch uses the same Entity
identity for publication, ticking, drawing and removal. Native holds and
retirement preserve source fields and retained failure prefixes.

## Evidence

The [original packet](../../tests/reference/entity/redstone_particles/README.md)
contains two fresh ordinary VMs, each with 37 genuine constructors and 30
component calls: 12 ticks, 12 draw prefixes and six inherited brightness calls.
All 30 component rows agree exactly. Every constructor's own entropy is audited;
private initial seeds are inferred by reversing the pinned draw count, rather
than directly observed. No seed, clock or entropy sample is supplied.

Native replay matches all 30 rows, including age wrap, zero-lifetime failure,
NaN partial ticks, ground damping, packed sprite words and 25,165,440 unchanged
inactive Tessellator words. Its 38 constructors have a separate native entropy
audit. The World callback regression checks four distinct READY `im` owners,
DN layer membership, RGB narrowing, superclass identity and retained NULL-name
failure ownership. These CPU tests require no original archive, assets or GL.

The original tick inputs explicitly enable no-clip movement. Ordinary collision
parity, original whole-frame pixels, default entropy distribution, audio and FPS
remain outside this finite comparison. These particles remain client-only.

```sh
ctest --test-dir build --output-on-failure --no-tests=error -R '^(entity[.]redstone_particle_reference|client[.]redstone_particle_factory|tools[.]redstone_particles_packet)$'
```
