# Original plant feature mapping

Authority is the unchanged client archive
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`
and server archive
`033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.
Complete paired classes include constructors, placement returns and inherited
base methods. Each placement is `a(Lfd;Ljava/util/Random;III)Z` on the client
and `a(Ldj;Ljava/util/Random;III)Z` on the server. PCs below agree across sides.

| Family | Client/server class | Reached placement operations |
| --- | --- | --- |
| Flowers/mushrooms | `be`/`am` | Air68; m74, feature a78, element81, cast82, survival92, late a106, quiet write109 |
| Tall grass/fern | `ap`/`ad` | Descent block9, leaves20/bn23; air109; m115, a119, element122, cast123, survival133, a147, metadata151, write154 |
| Dead bush | `kt`/`gw` | Same descent; air107; m113, a117, element120, cast121, survival131, a145, write148 |
| Reeds | `ir`/`fs` | Air52; four material/water identity pairs69/72,89/92,109/112,129/132; nested height142/147; survival176; write198 |
| Pumpkins | `wx`/`op` | Air68; below83, grass86/bn89; can-place105; late pumpkin118/bn121; orientation126; metadata write129 |
| Cacti | `fx`/`ea` | Air68; nested height78/83; survival112; write134 |

The parameterized constructors store `a` at6; tall grass also stores `b` at11.
The other three feature constructors retain no instance fields. Base `pg`/`jm`
constructors have no retained effects here, and their scale method is empty.

All source writes are quiet, and their ordinary Boolean results are discarded.
Reed and cactus height loops test survival at each layer and continue after
false; neither adds a per-layer air gate. Tall/dead descent queries before its
Y test, and a nonair result reads the actual leaves static before that test.
Outside World bounds therefore does not imply whole-feature early completion.

Flower-family casts admit the six actual descendants proved by the
[predicate member inventories](plant-predicates.md). Array active use precedes
the feature ID read; NULL, bounds, cast and invocation failures remain distinct.
The selected receiver and late write ID are separate source values.

See the [native ownership and validation limits](../world/population/plants.md).
This mapping is a source finding, not an executed original/native comparison.
