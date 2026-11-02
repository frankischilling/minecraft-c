# Player attack observations

`PlayerAttackObservation.java` constructs the actual original App, statistics
writer, World, Chunk, local Player and animal. It invokes `ob.b(gs,sn)`, which
executes `gs.d(sn)`. No game method, subclass or uninitialized allocation supplies
an attack result. The supplied client archive is pinned by SHA-256 in
`source-pins.json`.

Five authored scenes cover an ordinary Cow hit, downward velocity, a resisted
hit, fatal Sheep damage and an owned Wolf. The fatal case executes actual score
and kill callbacks, wool Item construction and World/Chunk publication. The Wolf
case publishes a real tame Wolf before the attack and observes its target field.
An ordinary capture completes without network attempts or native library links.
Original reflection also verifies that all eleven default Overworld Mobs inherit
`ls.h_()` and `sn.m_()`. Their method bytes and concrete class identities are
pinned. Wool inherits `uu.b(int)` and `uu.g()`; sixteen actual color calls return
white. Native controls check the same metadata and inherited no-op shape hook.

`finite.json` retains fourteen integer/boolean observations per scene: health,
hurt timers, last damage, idle counter, dying state, Player score, Mob kills,
damage dealt, Item count and Wolf flags/target identity. Constructor entropy and
the shared Math stream remain independent. This comparison does not cover their
values, geometry, sound, Item construction fields, mouse input or whole frames.
The existing animal damage/death packets retain their own broader state scopes.

To capture the unchanged original with one JDK 17 installation:

```powershell
python -B tests/reference/entity/player_attack/capture_reference.py `
  --jar b1.7.3.jar `
  --libraries "$env:APPDATA/.minecraft/libraries" `
  --output .local/player-attack-original
```

The output directory must be new. The controller hashes its tools, supplied
archive, libraries and authored helpers before and after execution. It records
the actual original class and method identities and verifies their archive
origin. `provenance.json` records the retained capture's scope and hashes.
This packet has no read-only debugger execution.

The public `entity.player_attack` test constructs native owners with authored
resources and compares the same five rows in its normal control run. Additional
controls exercise failures, source ordering and checked retirement. To compare
an already retained native stdout without rerunning the process:

```sh
python -B tests/reference/entity/player_attack/compare.py --stdout path/to/stdout.txt
python -B -m unittest discover -s tests/reference/entity/player_attack -p test_packet.py
```

The corruption tests reject changed semantic fields, Boolean/integer confusion,
reordered or repeated profiles, duplicate fields and malformed integer words.
Native mouse encounters and held Item attacks require separate qualification.
The finite Windows SDK attack run is recorded in
[the attack contract](../../../../docs/entity/player-attack.md) and this packet's
native validation metadata. It preserves the actual negative-Y damage bonus.
