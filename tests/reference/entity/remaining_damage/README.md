# Remaining Mob damage observations

The optional collector executes ten complete damage calls against the unchanged
Beta 1.7.3 client archive with SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
It pins the complete archive and reached method Code identities, retains whole
class metadata, and verifies that the actual loaded classes come from that JAR.
Class and constant-pool identity matter: Slime and Ghast volume methods have
identical Code bytes but load different constants from their own classes.

Every profile constructs a genuine `fd(new wy(), name, xa.a(0), 23L)` World and
nine actual empty cached Chunks covering `[-1,1]` on each horizontal axis.
The victim and Cow attacker are constructed normally and remain unpublished.
Only dropped Items enter the World and Chunk lists. This cache also supplies
the complete empty result of the untamed Wolf's real nearby-pack query.

Victim position is `(8.5,64.5,8.5)`, Cow position is `(11.5,64.5,12.5)`, yaw is
30 and initial velocity is `(0.25,-0.375,-0.125)`. Health starts at 20, prior
health at 17, maximum resistance at 20, resistance/last damage at zero, hurt
timers at 3 and 4, death age at 7 and living age at 17. The dying/removed flags
are false and score is -1. The victim's private Random is seeded with 38 only
after ordinary construction and optional real Slime size setting. World Random
is seeded with 2468. Constructor and dropped-Item entropy remain independent.

The ordered profiles are:

- Slime `uw`, actual size setter `e(I)V` with size 1: damage 4, then damage 20.
- Slime `uw`, size 4: damage 4, then damage 20.
- Squid `xt`: damage 4, then damage 20.
- Ghast `bp`: damage 4, then damage 20.
- Wolf `gi`: Cow damage 4, then Cow damage 40. The real Wolf wrapper halves
  those amounts using `(amount+1)/2`, producing 2 and 20 damage respectively.

Each call has a fresh World, victim and source. Profile names are
`KIND-sizeSIZE-damageAMOUNT`. `REMAINING_DAMAGE` JSON rows contain exact typed
operands and return, health/resistance/hurt fields, raw angle and velocity words,
private Random state, dying/removed flags, sound callbacks and ordered Items.
Sound records are `{name,volume,pitch}` with nullable name and raw binary32 words.
Squid's actual null hurt/death sound still reaches the original WorldAccess
callback. Item rows are `[id,count,metadata,pickup_delay,world_member,chunk_members]`.
Every Item is observed after actual World and exactly-one-Chunk publication.

Separate entropy rows retain each victim constructor seed plus dropped Item
private seed, yaw, hover and velocity. Those independent records are excluded
from exact native comparison. Seven negative comparator controls reject changed
health, Random, Boolean type, Squid drops, profile identity, null sound and row
inventory. No broad normalization supplies a result.

The helper denies network connections/listeners and native loads. Writes are
allowed only under its new private output root; other writes and all deletes
are denied and counted. The runner clears Java tool-option injection variables,
requires matching JDK17 tools and pins four SDK metadata archives. Every bounded
process receipt and all before/after input/helper fingerprints are retained.

```powershell
python -B tests/reference/entity/remaining_damage/capture_reference.py `
  --jar b1.7.3.jar --work-dir .local/remaining-damage-original
```

Use `--native-stdout PATH` for exact comparison with all ten rows from a completed
native fixture. The fixture must end with a `PASS: N Remaining damage ... checks`
line. Failed captures and missing prerequisites never become passing results.

This packet covers declared direct damage calls and ordinary death/drop effects.
It does not execute delayed Slime splitting/removal, populated Wolf-pack behavior,
Player shooters, actual Arrow impact, save/reload, unrestricted combat, graphics,
audio playback or FPS measurements. The nullable sound record is a WorldAccess
event, not a claim that original audio playback accepted that name.
