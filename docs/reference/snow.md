# Snow-layer source map

The [verified binaries](binaries.md) define client `jr` and server `gj`, extending
their respective Block classes. Both sides have the same support, neighbor,
private survival and tick control flow. Their World getters have different
obfuscated names. The [native contract](../world/snow.md) keeps those distinct
source operations in order.

| Operation | Client | Server | Source sites |
| --- | --- | --- | --- |
| Support | `jr.a(Lfd;III)Z` | `gj.a(Ldj;III)Z` | Whole body 0 through 44; below ID7, array17/index22, virtual opacity23, fresh material38, Material.c41 |
| Neighbor | `jr.b(Lfd;IIII)V` | `gj.b(Ldj;IIII)V` | Whole body 0 through 10; private call6, discard9 |
| Private survival | `jr.h(Lfd;III)Z` | `gj.g(Ldj;III)Z` | Whole body 0 through 42; support6, metadata23, final drop26, notifying clear35 |
| Quantity | `jr.a(Ljava/util/Random;)I` | `gj.a(Ljava/util/Random;)I` | Whole body 0 through 1; returns zero without using Random |
| Light tick | `jr.a(Lfd;IIILjava/util/Random;)V` | `gj.a(Ldj;IIILjava/util/Random;)V` | Whole body 0 through 43; Block-light static1, saved light8, metadata27, final drop30, notifying clear39 |

The support method subtracts one with Java int wrapping before ID query7.
An air ID returns false. Nonair reaches the current Block array and invokes
`uu.c()` or `na.a()`, including concrete overrides and live receiver fields.
Only true reaches a distinct World material getter and `ln.c()` or `hj.c()`.
The cached Block opacity array and `fd.h/dj.e(III)Z` solid support have different
call and field orders.

Private survival skips removal when support is true. Otherwise current metadata
precedes the inherited final `g(World,III,metadata)` call. Its virtual probability
body reads World.B; the local side then reaches quantity zero. Neither side
reaches probability draws, drop-ID statics, Stack construction or publication.
The following notifying setter executes even though quantity is zero, and its
ordinary result is discarded.

The light tick reads `eb.b` or `co.b` before invoking saved light. A result at
most eleven returns. Greater values reach metadata, the same inherited drop
chain, and notifying removal. There is no remote guard around the whole tick,
no survival call and no use of the supplied Random.

The class also declares player harvesting, item ID, collision bounds, shape
and, on the client, a face predicate. The harvesting overload directly spawns
a snowball and updates statistics. These are separate source bodies; their
presence does not establish a native implementation or observation. The
support/neighbor/tick work includes no fresh original experiment or native
replay and does not qualify original JVM failure timing or arbitrary native
recursion resources.
