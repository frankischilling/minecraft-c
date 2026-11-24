# Falling Block and FallingSand source map

The [verified original binaries](binaries.md) supply the paired method bodies.
Client `gk` and server `ei` extend the respective Block classes; gravel `ne/ig`
adds a constructor and drop-ID override, inheriting these falling methods.
The [native implementation](../world/falling.md) preserves their source order.

| Operation | Client / server | Source PCs |
| --- | --- | --- |
| Addition hook | gk.c / ei.c, World and three ints | 0–16; own ID6, delay10, actual World schedule13 |
| Neighbor hook | gk.b / ei.b, World and four ints | 0–16; same body, neighbor argument unused |
| Block tick | gk.a / ei.a, World, three ints and Random | 0–9; private call6, Random unused |
| Private falling body | gk.h / ei.g | 0–166 |
| Tick delay | gk.e / ei.c, no arguments | 0–1; returns three |
| Static pass-through predicate | gk.c_ / ei.c_ | 0–61 |
| Shared flag initializer | gk / ei | 0–4; false store1 |
| World-only FallingSand constructor | ju / gl | 0–10; base2, age7 |
| Full FallingSand constructor | ju / gl | 0–80; base2, age7, carried ID13, size26, position45 |
| FallingSand NBT write | ju.b / gl.b | 0–11; TileEntity narrowing7, setter8 |
| FallingSand NBT read | ju.a / gl.a | 0–14; Byte getter4, mask10, carried store11 |

The private body calls the predicate at19 before its Y sign test. Flag34
selects immediate descent; otherwise nonloading radius32 presence67 selects
between descent and Entity construction. Immediate clearing79 precedes the
loop's predicate90 and positive-Y test97. Only positive final Y reaches the
late own ID116 and notifying placement119. Ordinary setter booleans are ignored.

The other branch allocates an actual FallingSand at126 before argument
arithmetic. Its coordinates convert int to Float, add `0.5f`, then widen to
Double. The late receiver ID151 reaches constructor154 and World publication162.
There is no source-block clear on that branch.

The static predicate queries the actual block at4. Air returns first; nonair
reads the current fire static18 and its ID21. Otherwise live Block array29,
original signed index34 and selected Material35 precede comparisons against
water42 and lava52. A nullable Material is an ordinary identity value.
Each static invocation has declaring-class active use at its caller site.

FallingSand's full constructor uses the genuine base owner once. Size changes
leave old bounds intact until position45 updates the same bounds. Own age is
not saved. TileEntity writes narrow the carried int to Byte; reads mask with255.
The actual EntityList name is `FallingSand`, numerical registration21.

Native contracts cover prepared method, queue, ownership, constructor and NBT
scenarios. No fresh original falling execution or native replay was performed.
The later FallingSand Entity tick, inherited movement/collision, placement
permission and direct Item-drop descendants remain unfinished. The method map
does not qualify those bodies, arbitrary recursion or original JVM failure timing.
