# Client Item tick

`mc_item_entity_tick_in_context` executes the genuine canonical client
`hl.w_()V` body on a constructed Item. It first executes `sn.w_()V`, whose
virtual `U()V` resolves to inherited `sn.U()V`. The rest of the call performs
pickup-delay decrement, previous-position stores, gravity, the current Material
query and lava fizz, inherited six-neighbor push-out, the actual mover, current
ground friction, drag/bounce, both age counters and inherited `K()` at age 6000.
An already-dead Item still executes the source body. Its nullable Stack is not
read by this method.

The loaded code Registry is an explicit borrowed context, independent of the
Item's nullable World. Native holds protect the actual Item and Registry through
synchronous callbacks. World and source fields are captured/reloaded at their
reached operations. Every failure keeps prior fields, Random, watcher, pool and
World effects. Constructor diagnostics remain unchanged. Environment and mover
limits retain their existing defaults; explicit zero ceilings fail at the
pending operation instead of reporting truncated work as success.

The Item water override is `hl.k_()Z`, a 16-byte original method. It passes the
same permanent bounds to `World.a(bounds,Material.g,Entity)`. The existing
environment API already dispatches this override, so the new tick adds no
alternate water calculation. Lava, damage, death, material/block reads, sound and
movement likewise use existing genuine owners and algorithms. The private base
entry resolves Item's `sn.ab()` to inherited `K()`; the public Player entry and
its separate Living void-damage dispatch retain their behavior.

Inherited `sn.c(DDD)Z` checks the center and then the six neighbors in
minus-X/plus-X/minus-Y/plus-Y/minus-Z/plus-Z order. Strict less-than comparisons
choose the first closest open face. When the center is solid, it consumes one
ordinary `nextFloat` even when every neighbor is solid. Its ordinary return is
always false, which the Item caller discards. This differs from the separately
owned LocalPlayer push-out override.

Source Random order is retained: the Item's own lava branch draws two floats
for X, two for Z, then the fifth for fizz pitch. Inherited water entry may draw
for splash sound, bubbles and splash particles before the Item's gravity and
material query. The initial base flag suppresses that entry effect. The Item body
introduces no direct Math.random call or reset; reached WorldAccess bodies keep
their own entropy effects. Both Item counters and the base tick
counter use Java int wrap. Neither health/death nor pickup-delay fields are
invented as an early-return condition.

The error has a discriminator and one full active child. Read that child only
when its tag matches; successful diagnostics are freshly empty. This keeps the
actual failed environment, World, Registry or movement diagnostic without
appending mutually exclusive large records. The runtime uses C17 and existing
strict x86-64 arithmetic, with no JVM or new mutable owner layout.

The finite reference fixture uses unchanged original `hl`, `fd`, `lm`, `iz`
and watcher classes with a concrete authored `pm` observer. Native setup uses
the existing genuine dual-side GAME World/Store/Registry/Chunk fixture; only
client Items are ticked. Input fields and loaded Chunk arrays are authored
after real construction. Before each observed call, actual Entity Random
`setSeed(38)`/native reseed operations declare the same input. No captured draw
outputs enter runtime. Constructor/default entropy and startup order remain
separate qualifications.

There are 37 ordered states and 33 ordered events, all compared as raw bits.
Thirty-three calls complete. Three retain reached authored sound/particle
callback failures; the fourth prefix is the actual NULL-World water call after
the base counter/previous-field stores. Coverage includes six push-out exits,
enclosure and tie order, stone/ice friction, lava and water, callback mutation
and rereads, health/dead continuation, delay/age/int wrap, void death, null
World, and quiet/signaling NaN, infinity and signed-zero no-clip motion. All
70 records match on MSVC Debug and separate sanitized Clang/GCC Debug profiles.

Native controls additionally cover RAW/wrong-kind/hold/FP admission, actual
zero material/particle/mover work ceilings and their retained prefixes, a
genuine nullable Stack, and 128 repeated full ticks without owner or arena
growth. Each profile passes 1,089 native checks. MSVC uses its actual default
1 MiB PE stack; neither stack limits nor diagnostics are weakened.

Run the repeatable native CTest route without Java or imported assets:

```text
python tests/reference/entity/item_tick/run.py --native PATH_TO_TEST --work-dir OUTPUT_PARENT
```

The optional original route adds `--client-jar`, `--library-dir` and `--java`.
It checks the pinned JAR/support archives, provided whole-class and method Code
metadata, 19 resolved Entity methods, actual game-class origins, matching
same-bin JDK17 Java/javac, helper inventory/Code and before/after inputs/helper products.
Captures are bounded, require successful exit and empty runtime stderr, and
require positive final markers plus the complete ordered state/event inventory.
Seven controller rejection controls pass normally and with `python -O`.

World tick dispatch, collection removal, pickup, rendering and the selected
client WorldAccess implementations are separate owners. Their real missing
descendants retain the existing failure boundaries when reached. A NULL
passenger uses the source empty recursive fall path; the existing mover's
unsupported nonnull passenger frontier remains explicit. This qualification
does not claim a complete World tick, server Item gameplay or client startup.
