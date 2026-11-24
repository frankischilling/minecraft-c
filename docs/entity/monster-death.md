# Monster death and drops

Spider, Zombie, Skeleton and Creeper now complete fatal damage through the
original Living death body. A nonnull attacker receives score and kill callbacks
before the dying flag is stored. The current World then decides whether ordinary
drops run. A second World read reaches the client's literal-return status method.
The Monster retaliation wrapper runs only after the complete damage call returns.

Zombie drops Feathers, Spider drops String and Creeper drops Gunpowder. Their
inherited quantity is `Random.nextInt(3)`. Skeleton overrides the drop method:
it draws an Arrow quantity, publishes those Items, then reloads its Random and
draws a separate Bone quantity. Each loop reads its actual Item static for every
drop. The second draw cannot move before the first loop's callbacks.

Creeper first completes the inherited death body. If the original attacker is a
Skeleton, it then reads the first record ID and adds a fresh `nextInt(2)` result
with Java integer wrapping. This record drop also runs when the parent's remote
World gate skipped ordinary drops. An Arrow is not implicitly replaced by its
shooter at this call site.

Player kills reach Kill Enemy through the existing achievement dispatcher.
Build Sword must already be present. A missing prerequisite skips the write and
notice; an existing zero-valued prerequisite still counts as present. Score,
Mob Kills and Kill Enemy remain separate effects with their original ordering.

## Source and lifetime

The unchanged client archive has SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [optional collector](../../tests/reference/entity/monster_death/README.md)
pins the reached method Code attributes and concrete class inheritance.
Relevant client declarations are `ls.b(Lsn;)V`, `ls.q()V`, `uz.j()I`, `cn.j()I`,
`fr.q()V`, `gb.b(Lsn;)V` and `gs.a(Lls;)V`.

Skeleton's quantity calls are at bytecode 5 and 39, static reads at 17 and 51,
and Item drops at 24 and 58. Creeper's record static read is at 13, Random call
at 24 and drop at 29. Player's achievement static read is at 8, followed by the
statistic wrapper at 11. These are bytecode locations, not decompiler line numbers.

Death work owns the receiver, reached attacker, Registry and both captured World
occurrences until checked cleanup. A failed publication preserves the same pending
drop, its failure receipt and already published Items. Cleanup releases native
ownership without retrying source work or removing earlier drops. Four passive
Item records accommodate both maximum Skeleton loops without overwriting the
first loop when the second starts. No per-frame allocation policy changes here.

## Verification and limits

`entity.monster_death` checks twenty deterministic return/state/Item records and
authored callback failures. The failure cases cover every Skeleton publication,
a World changed by publication, a Random cleared during the first Skeleton loop,
late Registry admission and Creeper's record Random reload. Active cleanup refuses
reentry; later cleanup preserves published Items and Random progress.

`entity.player_monster_kill` checks all four Monster classes, missing/present
achievement prerequisites, repeat increments, notice dispatch, null App/writer
failures, early returns and two complete fatal Player attacks with real World,
Chunk, statistics and Item owners.

The optional original comparison covers 22 calls: twelve direct deaths, four
fatal Cow attacks, four Creeper/Skeleton deaths across local and remote Worlds,
and two fatal Player attacks. Health, return values, dying/removed flags, final
private Random state, sound names, ordered Item IDs/counts/metadata/pickup delay,
World/Chunk membership and selected Player statistics compare exactly.

Constructor entropy and Item random motion remain independent. These finite
method calls do not establish complete combat, delayed removal, save/reload,
original audiovisual equality, frame times or an FPS improvement. Full Arrow
victim damage and pickup/rider behavior still need work.
