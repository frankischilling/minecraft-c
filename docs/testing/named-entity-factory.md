# Named Entity factory and World setter

The [reference packet](../../tests/reference/entity/named_factory/README.md)
records two fresh original-client JVM runs, `-Xint` and default. Each made ten
direct named-factory calls and one original World-setter call. It records the
component used at AG48; AG drawing and CY ticking were not entered. No native
parity or JDI execution trace is claimed.

| Original operation | Descriptor | Static source sites |
|---|---|---|
| JC named factory | `(Ljava/lang/String;Lfd;)Lsn;` | Map.get6, public getConstructor29, newInstance40, Entity cast43, printStackTrace52, return56 |
| SN World setter | `(Lfd;)V` | World reference store2, return5 |

The factory's protected range is [2,47), with an Exception handler at50. It looks
up the actual Class by String value and invokes its public World constructor.
Unknown names return NULL without the NBT overload's skipping diagnostic. The
handler prints an actual caught exception and returns the nullable result.
Class-initialization Errors and failures in the handler must not be flattened
into ordinary unknown-name returns. Class/Code hashes pin this static reading;
they do not establish entered instruction PCs.

Both runs observed the same finite results:

| Inputs | Observed result |
|---|---|
| Unknown, NULL, empty, lowercase `pig` | NULL, without an unknown-name print |
| Abstract registered `Mob`, with NULL and actual World | NULL after genuine `InstantiationException` stack traces |
| Pig cases5/6/7 | Separate WH objects, IDs0/1/2, +1/+1 deltas; Worlds NULL/NULL/actual FD |
| Original setter on E0 | Same receiver, NULL World becomes actual FD; other selected fields unchanged |

Pig defaults included width/height0.9, health10, `/mob/pig.png`, zero selected
position/offset and Byte watcher entries0/16 both0. An equal-valued distinct
String still selected Pig. Four actual registry maps retained the same24
registrations after the first and last calls. These snapshots do not specify
HashMap iteration order.

The actual World was loaded from a copied earlier Furnace SDK save through the
original storage/provider/cache owners. Recorded time211, block62/metadata2,
saved SK and empty Entity/Player lists remained unchanged at the sampled call
boundaries. All source save bytes stayed exact; each owned copy changed only
its eight-byte session lock. No original tick/save or Player construction was
performed.

All five predeclared constructor entropy fields remain in the finite JSON.
Only those fields are excluded from cross-VM equality. IDs, World aliases,
watchers and every other selected field are compared exactly. The executed
Java and declaration are published unchanged; portable packaging helpers are
explicitly later additions. Raw streams and binary/save material remain private,
with hashes and complete transport receipts checked by the publication guard.

Native consumers can compare the three Pig constructor projections and the
World-reference transition in `native-observations.h`. The named factory needs
its defining Registry context independently of a nullable World. A native
unsupported constructor must remain a checked frontier, rather than imitate an
unknown-name NULL. In particular, the original abstract-Mob outcome includes
printStackTrace; a native print52 frontier does not qualify all ten calls.

The next caller steps remain separate. AG owns a name-to-preview cache, retries
a NULL cached value, rebinds a nonnull preview through SN.a(FD), and later draws
it through TH's coordinates overload. CY performs Living casts226/229 after
factory223, outside JC's catch. This packet does not qualify those cache, GL,
cast or spawn continuations.
