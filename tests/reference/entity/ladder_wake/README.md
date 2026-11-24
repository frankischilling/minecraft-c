# Ladder drop during Player wake

Two fresh JDK17 interpreters call the unchanged original Player damage method
after the Player enters sleep through its real method. The occupied Bed head
is at `(8,64,8)`, its foot at `(8,64,7)`, and an unsupported Ladder at
`(7,64,8)` has metadata two and air at its selected support `(7,64,9)`.

Normal wake clears occupancy, publishes one Ladder Item while the Ladder still
exists, removes the Ladder, then completes wake. A declared Item observer
exception preserves the Item and Ladder and stops before exit search. Ordered
observations retain the Player state, complete central Chunk plane hashes,
Random states, actual Item fields, callback and exception stacks.

Thirty-one original classes are pinned. The Ladder neighbor method has 139 Code
bytes; its inherited drop precedes the notifying air setter. The native
projection pairs 50 values for normal return and 42 for publication exception:
Player pose/dimensions and sleep state, health/idle, committed cells and ordered
cell callbacks, the publication cell snapshot, World/Player seeds, and Item
position, Stack, delay and membership flags. Wider state, constructor entropy,
Gaussian caches and Java stacks retain their original records without a native
equality claim. These prepared calls execute no ticks, frames or network access.

```powershell
$env:PATH = "$env:JAVA_HOME/bin;$env:PATH"
python -B tests/reference/entity/ladder_wake/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/ladder-wake-original
```

Raw streams, helper class files, original/tool/library identities and bounded
process receipts remain in the private capture. Public finite observations
and provenance bind the declared helper and reader sources.
