# Flint and Steel use

Ordinary client Flint and Steel executes the original `YE.a(Stack,Player,World,IIII)`
body in
[flint_and_steel.inc](../../src/item/use/flint_and_steel/flint_and_steel.inc).
The [Item-use owner](../../include/mc/item/use/use.h) also supplies the real IZ
wrapper and its used-statistic tail. OB right use reaches that wrapper after a
clicked Block returns false from activation.

The six face comparisons independently adjust the target with Java integer
wrapping. A face outside zero through five leaves the coordinates unchanged.
The acquiring World ID read follows those adjustments. For an air result, YE
forms sound centers with integer-to-double conversion and double addition of
0.5. It resolves `fire.ignite`, reads the actual shared Item Random `GM.b`, and
draws one float for pitch `draw * 0.4f + 0.8f`.

The sound call completes before YE reloads the live Fire static and reads its
ID. The notifying World setter then executes the reached
[Fire and Portal bodies](../world/fire.md). YE discards the ordinary setter
Boolean. An occupied target skips sound and the setter, but still reaches the
same durability call with amount one and returns true. There is no remote-World,
Stack-count or face-validity check in this body.

Durability reads the Stack's current Item ID through the actual IZ methods. A
sound callback can therefore change which Item later controls wear. The
original strict damage comparison, broken statistic, count decrement and damage
reset remain in the shared durability service. Null Player remains valid in
the direct body; the IZ wrapper can subsequently fail at its reached Player
used-statistic call after completed sound, Fire and wear effects.

The internal direct YE entry adds no IZ resolution or statistic tail. In that
entry, null Stack reaches the durability invocation at 130. The public IZ entry
reads its Stack receiver before selecting YE. Both entries retain their reached
World, Stack, Player and callback producers until checked retirement. Failed
query, sound, setter and durability children remain inspectable through their
actual diagnostic views. Positive provider, WorldAccess List and Fire failure
witnesses determine Source exception classification; operation text and native
failure status do not.

The body does not advance World time or drain scheduling. Later TNT fuse and
explosion work, Portal travel, natural encounters, original audiovisual equality
and measured FPS retain their separate qualification scopes.

## Component evidence

Two fresh unchanged-original VMs agree on 104 prepared profiles. Windows and
sanitized Linux each match 34,159 selected original scalars and seven actual
receipt-lifetime controls. The complete Windows build and 1,266 tests pass,
alongside 13 focused Linux sanitizer tests and strict GCC compilation of six
changed C units. The [test record](../testing.md#flint-and-steel-use) preserves
comparison limits, including native App/statistics construction and persistence,
Throwable messages/stacks and the one null-IZ invocation-boundary PC. A separate
[native frame baseline](../performance/client-frame-baseline.md) measures one
authored scene and establishes no Item-use or original-game FPS comparison.
