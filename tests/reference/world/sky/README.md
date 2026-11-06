# Original World and Dimension sky leaves

`finite.json` contains 366 unchanged-client observations: 72 rows for actual
`fd.b(F)F`, `fd.g(F)F`, `fd.e(F)F`, and virtual `xa.c()Z`; then 294 rows for
virtual `xa.a(FF)[F`. The matrix uses all three real Dimension constructors,
eight signed World times, three partial ticks, four live rain histories, and
49 sunrise angles with two partial arguments. Angles include signed zero,
negative turns, and values close to both sunrise-window boundaries.

Every sunrise row records whether the return is null and all four elements of
the actual private `xa.h` array, including its retained contents after a null
return. The observer requires a nonnull return to be that same array. Nether
inherits the base sunrise and horizon methods; the Sky overrides return null
and false. No provider, method body, Math table, or float array is replaced.

The helper verifies that World angle, the Sky sunrise override, and horizon
queries leave the original MathHelper class cold; the first star call performs
its real initialization. It also checks genuine Object-only RH prefixes:
off-window sunrise returns null, an in-window call throws for null `h`, and
the Object-only Sky override still returns null. The native test separately
checks the corresponding source PCs, untouched outputs, Math allocation
failure, unsupported FP controls, and released holds/call guards.

Native cold-Math checks run on the real Dimension methods before World owner
creation. The canonical native World classes require an initialized trig
table, so the World fixture then binds that same actual Math table and checks
its identity and hold count through the World calls. The original observer's
separate cold World-angle proof remains part of its unchanged-JAR receipt.

World uses Object-only allocation with a genuine constructed Info object and
the reached fields prepared directly. Dimensions use their real constructors
without World binding. This qualifies these scalar/array methods and their
constructor-owned array identities; it does not execute a World constructor,
biome setup, Chunk query, renderer, or Display. Measured original calls run
under a deny guard with zero network and native-link attempts. JAR, whole
declaring classes, selected Code attributes, helper, inputs, and the usual four
SDK metadata JARs are pinned and hashed before and after execution.

```powershell
python -B tests/reference/world/sky/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/world-sky-original-fresh
```

`run_native.py` requires successful native completion and exact equality of
all 366 rows. Its array result is borrowed from the actual retained Dimension;
the sky caller must keep that owner live through every later array read.
