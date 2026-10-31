# World and provider sky leaves

The source renderer uses actual World celestial angle `fd.b(F)`, rain `fd.g(F)`,
star brightness `fd.e(F)`, provider sunrise `xa.a(FF)` and horizon tint `xa.c()`.
These methods operate on canonical WorldInfo, weather, Dimension and Math owners.
Reached source reads, arithmetic and native holds retain their original order.

The provider returns its own h[4] array for sunrise/sunset and overwrites that
same storage only on the original qualifying angle branch. A null result leaves
the array's earlier contents intact. Sky Dimension's actual override returns
null without Math work and returns false for horizon tint. Nether inherits the
ordinary sunrise and true horizon methods; its separate no-sky field makes the
normal Sky draw return before these calls.

The [366-row fixture](../../tests/reference/world/sky/README.md) records 72 World
angle/rain/star/horizon rows and 294 sunrise rows, including every retained h
element and returned-array identity. It spans all three Dimensions, extreme
World times, partials and fade boundaries. Every row compares exactly on MSVC,
GCC13 and Clang18; native controls also cover null/FP order and ownership.

Prepared original World fields qualify these bodies rather than complete World
construction. The original helper separately demonstrates cold-Math behavior.
The native World class container requires an initialized canonical trig table;
the fixture checks the arithmetic-free Dimension operations before that native
bootstrap and preserves the same table/hold identity through World calls.
This setup distinction is explicit in the fixture and is not a whole-constructor
or native cold-World claim.

These leaves feed the [source sky renderer](../client/renderer-frames.md).
Sky colors, Dimension fog and cloud vectors remain documented in
[World colors](colors.md). Ordinary World ticks and terrain frames remain
unfinished.
