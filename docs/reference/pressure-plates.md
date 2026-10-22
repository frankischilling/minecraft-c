# Pressure-plate source map

The [audited client and server binaries](binaries.md) define pressure-plate
owners `bv` and `bc`. Registered wood ID72 uses category `rt.a` / `kx.a`;
stone ID70 uses category b. Category c selects Players. The native modules are
[private update/tick](../../src/world/runtime/mutation/pressure_plate.c),
[lifecycle](../../src/world/runtime/mutation/pressure_lifecycle.c) and
[contact dispatch](../../src/world/runtime/contact.c). Their
[ownership contract](../world/pressure-plates.md) distinguishes implemented
untyped traversal from the still missing Class-filtered queries.

| Body | Client | Server | Reached source sites |
| --- | --- | --- | --- |
| Contact | `bv.a(Lfd;IIILsn;)V` | `bc.a(Ldj;IIILlq;)V` | Remote read1/return7; metadata13; metadata==1 return20; private update27; return30 |
| Tick | `bv.a(Lfd;IIILjava/util/Random;)V` | `bc.a(Ldj;IIILjava/util/Random;)V` | Remote read1/return7; metadata13; zero return19; private update26; return29 |
| Private update | `bv.h(Lfd;III)V` | `bc.g(Ldj;III)V` | Metadata5; receiver/static reads30/33,88/91,147/150; reference comparisons36/94/153; pooled boxes79/138/197; queries82/141/200; List.size207; return411 |
| Added | `bv.c(Lfd;III)V` | `bc.c(Ldj;III)V` | Literal return0 |
| Neighbor | `bv.b(Lfd;IIII)V` | `bc.b(Ldj;IIII)V` | Solid support below10; unsupported metadata35/drop38/notifying air47; return51 |
| Removed | `bv.b(Lfd;III)V` | `bc.b(Ldj;III)V` | Metadata5; positive value notifies current24/below38; inherited removal47; return50 |
| Delay | `bv.e()I` | `bc.c()I` | Literal twenty at0; return2 |

The contact Entity and tick Random arguments are unused. Update treats prior
metadata as on only when equal to one. Removal uses the distinct greater-than-zero
predicate. Each category getstatic performs actual class active use after the
receiver category read. Query boxes preserve Java wrapping and float arithmetic
before widening, followed by the actual pooled factory. Category a invokes
`World.b(null,AABB)`; categories b and c invoke separate Class-filtered overloads.

| Transition | Ordered invokes |
| --- | --- |
| Found, previously off | Notifying metadata234; current neighbors246; below260; range272; positional sound301 |
| Empty, previously on | Notifying metadata320; current neighbors332; below346; range358; positional sound387 |
| Found | Live receiver ID field401; virtual delay405; World schedule408 |

The nonloading scheduler presence gate, synchronous neighbor reentry, observer
and sound descendants remain actual lower operations. No callback suppression,
invented empty Class result or replacement deferred schedule is equivalent.

## Reproduction and retained source spans

Run `javap -classpath <side.jar> -p -c -s bv` for the client and the corresponding
`bc` command for the server, using the binary hashes in the audit. The retained
whole-class dumps include all classes: client 9,770,684 bytes, SHA-256
`15acc525e12512ba96782b57ead3940bf4cc608ad3dae3b9fca4e5acf96189bd`;
server 4,826,184 bytes, SHA-256
`020035f5590871cb3851e460f60013156fd6cb69c0e46bbee9454ebd33851f7e`.
The following spans use one-based LF lines and retain the original CR bytes.
They identify complete paired bodies rather than decompiler line numbers.

| Span | Client lines / SHA-256 | Server lines / SHA-256 |
| --- | --- | --- |
| Tick, contact and update, 8,736 bytes each | 12972–13273 / `d8b743f563097c51f91aa186150deeec422b8f1096dd5acada6d44f39594fd50` | 6027–6328 / `25970c95c4b995adb1c6c7e2cffb792fa746c3f6b6b566d08e844af29817454b` |
| Support, added and neighbor, 1,359 bytes each | 12918–12971 / `56a16b479ac1eb16a53cee181e8d98540633c4e6ed1dca580b9d2514ff28eb4b` | 5973–6026 / `27fca162fdb94d6d4a91a0648abd9f094a91eb9745dee785d9007ee69d5bd9f8` |
| Removed, 999 bytes each | 13274–13308 / `3f1e24473f2383b961f913673bef11c1e4e10ee63b3ee695fce14d8efac12018` | 6329–6363 / `f2d8b6e7ea009347fc457f48ba180aa2297e4e7af67dd9812147d0915b5af651` |

This is paired bytecode authority and a native implementation contract. It is
not an executed-original pressure comparison. Broader Class-query traversal,
detector rails, redstone, tick draining and gameplay acceptance remain separate.
