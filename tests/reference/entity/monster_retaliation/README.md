# Monster retaliation observations

This optional collector executes four nonfatal damage calls in the supplied
unchanged Beta 1.7.3 client. It constructs a genuine World, Zombie, Skeleton,
Creeper and Spider, with actual Cow attackers and prior Pig targets. It never
loads a graphical client or replaces an original method. The JAR must have
SHA256 `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.

Each independently constructed Monster starts at `(8.5,64.5,8.5)`, yaw `30`,
velocity `(.25,-.375,-.125)`, health `20`, resistance period `20`, active
resistance `0`, previous damage `0`, hurt timers `0` and attacked flag false.
The Cow stands at `(11.5,64.5,12.5)`. The actual inherited damage method receives
that Cow and amount `4`. No Random seed is reset, and no field is repaired
during or after the call.

The four return records retain the actual Boolean, health, previous health,
resistance timer, last damage, both hurt timers, raw binary32 attacked angle,
three raw binary64 velocities, attacked flag, concrete sound name and whether
the resulting target aliases that same Cow. The observer separately retains all
sound arguments, including the independently seeded pitch.

Use JDK 17 and the four pinned original SDK libraries from an existing Minecraft
installation. The default library root is `%APPDATA%/.minecraft/libraries` on
Windows or `~/.minecraft/libraries` on Linux. An explicit root is also accepted:

```sh
python -B tests/reference/entity/monster_retaliation/capture_reference.py \
  --jar /path/to/b1.7.3.jar --library-root /path/to/.minecraft/libraries \
  --work-dir .local/monster-retaliation-reference
```

The collector creates a fresh output directory. It records complete bounded
stdout/stderr, subprocess completion, JDK/JVM and SDK fingerprints, the original
class and Code manifests, compiled helper inventory, and immediate pre-execution
and final input fingerprints. The original process refuses network access,
native-library loading and filesystem writes. Expected JDK 17 SecurityManager
deprecation warnings remain in captured stderr. Only the authored observer
classes may appear in its compiled output.

To compare the four `MONSTER_DAMAGE_RETURN` rows emitted by the actual native
Monster damage fixture, add `--native-stdout /path/to/stdout.txt`. The supplied
file must contain the fixture's complete PASS line and exactly four return rows.
The comparison requires exact keys, JSON types, values, order and raw floating
words. Eight copied-data corruption controls check the comparator. The original
collector pins and preserves the supplied stdout file, but a matching file alone
does not establish its native build or execution provenance.

These observations cover the declared terminal fields of four nonfatal calls.
They do not compare complete heap state, Random streams, pitch, mount or self
branches, stronger repeated hits, death/drop behavior, World scheduling,
graphical presentation, audio output or FPS. Public helpers and a fresh public
collector run have their own identities; they do not inherit an earlier private
capture's provenance. Original JARs, assets, compiled helpers and capture outputs
remain outside the public repository. Default CTest runs do not execute Java.
