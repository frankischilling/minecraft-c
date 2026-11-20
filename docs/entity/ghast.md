# Ghast construction

The client `bp` constructor creates a genuine canonical Ghast owner with the
same native Entity and Living storage used by other implemented Mobs. It calls
the actual `wq` parent, which directly invokes `ls`; it does not run the
Creature, Monster or Animal constructors.

The virtual watcher hook runs during Entity construction. `bp.b()V` invokes its
inherited hook, registers Byte0 at index 16 and returns at 17. Living construction
then supplies its ordinary health 10, Math draws and pose stores. The Ghast body
stores `a=0`, `g=null`, `h=0`, `e=0` and `f=0` at PCs 7/12/17/22/27. It assigns
`/mob/ghast.png` at 33, invokes size 4×4 at 41, stores `sn.bC=true` at 46 and returns
at 49. The three double destination fields retain their allocation zeros.

The [source audit](../../tests/reference/entity/player_ally_alert/ghast-construction-pins.json)
pins the original archive, classes and Code attributes separately from the
earlier frozen runtime capture. Constructor Code has 50 bytes and SHA-256
`65dc05fe7133a095c1d1c532b08a2131f0d56ade5364959ccbd91b0075864994`;
the six-byte `wq` constructor has SHA-256
`f6bf399d80cadad474e948edb2e7074e09b2c260c1667e3762dc4a05fab2b2c6`.

This constructor supports the actual Ghast inputs of the
[Player ally-alert comparisons](../../tests/reference/entity/player_ally_alert/README.md).
Those calls inspect the constructed owner and take the helper's literal early
return. They do not exercise Ghast flight, targeting, fireballs, spawning or
loaded Nether play. The native tick stops explicitly at the pending flying
dependency in `bp.w_()` instead of entering the Creature tick. The inherited
Living NBT path retains its actual declaring class. Broader Ghast behavior
remains unfinished.
