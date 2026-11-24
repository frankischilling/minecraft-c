# Bone meal Block use

Dye damage 15 enters the Beta 1.7.3 bone meal body through the actual
ItemStack wrapper. Other signed damage values return false before reading the
World, count or Random. The body ignores Player and face. It recognizes
Sapling, Crops and Grass; this version has no mushroom-growth branch.

Recognized remote targets return true without growth, consumption or Random
draws. The enclosing ItemStack wrapper still reaches the real used statistic.
A null Player can therefore fail after a completed body and its effects.

## Growth and consumption

- Sapling reloads and casts its live Block receiver, evaluates World.r, then
  calls the existing Sapling growth body. Only a returned child reaches the
  late count decrement. An ordinary failed tree placement can restore the
  Sapling and still consume bone meal.
- Crops reloads and casts its live receiver, then calls the notifying metadata
  setter with stage 7. A returned void child consumes bone meal even for an
  already mature Crop or an ordinary false underlying write. Callback failure
  preserves its earlier effects and skips consumption.
- Grass consumes first, then performs all 128 source attempts. Each attempt
  walks up to seven steps, checking Grass below and solid support in source
  order. Empty candidates select Tall Grass or either Flower using the actual
  Item Random. Ordinary setter booleans are discarded.

Count subtraction and coordinates use Java wrapping arithmetic. Neither zero
nor a negative count prevents the body. Callback edits are observable: Crops
reloads count after its child, while Grass has already decremented it.

Grass reloads mutable GM.b at every reached draw. It does not cache that Random
or substitute World.r. Its vertical walk uses signed multiplication followed
by division toward zero. The first sixteen attempts perform no walk or
below/support test. No sound or plant-survival preflight is added.

## Original observations and native comparison

The [reference packet](../../tests/reference/item/dye_use/README.md) preserves
two separate ordered cohorts. Two fresh original VMs repeat 39 actual
ItemStack wrappers. An earlier pair repeats 78 interleaved direct and wrapper
calls; those observations remain supporting history. Each cohort uses one real
World and four canonical Chunks with declared array/height resets between
profiles. Pending queues are neither reset nor drained. The separate wrapper
cohort gives native replay the same conditioning history.

Both Windows MSVC and Linux Clang 18 ASan/UBSan compare all 39 wrapper rows:
36 selected outcomes match literally, including six null prefixes. Three
declared callback refusals match their complete selected effect prefixes.
Their native LIMIT causes remain distinct from original IllegalStateException;
exception-class equivalence is unqualified for those rows. Comparisons include
Stack fields, used statistics, both actual Random owners, ordered callbacks,
all five Chunk-array changes and the final Block/metadata map.

The saved original Item Random remains a separately observed identity when a
callback aliases live GM.b to World.r. That case changes both continuations and
the plant map. Thirteen guarded Linux component tests pass in 22.53 seconds
before the later icon addition, with all 4,721 inputs and 543 executables
unchanged. Full current-source qualification has its own scope in
[testing](../testing.md).

Thirty-six primary packet controls and 35 preserved historical controls pass.
Native comparison also runs 23 effect/policy corruption controls and ten
malformed-output controls. Its parser requires the exact existing achievements
and recipes prelude followed by every expected record. The first rejected
prelude capture completed the native calls but failed the closed output parser.
That failed receipt remains retained; the correction requires both exact
bootstrap lines and rejects wrong, extra, missing or reordered prelude lines.
No gameplay assertion was relaxed.

## Dye icons

The entered icon method implements KM.a(I)I using its live inherited bh field:

```text
wrap32(wrap32(bh + (damage % 8) * 16) + damage / 8)
```

Remainder and division truncate toward zero. Both additions wrap. Registered
bh 78 and damage 15 produce icon 191. Color, full-3D and rotation retain their
inherited base Item bodies.

The [icon packet](../../tests/reference/item/dye_icon/README.md) repeats 33
original scalar profiles in two fresh VMs, including all sixteen normal
damages, negative values and mutable-bh overflow cases. Windows native direct
and ItemStack icon results and inherited leaves match. A separate original
Item-to-Stack wrapper channel remains supporting evidence. The failed first
GM-first initializer attempt is retained; the completed cohort explicitly
initializes UU before GM.

