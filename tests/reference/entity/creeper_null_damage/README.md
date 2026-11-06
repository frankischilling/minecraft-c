# Creeper NULL-attacker damage static source pins

This packet is STATIC ONLY. It reads the supplied unchanged client JAR offline;
it does not load a game class, invoke a constructor or execute original damage.
It contains class/Code identities, complete declaration inventories, selected
resolved references and short instruction windows. No native/original runtime
comparison, damage return, audible effect, death or drop is credited.

GB extends GZ, GZ extends II, II extends LS and LS extends SN. GB has no
a(Entity,int) declaration and inherits GZ. The GZ invokespecial at bytecode 3
names II.a, which resolves to LS because II has no matching declaration.
LS.U at bytecodes 63 through 69 loads this, NULL and damage 1, invokes damage
at bytecode 66 and discards the result. SN's environment call at bytecode 501
and LS.U's SN call at bytecode 9 are pinned as separate static callers.

GB.j_ returns the original String `mob.creeper`; GB.i returns
`mob.creeperdeath`. Each three-byte body is pinned together with its resolved
constant-pool String and UTF8 bytes. The inherited LS.k leaf returns 1.0.
LS damage's hurt/death sound, volume and World sound references are retained.
These declarations establish dispatch and literals; they are not executed
Creeper sound observations or replacements for existing receiver-specific data.

Fatal LS damage invokes virtual LS.b(Entity) at bytecode 316. For GB the
override is GB.b(Entity). Its first three instructions load this/attacker and
invoke special GZ.b at bytecode 2. GZ and II have no such declaration, so that
parent call resolves to LS.b. Only after that unconditional parent call does
the original body test whether the attacker is a Skeleton at bytecode 6.
The static prefix does not credit completion of the parent death/drop body or
the later record branch. A native frontier at GB.b bytecode 2 must retain that
order and target.

```powershell
python -B tests/reference/entity/creeper_null_damage/verify_source.py --jar b1.7.3.jar
python -B tests/reference/entity/creeper_null_damage/audit.py
python -B -m unittest discover -s tests/reference/entity/creeper_null_damage -p test_packet.py
```

The archive verifier compares fresh class/Code inventories and resolved
references with `source-pins.json`. Missing archive input reports UNAVAILABLE
with exit 77; changed identities report FAIL with exit 1. The public audit and
23 corruption controls need neither Java nor original assets. They reject
runtime-credit claims, receiver/hierarchy conflation, missing declarations,
wrong sound constants/UTF8, changed virtual/special dispatch, moved parent
prefixes and substituted NULL/damage/volume inputs. Provenance records the
verified archive and read-only parser/helper identities. Existing captured
packets remain unchanged.
