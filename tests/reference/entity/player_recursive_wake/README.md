# Recursive Bed wake reference

Two fresh JDK 17 interpreter VMs each call unchanged `gs.a(sn,int)` three times
with a null attacker and zero damage. The packet reuses the published damage
wake setup, complete class pins and actual App, World, Chunk and Player
constructors. The Player enters sleep through the original sleep method. The
probe replaces the setup observer through the real World add and remove
methods, then prepares an orphan Bed head at `(7,64,8)` next to the occupied
head at `(8,64,8)`. The orphan foot at `(7,64,7)` is air.

The occupied head changes metadata from 12 to 8. Its neighbor notification
reaches the orphan Bed hook, which commits air at the orphan head before the
recursive cell observer runs. All three profiles retain those two changes and
the two actual callback stacks.

| Profile | Observed completion |
| --- | --- |
| `success` | Wake completes, the exit position and bounds change, the sleep timer becomes zero, and zero damage returns `false`. |
| `observer-error` | The orphan observer throws the declared `IllegalStateException`. Standing dimensions and both Bed changes survive. The Player stays asleep at the original position with timer 37. |
| `null-exit` | The orphan observer clears the actual `Player.World` and returns. The original exit search `ve.f` throws `NullPointerException`. Standing dimensions, both Bed changes and the sleeping state survive. |

The verified 177-byte `ve.f(Lfd;IIII)Lbr;` Code has SHA-256
`73d8e8bf553c2ff112e2ec45c26941790fc8712020f5228954ce15e757cb3327`.
The retained original stack starts at `ve.f`. Its verified bytecode shows
`fd.e(III)` at 4 as the first dereference of the null World argument. The
packet records the actual Java stack and derives the bytecode site from that
method's pinned instructions.

```powershell
python -B tests/reference/entity/player_recursive_wake/capture_reference.py `
  --client-jar "$env:APPDATA/.minecraft/versions/b1.7.3/b1.7.3.jar" `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/player-recursive-wake-new
python -B tests/reference/entity/player_recursive_wake/packet.py
```

The collector checks the original client and SDK archive hashes and the 29
shared original class pins. It retains the actual class origins, tool and JVM
input hashes, private bytecode output, and bounded process receipts. Both runs
agree on every selected value. `finite.json` and `provenance.json` contain the
selected state and source bindings. The raw streams and compiled helpers stay
in the private capture. These observations cover prepared CPU component calls.
