# Chest container component reference

This packet records 111 calls against unchanged original `CD`, `OG` and `HP`
classes from the verified Beta 1.7.3 client. Two fresh processes agree: JDK17
interpreter mode and the default VM, both with verification enabled. The probe
blocks network connections/listeners and native-library linking; all counters
remain zero. No Display or GL call runs.

The `LW` arguments are controlled inputs, not genuine Player or Chest owners.
Each has a declared logical-size sequence, limit, usability result and 64-cell
physical array. Their methods record calls. The controlled split returns its
existing nullable Stack without mutation, and set stores directly without
clamping or dirty. These choices expose `OG` dispatch, repeated size reads,
short-circuiting and failed prefixes. They do not qualify original `JS` split,
set, eligibility or dirty behavior. `IZ` objects and all container/Slot bodies
are genuine original instances.

`cases.json` declares inputs before execution. `finite.json` retains raw
ordered callbacks, source Slot graphs, exception classes/messages, Stack fields
and aliases before and after every call. Symbols A/B identify supplied Stacks;
N labels identify newly observed objects in observation order, not allocation
order. A shared symbol always denotes the same object. Snapshots retain supplied
Stack identities even after the container stops referencing them.

The observations include:

- CD constructor layouts for 27/54 storage Slots, zero, partial rows, negative
  and minimum signed sizes; nullable Player and storage inputs; actual base
  lists, snapshot count, window0 and transaction0.
- OG ordered size/name/get/set/split/limit/dirty/usability dispatch, same-child
  aliases, nullable children and names, signed indices, overflow, repeated
  left-size queries and explicit child failures.
- CD Shift transfer at each storage/main/hotbar boundary, reverse placement,
  partial merge, full destinations, oversized/zero/negative source counts,
  unstackable IDs, invalid indices and shared source/destination identities.
- HP construction with actual nested CD and inherited ID state. Storage27
  gives width176/height168 and storage54 gives width176/height222. HP reads
  `LW.a()` again after CD construction. A controlled changing size therefore
  produces different HP and CD row counts. Both calls remain in the evidence.

CD Shift returns its original Stack clone even when no transfer changes the
count. Count zero returns a clone and clears its source Slot. Negative counts
return a negative clone without a merge. A declared shared source/destination
Stack has its actual alias effects retained. No tolerance or normalization hides
these cases.

The probe explicitly initializes original Blocks (`UU`) before Items (`GM`).
An earlier GM-first preparation failed during circular class initialization
before any case; its failed local capture is retained and identified in
provenance. The admitted dataset uses only the declared UU-first process.

`source-pins.json` contains archive class/Code hashes for 19 selected classes.
The process also reports loaded-resource hashes and their common source-JAR
hash; these match the archive. These are resource/origin checks, not JDI live
Code observations. The compiled helper directory contains only authored probe
classes, and no original class replacement is compiled. JAR, libraries,
JDK/module/JVM files, inputs and compiled helper bytes match before and after
both processes. Raw transport and compiled helpers remain ignored by hash.

`native-observations.h` is generated from declarations and observed results.
It contains nine exact CD layouts and 17 selected Shift cases. Its Stack cells
retain raw fields and alias labels, including detached input identities. Empty
controlled-array tails outside Player36 and storage27/54 are checked before
being omitted. `audit.py` regenerates the C token stream and checks every public
fingerprint. These tables supply expected data; their presence is not a native
acceptance result.

Reproduce with supplied local references and JDK17:

```powershell
python -B tests/reference/inventory/chest_container/capture.py --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/chest-original-new
python -B tests/reference/inventory/chest_container/publish.py --capture .local/chest-original-new
python -B tests/reference/inventory/chest_container/audit.py
python -B -m unittest discover -s tests/reference/inventory/chest_container -p test_packet.py
```

Format a newly generated C header with the repository's clang-format18 setup,
then run the same publication command again. Publication preserves formatter
output when its C tokens match the generated data. Publication runs no game code.
The four JDK17 SecurityManager deprecation warnings are required explicitly;
other stderr is rejected. Original registry preamble is retained and checked.

Actual Chest/Player ownership, right-click activation, screen initialization and
publication, close paths, drawing, physical input, saved-world behavior, original
image equality and gameplay FPS remain separate qualification requirements.
