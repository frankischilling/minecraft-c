# Orphan Bed foot during Player wake

The collector invokes unchanged Player damage with a null attacker and zero
damage after the original Player has entered sleep. The occupied head is at
`(8,64,8)`, its foot at `(8,64,7)`, and an orphan foot at `(7,64,8)` has air
where its head would be at `(7,64,9)`.

Two fresh JDK17 interpreters execute normal return and a declared exception in
the Item publication observer. Ordered observations retain both cell changes,
the actual Item and Player state, World and Player Random states, callback
stacks and return or exception. All thirty original classes are pinned,
including the Item class checked against the earlier Item tick packet.
Constructor entropy and fields outside this declared observation remain
separate from native comparison. The native projection selects Player pose and
sleep state, committed cells, both Random seeds, and Item position, Stack,
delay and publication flags. Item motion, yaw and phase and the Random Gaussian
cache fields remain unpaired. These are prepared CPU calls without ticks,
frames, device output or network access.

```powershell
python -B tests/reference/entity/bed_foot_wake/capture_reference.py `
  --client-jar "$env:APPDATA/.minecraft/versions/b1.7.3/b1.7.3.jar" `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/bed-foot-wake-original
```

Original raw streams, authored helper class files, tool and archive identities
and complete process receipts remain in the private capture. The finite packet
retains the selected original state and the source/Code bindings.
