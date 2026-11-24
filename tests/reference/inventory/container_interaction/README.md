# Player Inventory Container interaction reference

This client packet observes 73 unchanged-original controller calls on genuine
`KQ -> FD -> DC -> IX/AA -> GP/PV/YV` owners. It covers left/right pickup,
placement, splitting, swaps, merging, full/oversized Stacks, metadata,
shared-Stack aliases, armor acceptance and limits, Shift transfer, crafting
output, outside clicks and Container close. Deliberate negative/past-last Slot
indices preserve their actual exceptions. The default and active Containers are
the Player constructor's actual `d/e` references; the supplied window argument
7 does not replace the actual default Container's window 0.

`cases.json` declares every pre-call input. Stack labels A/B identify genuine
constructed inputs. N1, N2 and later labels preserve newly observed identities,
assigned by publication order, returned Stack, then snapshot traversal. The
before traversal observes cached cells, main, armor, crafting/result and cursor.
Every snapshot retains all previously observed Stack owners and their actual
ID/count/damage/animation. No game field is repaired during a call. The original
classes/JAR, constructors, Random instances and shared Math entropy remain
unchanged. No game subclass, RAW allocation, Unsafe, class substitution or
Random rebinding supplies the execution.

`finite.json` retains all raw ordinary rows. `observations.json` retains the
independent JDI rows, actual ten Item constructors, selected ordered callbacks,
the 45-Slot graph, headless screen observations and 59 live Code pins. Full raw
method-entry transport is retained locally by hash. The callback selection is
explicit in `audit.py`; it excludes the thousands of recipe-query reads while
retaining setters, splits, clones, changed/pickup callbacks, statistics and
publication calls. Source-code inventories are in `source-pins.json`.

The strict comparator first audits each process's actual Player-private seed:
four nextFloat draws per dropped Item, exact source float operations, and exact
binary64 velocity arithmetic using that process's recorded trig leaves. It also
requires the World stream unchanged, actual Item/Stack identity, pickup delay40,
World/Chunk membership and ordered publication. A 110-digit authored Decimal
Taylor audit checks that each trig leaf is one of the two binary64 values
bracketing the true result on the declared angle range. This follows the
[JDK17 Math accuracy contract](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/lang/Math.html).
It is a bounded numerical audit, not bitwise Java/native trig-backend equality.
No payload is rounded or normalized. Constructor JDI directly observes all four
shared Math.random returns and verifies their phase/yaw/velocity formulas;
Item-private entropy remains unchanged between construction and publication.
Math.cos/sin intrinsic return events are unavailable in this JDI profile. The
original helper independently calls the pure JDK functions on its reconstructed
angle; the native receipt captures the actual production callee returns.

Only after those independent audits does comparison select exact deterministic
Inventory/crafting/cursor state, retained/returned clones and aliases, statistics,
exceptions, callbacks and deterministic drop fields. Raw independent entropy and
trig/velocity words remain in both captures. A failed earlier capture retains a
real JDK-versus-Python-libm difference; it prompted removal of the unjustified
assumption that Python's libm reproduces every JDK Math bit.

Two unusual source outcomes are intentional observations: an empty normal main
Slot click marks Inventory changed; output pickup does not. Picking four planks
into an existing four-plank cursor records crafted count8. Shift output in these
profiles records count0. These are preserved, including the original mutations,
rather than replaced with modern Inventory rules.

`item-name-inventory.json` separately records all202 actual Item IDs, 38 runtime
classes and six `a(IZ)` declaring bodies (GM/CK/BI/EN/KM/RX), with metadata0/15
values, repeated/unary String identity, actual NH.b translation and JDK trim
values. Slab44 metadata15 throws ArrayIndexOutOfBoundsException. It supplies no
complete tooltip/draw or specialized icon/color qualification.

The genuine UE/ID/static-BB CPU observation includes the open achievement,
actual Player Container capture, input flag, dimensions, no-pause result,
DA initialization, an unrelated key, live tick and direct ID.h close. The App
constructor returned normally; its actual Font slot is still NULL. No Display,
Font, Engine, Player-renderer or completed Inventory draw is claimed. E/Escape
through DC.r/Display and the full UI lifecycle remain separate evidence gates.

Reproduce with the supplied original JAR and pinned JDK17/libraries:

```powershell
python -B tests/reference/inventory/container_interaction/capture.py --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/container-original-new
python -B tests/reference/inventory/container_interaction/audit.py
python -B -m unittest discover -s tests/reference/inventory/container_interaction -p test_packet.py
```

The audit and corruption tests need neither a JVM nor original assets. Capture
blocks network/native linking, retains stdout/stderr and verifies authored LF
helper/JAR/library/JDK bytes before and after execution. `provenance.json` records
the actual successful/failed captures and independent native comparison scope.
Native comparison does not prove complete source call-trace equality, pixels,
whole-frame parity, general gameplay, focus stability or FPS.
