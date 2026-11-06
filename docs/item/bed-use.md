# Bed Item use

Item 355 reaches the original client `LP.a(Stack,Player,World,IIII)` placement
body through the ordinary Item-use controller. It rejects faces other than
the top face before its Bed, Player, World or Math children. On the top face,
it increments Y with Java integer wrapping, captures `UU.T`, checks the actual
Bed type and reads the Player's yaw. The source float multiply/divide and
double addition precede the reached Math namespace initialization and floor
at bytecode 37.

The selected direction is `floor(yaw * 4 / 360 + 0.5) & 3`, with those source
intermediate types. The body checks foot air, head air, foot support and head
support in that order. A false predicate returns false without placing either
half. The implementation is in [bed.inc](../../src/item/use/bed.inc); it shares
the existing retained [Item-use call](../../include/mc/item/use/use.h).

The two World setters run at 167 and 194. Their boolean results are discarded
by the original. The foot uses direction metadata; the head adds bit 8. Each
setter reads the captured Bed object's current ID again, so a callback can
change the later ID. Only after both invocations return does LP read the actual
Stack count at 200, decrement with Java wrapping, store at 205 and return true.
It adds no early count, height, permission, collision or remote-World guard.

This matters at the original height boundary: a click at Y127 tests air at
Y128 and valid support at Y127. Both setters return false, yet LP decrements
the Stack and returns true. Callback failures retain the already placed foot
or pair and stop before the count store. A callback that changes the count
affects the late read. These outcomes remain visible to the caller.

## Native ownership and entry points

`mc_item_use_on_block_with_context_begin` accepts the defining Math namespace.
The call borrows that pointer from the enclosing graph and acquires its own
hold only when LP reaches Math at 37. Rejected non-top faces need no Math
owner. Failure retains reached World queries, setters, callbacks and class
causes until checked retirement. Native allocation, foreign-owner and
floating-environment refusals remain distinct from original exceptions.

The singleplayer controller carries this Math owner through its optional
sources context. Its normal activation-before-Item order and ordinary late
used statistic still apply. The public reference replay enters LP directly
through an internal facade, matching the captured original invocation;
separate authored controls exercise the ordinary wrapper.

## Evidence and remaining work

The [portable packet](../../tests/reference/item/bed_use/README.md) contains
72 original calls agreed by two fresh headless VMs, 1,438 selected JDI events,
eight reached Code fingerprints and 15 class origins. Cases include direction
thresholds, predicates, null arguments, changed source operands, discarded
setter results, signed count wrapping and callback failures. Native direct-body
replay passes the unchanged rows on Windows MSVC Release and Linux Clang 18
with ASan/UBSan. Exception/refusal effect prefixes are compared; their exception
classes are not equated.

```sh
ctest --test-dir build --output-on-failure -R '^(item[.]bed_use|tools[.]bed_use_packet(_optimized)?)$'
```

[Bed activation](../world/bed-interaction.md) and
[saved-Bed respawn](../world/bed-spawn.md) have separate source and test scopes.
These component rows do not establish crafting, successful nighttime sleep,
waking, a complete original World session, original pixels/audio or FPS.
