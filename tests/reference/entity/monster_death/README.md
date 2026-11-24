# Monster death observations

The optional collector executes 22 calls against the unchanged Beta 1.7.3 client
archive. The whole archive SHA-256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
It records exact method Code identities before execution and verifies that the
selected game classes load from that archive. No game subclass, bytecode rewrite,
substitute method or captured constructor seed supplies a result.

Each profile constructs a genuine `fd(new wy(), name, xa.a(0), 23L)` World,
one actual empty Chunk in its real `ok` provider cache/list, and a real Monster.
Positions are `(8.5,64.5,8.5)` for the victim and `(11.5,64.5,12.5)` for its
attacker. Health starts at 4. The private victim Random receives the declared
fixture seed after ordinary construction. Constructor entropy stays independent.
The shared Math stream is never reset. The World's private seed is 2468.

Rows execute in this order:

- Zombie `uz`, Spider `cn`, Skeleton `fr`, Creeper `gb`: direct `b(null)` for
  each seed 0, 2, 3, totaling twelve calls.
- The same four kinds: fatal `a(Cow,4)` with seed 38, totaling four calls.
- Creeper direct `b(Skeleton)`: seeds 0 and 2, first local then remote World,
  totaling four calls. The late record drop follows the inherited death body,
  including when the parent's remote gate skipped ordinary drops.
- Zombie and Skeleton fatal `a(Player,4)`: seed 38, score field 7, initial
  Player score 3, actual App/session/statistics writer, totaling two calls.
  Build Sword is absent. The actual kill callback reaches its achievement body
  but its prerequisite test prevents a Kill Enemy write or notice.

Profile names are `KIND-MODE-SOURCE-local|remote-SEED`. Every `MONSTER_DEATH`
JSON row contains the profile and operands, actual void/Boolean return, dying and
removed flags, health, final private Random state, ordered concrete sound names,
and actual dropped Items. Item records are
`[id,count,metadata,pickup_delay,world_member,chunk_members]`.
Player records are `[score,mob_kills,kill_enemy]`; other profiles use null.
Items really enter the original World and Chunk collections. Publication
callbacks observe each distinct Item after insertion and validate membership.

Separate `MONSTER_DEATH_ENTROPY` rows retain raw victim constructor seeds and
each Item's private seed, yaw, hover and velocity. These raw records remain
independent and are excluded from exact native comparison. No complete Item
kinematics, shared-stream equivalence, delayed death/removal, save/reload,
unrestricted combat, rendering, audio or FPS claim follows from this packet.

The target denies network connections/listeners and native loads. Writes are
allowed only under its new private output root for the genuine statistics
writer. Deletes and writes outside that root are denied and counted. The Timer
worker started by the real App is stopped and joined before exit. The runner
clears Java tool-option injection variables, requires matching JDK17 tools,
pins four SDK metadata archives, and retains bounded subprocess receipts.
Input and helper fingerprints must remain unchanged after execution.

```powershell
python -B tests/reference/entity/monster_death/capture_reference.py `
  --jar b1.7.3.jar --work-dir .local/monster-death-original
```

Use `--native-stdout PATH` to compare the first 20 typed non-Player rows from a
completed native fixture. The two excluded Player profiles are listed explicitly
in the comparison receipt; their observed original records remain available.
Use `--native-rows 22` only for a fixture emitting all 22 declared rows.
A native PASS line is required; a missing prerequisite or failed
capture never becomes a passing comparison. The six comparator controls reject
changed health, Random state, Boolean type, Item list, profile identity and
missing rows. Only the finite declared row fields are compared exactly.
