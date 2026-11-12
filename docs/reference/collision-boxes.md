# World and Block collision source map

The verified original client and dedicated-server JARs remain the authority.
Their SHA-256 values and research setup are recorded in the [reference
guide](binaries.md). Mapped names serve as search aliases. The native
[collision module](../world/collision/boxes.md) retains each source identity
and reports missing descendants explicitly.

## World and Entity bodies

| Behavior | Client | Server | Source order |
| --- | --- | --- | --- |
| World collision collection | `fd.a(sn,eq)List` | `dj.a(lq,cz)List` | Clear K4; six floor limits; X/Z/Y traversal; Block array capture113; acquiring ID getter123; captured-array index126; contribution148; pooled expansion183; nearby186; return current K299 |
| Column presence | `fd.i(III)Z` | `dj.g(III)Z` | Y guard; coordinate shifts; private presence20; return23 |
| Candidate loop | Same World body | Same World body | Captured R size198; first get210; candidate virtual218; optional intersects231/add243; second get252; moving pair virtual260; optional intersects273/add285 |
| Base candidate box | `sn.f()eq` | `lq.e_()cz` | Literal null, return1 |
| Base pair box | `sn.a(sn)eq` | `lq.a_(lq)cz` | Literal null, return1; no argument read |
| Boat candidate / pair | `fz.f()` / `fz.a(sn)` | `eb.e_()` / `eb.a_(lq)` | Own permanent bounds / argument's permanent bounds |
| Minecart candidate / pair | `yl.f()` / `yl.a(sn)` | `pr.e_()` / `pr.a_(lq)` | Literal null / argument's permanent bounds |

Complete hierarchy inspection finds only Boat and Minecart overriding these
two virtuals. Item `hl/ez` and FallingSand `ju/gl` directly inherit both base
bodies. This inventory does not implement the other Entity constructors.
The World loop's second get precedes even a null moving-receiver failure.
Pooled expansion can reuse and overwrite the original query's own identity;
later descendants must read its actual current contents.

## Arrow inherited null bodies

The client archive SHA-256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Complete `sl.class` is 6,123 bytes with SHA-256
`2ba2e27fbc558a78b87c26316968cd5e9595656b96c861b2975cae79201d3a45`.
Its direct superclass is `sn`; neither relevant descriptor appears among SL's
declared methods. Both inherited SN methods return literal null without reading
the argument, bounds, World, shooter, dead flag or Random state.

| Complete client method | Code bytes | Code-attribute SHA-256 |
| --- | --- | --- |
| `SN.f()Leq;` | 2 | `6a01c4869864d7ee9b83f29dc72397f8372ad752b2f0a76408dae836ddeb8b2d` |
| `SN.a(Lsn;)Leq;` | 2 | `2d3a3cdfdd67ffb7a37cfb1bacf4f5cd24433dee908e1bd1a8c6afa0ce2dc1e4` |

Both Code arrays have SHA-256
`146cb1bd5c639e50589db67598e4a3f423eb736f7911d3b8329f72e0f5cd5fac`
and empty exception tables. The complete client World collector has 300 Code
bytes and SHA-256
`0689b878be3ff7bf85f91188794d02e411ce79da3fd4a8b6b9f09fce96d0c1d1`.
Its candidate call at 218 precedes a separate List get at 252 and moving call
at 260. A null first result skips only its own intersection/add arm.

Minecart remains distinct: `YL.f()` returns null, but its complete five-byte
`YL.a(SN)` reads the argument's permanent bounds. Its Code SHA-256 is
`6b1b29e24a992487bef009bb2f88f061c3a9569e8422e45a1cc21d911af0c4d0`.
Admitting Arrow's inherited pair body does not admit Minecart's pair body or
server Arrow construction. These are whole-method static identities; no new
original execution or full-query replay is claimed.

## Block getters and contributions

All 96 registrations and 69 concrete kinds were checked through their actual
inheritance chains. The selected audit completely reads 61 paired bodies:
25 collision-getter declarations, four contribution declarations, 16 separate
state-shape declarations and their reached helpers. Client/server instruction
order agrees after the actual symbol and member-name translations.

Base `uu/na.e(World,III)` loads current `bs,bt,bu,bv,bw,bx`, adds the respective
integer coordinates as doubles, and acquires its pooled box at44. Base
contribution invokes the actual virtual getter6, skips null, performs
`query.intersects(box)`20, then adds that same identity to the passed ArrayList30.
The separate state-shape setter is not an automatic prerequisite.

Thirteen getter declarations genuinely return literal null: portal, pressure
plate, button, rail, reeds, web, fluid, redstone, sign, torch, plant, lever and fire
families. Snow and moving piston have conditional null routes with preceding
dependencies. All other routes use their actual inherited or concrete getter;
opacity and Material are not substitutes for collision dispatch.

Ladders mutate the singleton shape using exact metadata values. Doors and
trapdoors invoke their actual state-shape descendants before the base getter.
Snow uses metadata's low three bits and its distinct fixed half-height
collision threshold. Fence, soulsand, cactus, cake and farmland directly
construct their evidenced boxes, including float narrowing and wrapped
integer additions. Slabs and beds use their actual current singleton fields.

Stairs `ss/lu` and piston heads `h/g` append two independent part boxes in
source order. Their final full-bounds resets are normal completion, not
finally blocks. Piston bases `jq/gi` first force full bounds and then invoke
the base contribution; they do not run their extended-state shape setter.
Asymmetric piston-head rod coordinates are preserved.

Moving `ut/mz.e` invokes the real typed TileEntity lookup before testing for absence
or the wrong concrete TileEntity type. Positive geometry requires real `uk/mu`
progress, direction, stored Block and the live `wj/od` Direction arrays. The
separate added callback `ut/mz.c(World,III)` is literal return0. No native
positive geometry follows from that callback alone. The separate
[Piston closure](piston.md) supplies genuine TileEntity construction, interpolation,
Direction initialization and held carried Block-box translation. Active mechanics
remain distinct unfinished descendants.

The [original capture tool](../../tests/tools/world/collision/capture_block_shapes.py)
uses untouched original classes, real registry bootstrap, actual virtual
getters/contributions and explicit reflection-driven setter controls. Its
null-World scenarios establish which descendants leave World unused and which
fail at that dependency. They do not prove nonnull-World metadata or complete
World collision behavior. Direct moving-geometry helper observations are
separate from positive piston-TileEntity execution.