Native cold-owner controls use Registry-owned RAW metadata without invoking
the Java-new allocation facade. They verify that the entered integer body
does not initialize classes, consume Random or alter its receiver. They do not
claim that Java allocation can produce an uninitialized cold instance.
The first native control incorrectly expected that facade to yield a cold RAW
Item and remains failed evidence. The corrected fixture preserves the runtime
allocation contract. A separate LF correction affected only the declared
path/newline projection of the original failed-initializer diagnostic; raw
captures, successful stdout and original method pins remain unchanged.

The four focused Windows icon/render tests pass, including the 33-row replay.
The final sanitized Linux public suite also passes that replay and both Dye
packet/native pairs. The earlier pre-icon Linux result retains its narrower
scope.

## Windows loaded Crop and save cycle

The optional `client.dye_loop` scenario passes 3,136 checks in 7.96 seconds,
with all 1,994 selected inputs/products and 413 imported assets unchanged.
One ordinary right-click uses bone meal on the loaded stage-zero Crop. The
real Block-use path changes stage 0 to 7, decrements the same Dye Stack from
two to one, increments its actual used statistic once and returns the child
through checked retirement. There is no in-air use, sound or Block placement.
The returned World/HUD and held Item draw uses icon 191; health stays 5.

The target section display list is replaced. All 48 position/UV records at
both stage 0 and stage 7 match the executed original Crops geometry. The
48 colors at each stage match the source-lighting check. This qualifies those
geometry fields, not original whole-frame pixels.

Actual Escape and Save and Quit produce a pre-frame owned NBT sample.
Independent Region and Player NBT decoding finds Farmland 60, Crop 59/stage 7
at (3,65,3), health 5 and exactly one Inventory entry: slot 0, Dye 351/count
1/damage 15. Reselection constructs distinct World and Player owners with
those values and returns HUD/held draws. Cleanup leaves no windows.
Pre-bootstrap difficulty 2 prevents peaceful regeneration; the observer does
not repair live health, camera, clocks, Random, World or input queues.

The first native SDK process reaches the completed input receipt, then fails
a combined post-use assertion covering Inventory, statistics, Crop state,
held drawing, section/geometry and NBT. Its precise failed term remains
unresolved. The later passing capture retains those checks and does not prove
the earlier failure's cause. An earlier idle guard also refuses launch; both
that refusal and the failed process remain recorded. The passing guard and
cohort record no last-input timestamp changes, within that monitor's scope.

## Ownership and remaining scope

Dye reuses its latest acquisition, setter and detailed support diagnostics
after harvesting their returned producers. Exact receipt identities stay held
until retirement. Owned loader copies distinguish later reuse of the same
loader address and preserve every live nested alias. Existing Hoe/Seeds calls
retain their two distinct query allocations. No gameplay work is capped to fit
a diagnostic array, and retirement never replays a failed source operation.

Sapling's null World Random still reaches an explicitly unqualified native
child frontier rather than a typed Java exception. Sheep coloring is a
different entry path and remains unfinished. The local Windows loaded-Crop
scenario above has its own source/product receipt. The final Linux public
suite passes 670/670 tests in 164.89 seconds. Both Windows public cohorts pass
679/681, in 350.24 and 317.34 seconds, with unresolved focus and final reload
decoder failures; all four Dye packet/native tests pass in each cohort.
[Test details](../testing.md#bone-meal-block-use-dye-icons-and-loaded-crop)
retain the unchanged-input brackets and metadata-only rotation whose exact
writer is unresolved. Four fresh Workbench shutdown profiles also pass 8,259
checks in 61.63 seconds with all 1,977 selected inputs and 413
assets unchanged. That native ownership scenario does not invoke Dye growth.
Its first attempt remains NOT_RUN after a refused passive guard. The passing
attempt records zero guard timestamp changes and one cohort-monitor change;
the latter identifies neither a human nor a cause. The preceding Workbench
packet remains historical. These results do not establish loaded Grass/tree
growth, unrestricted farming, original pixels, audio, dedicated-server behavior
or an FPS gain.
