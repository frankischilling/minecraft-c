# Workbench Container interaction reference

This packet observes 116 unchanged-original Workbench calls on genuine
`KQ -> FD -> DC -> IX/AA` owners and a genuine `OO -> IT` screen/Container.
Every case constructs `OO` from the actual Player Inventory and World. Actual
`DA` initialization calls inherited `ID.b`, which publishes that screen's `IT`
as `Player.e`. The Player constructor's actual `AA` remains `Player.d`.
The supplied controller window argument7 does not replace the Container's
constructor window0.

The cases cover left/right pickup, placement, splitting, swapping, merging,
metadata, full and oversized Stacks, shared aliases, outside cursor drops,
Shift transfer and recipe-result pickup. The 46 Slots retain the original
layout: result0, crafting1 through9, main10 through36 and hotbar37 through45. Chest and furnace recipes
exercise the 3x3 grid. Shift cases include both ends of the main/hotbar ranges,
the last crafting cell and reverse result placement. Invalid Slot indices
retain their actual exceptions.

Close operations have explicit contexts:

- `close` calls the genuine controller. It closes active `IT` once and restores
  `Player.e = Player.d`.
- `direct-close` calls the actual `IT.a(Player)` body once and retains the active
  owner. `repeat-direct-close` calls that body twice.
- `screen-close` calls genuine `OO.h` while `Player.e` is its `IT`. `ID.h` first
  closes the active Container through the controller; `OO.h` then closes its own
  captured Container again.
- The `-remote` suffix declares `World.B = true` before the call. The inherited
  cursor-close prefix still runs; the nine-cell grid loop is skipped.

Original `IT` close has no explicit crafting-grid clear. A two-cell grid
therefore publishes two Items through controller close and four through direct
`OO.h` in this context. A shared cursor and three shared grid references produce
seven publications through that direct screen close. The retained aliases and
all raw publications remain in the evidence. These direct screen calls do not
qualify keyboard close: actual `DC.r` first resets the active owner through
`GS.r`, so its caller context differs.

`cases.json` declares every pre-call input. The genuine World/Chunk constructor
inputs include a Crafting Table at `(8,64,8)`; the Player is at `(8.5,64,8.5)`
with yaw/pitch0. Stack labels identify genuine constructed inputs; N1 and later
labels preserve newly observed identities. Every before/after snapshot retains
main36, armor4, crafting9, result1, cached46, cursor, statistics and every
previously observed Stack's ID/count/damage/animation. The audit independently
checks the declared aliases, input Stack fields and initial owner/cache/stat
state. No game field is repaired during an invocation.

`finite.json` retains all ordinary rows. `observations.json` retains the
independent JDI rows, 50 actual Item-constructor observations, 1538 selected
ordered callbacks, the exact Slot graph, a separate empty `OO` lifecycle and
77 entered live Code pins. Full raw method-entry transport remains local by
hash; `audit.py` declares the callback selection. `source-pins.json` inventories
the original classes and methods, including `FI`, `DC`, `OO`, `IT`, `ID` and
inherited Container/Slot/Stack bodies. Archive and live Code agree.

The strict comparator audits each process's actual Player-private stream,
four nextFloat draws per dropped Item, exact source float/double arithmetic,
unchanged World entropy, pickup delay40 and actual World/Chunk publication.
JDI observes the constructor's four independent shared Math.random returns
and checks its actual phase/yaw/velocity formulas. Item-private entropy stays
unchanged between construction and publication. No Random is replaced or
reseeded. The observer reconstructs each drop's argument and calls the pure
JDK trig leaves; an independent Decimal Taylor audit checks binary64 bracketing
under the JDK17 Math accuracy contract. This is an accuracy audit, not bitwise
Java/native trig-backend equality. Raw entropy and velocity words remain intact;
only the stated deterministic fields are compared between processes.

The separate CPU lifecycle returns from genuine `OO` construction, actual
`DA` initialization/`ID.b` publication, an unrelated key, inherited live tick,
no-pause query and direct empty `OO.h`. The constructor input flag is false.
The App constructor returned normally and its actual Font remains null.
`App.r` screen replacement, local `FI -> DC -> Minecraft` opening, keyboard
close, eligibility-driven close and completed Workbench drawing remain separate
evidence gates. No Display or GL execution is used by this packet.

Reproduce with the supplied original JAR and pinned JDK17/libraries:

```powershell
python -B tests/reference/inventory/workbench_interaction/capture.py --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/workbench-original-new
python -B tests/reference/inventory/workbench_interaction/publish.py --capture .local/workbench-original-new
python -B tests/reference/inventory/workbench_interaction/audit.py
python -B -m unittest discover -s tests/reference/inventory/workbench_interaction -p test_packet.py
```

Capture blocks network/native linking and verifies original JAR, JDK, libraries,
authored LF helpers and compiled helper bytes before and after execution.
Publishing runs no game code. The audit and 30 corruption tests need neither
Java nor original assets. The native adapter can use `capture.declarations` for
the unchanged eleven-column TSV format and `compare.rows`/`compare.compare` for
`WORKBENCH_INTERACTION` rows. Independent native comparison, full UI and saved
world acceptance have their own receipts; this packet supplies no pixel,
whole-frame, general gameplay, focus-stability or FPS claim.
