# Collision bounds reference map

Authority is the verified Beta client and dedicated server documented in the
[binary audit](binaries.md). Client `eq` and server `cz` use public doubles
`a,b,c` for minimum X/Y/Z and `d,e,f` for maximum X/Y/Z. Static `g` is the shared
List and `h` its Java int cursor. Complete class bodies were read, including all
branches and terminal instructions. All 23 paired bodies agree after actual
side-specific names and constant-pool references are mapped. The client has two
additional bodies: pool clear and mean-edge length.

| Behavior | Client | Server | Source sites |
| --- | --- | --- | --- |
| Direct factory | `a(DDDDDD)` | `a(DDDDDD)` | New0, constructor14, return17 |
| Pool initialization | `<clinit>` | `<clinit>` | List constructor4, publish7, cursor11 |
| Cursor reset | `b()` | `a()` | Cursor store1, return4 |
| Pool clear | `a()` | Absent | List.clear3, cursor9, return12 |
| Pooled factory | `b(DDDDDD)` | `b(DDDDDD)` | Zero factory23, append26, cursor41, get44, setter62, return65 |
| Directional sweep | `a(DDD)` | `a(DDD)` | Pooled factory124, return127 |
| Symmetric expansion | `b(DDD)` | `b(DDD)` | Pooled factory62, return65 |
| Shifted copy | `c(DDD)` | `c(DDD)` | Pooled factory38, return41 |
| X/Y/Z clamps | `a/b/c(box,D)` | `a/b/c(box,D)` | Early returns25/51, final131 |
| Box intersection | `a(box)` | `a(box)` | False25/51/77, true79 |
| Same-box translation | `d(DDD)` | `d(DDD)` | Stores7/17/28/38/48/59, same-object return63 |
| Contraction | `e(DDD)` | `e(DDD)` | Pooled factory62, return65 |
| Pooled copy | `d()` | `b()` | Pooled factory24, return27 |
| Coordinate assignment | `b(box)` | `b(box)` | Stores5/13/21/29/37/45, void return48 |

The [native bounds contract](../entity/bounds.md) implements the movement helpers
and actual pool identities without substituting value-only temporary boxes.
Permanent Entity construction keeps its distinct direct box. Native storage
holds and allocation limits describe C ownership; original JVM allocation-failure
and garbage-collection timing are not reproduced by those policies.

Client `Minecraft.run` calls cursor reset at PC65 once per outer frame before
processing queued ticks and rendering. Server `MinecraftServer.h` calls it at
PC120 once per full server tick before iterating Worlds. Client `Minecraft.e`
calls clear at PC20 in error recovery. No helper may invent a per-movement reset.

The complete source inventory also includes strict vector containment, client
mean edge, segment intersection, three private vector face tests and `toString`.
These bodies remain unported. Segment intersection creates six pooled Vec3
interpolation candidates, validates faces, selects the nearest candidate while
keeping the first equal-distance result, then constructs a HitResult that makes
another pooled Vec3 copy. Real separate Vec3 ownership, its signed-zero behavior,
HitResult construction and class initialization are required before that route
can work. Native `printf` does not implement Java StringBuilder/double formatting.

Source review and helper comparisons cannot establish full Entity movement,
World collision collection, FallingSand physics or an application reset route.

Initial exact raw-bit comparison found two-NaN addition payload differences:
74 rows per side on MSVC and GCC, and 50 per side on Clang. The original reports
remain failed and retained. A separate complete 64-pair observation of quiet and
signaling NaNs, both signs and distinct payloads, confirms the original captured
interpreter selects the quieted right operand. The native addition compatibility
operation preserves that choice explicitly; ordinary subtraction and raw stores
remain separate. These are observations of the pinned host interpreter, not a
claim that Java specifies universal NaN payload selection.

The public [reflection observer](../../tests/tools/entity/bounds/BoundsObservation.java)
invokes the genuine verified JAR methods in an isolated loader. Its
[comparison driver](../../tests/tools/entity/bounds/compare_bounds.py) retains
commands, complete raw-bit rows, streams, actual process outcomes and before/
after input hashes. The full corpus contains 17,388 client and 16,146 server
rows, including every ordered pair of eight quiet/signaling NaNs through all
coordinate slots and helper operations. It never normalizes a mismatched value.

After a normal test build, run from the repository root with Java and javac on
PATH and the two verified local reference JARs present:

```sh
python tests/tools/entity/bounds/compare_bounds.py --native build/mc_entity_bounds_values_probe --output build/bounds-original
```

Use the `.exe` probe name on Windows and a fresh output directory for each run.
Java compiles and executes only the reference observer; the native bounds probe
and game runtime remain C.
