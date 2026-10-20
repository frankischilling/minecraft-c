# Original plant predicate mapping

These mappings come from complete method bodies and relevant class member
inventories in the unchanged client/server archives. Their SHA-256 values are
client `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`
and server `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.

All World predicate descriptors are client `(Lfd;III)Z` and server `(Ldj;III)Z`.
Substrate predicates use `(I)Z`. A complete declaring body and proved inheritance,
rather than a similar name or block ID, determines each virtual method.

| Body | Client | Server | Relevant operations |
| --- | --- | --- | --- |
| Base can-place | `uu.a` | `na.a` | World block5; air shortcut; Block.m15, element20, bA21, Material.g24 |
| Base survival | `uu.g` | `na.f` | Literaltrue0/1; pumpkin inherits this exact body |
| Flower can-place | `wb.a` | `nw.a` | Base invokespecial6; below block20; virtual substrate23 |
| Flower survival | `wb.g` | `nw.f` | Plant light5; only below8, sky18; below block32; virtual substrate35 |
| Flower substrate | `wb.d` | `nw.c` | Live grass v1/bn4; conditional dirt w11/bn14; conditional farmland aB21/bn24 |
| Dead-bush substrate | `jb.d` | `fz.c` | Live sand F1/bn4 |
| Mushroom survival | `tl.g` | `md.f` | Y check; light18 below13; below block34; virtual substrate37 |
| Mushroom substrate | `tl.d` | `md.c` | Capture actual Block.o0; reached signed element4 |
| Reed can-place | `ri.a` | `ko.a` | Below block7, own bn15; live grass25/28 or dirt36/39; water material checks56/59,76/79,96/99,116/119 |
| Reed survival | `ri.g` | `ko.f` | Invoke actual can-place6 |
| Cactus can-place | `or.a` | `je.a` | Base invokespecial6; survival20 |
| Cactus survival | `or.g` | `je.f` | Neighbor material7/25/43/61 then predicate10/28/46/64; below79; live cactus86/89 or sand97/100 |
| Pumpkin can-place | `fc.a` | `di.a` | Base-like target prefix; separate actual solid-support call37 |

Tall grass `ru`/`kz` inherits both flower predicates and substrate. Dead bush
overrides substrate but inherits flower placement/survival. Mushroom overrides
survival and substrate, while its can-place remains the flower method. Pumpkin
has its own can-place and no survival override. No other Block kinds receive a
guessed base implementation from these findings.

The material query itself is `fd.f(III)Lln;` / `dj.d(III)Lhj;`: World block4,
air Material14, Block.m18, element23 and bA24. Its nullable material return is
distinct from a NULL Block receiver. The native detailed getter and legacy
getter share this body and preserve separate error attribution.

These are source findings and implementation contracts. They are not new
executed original plant-predicate observations or complete population evidence.
See the [runtime scope](../world/plants.md).
