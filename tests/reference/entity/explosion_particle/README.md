# Explosion particles

The original packet executes genuine `gy` constructors, `gy.w_()` ticks and
the inherited `xw.a(nw,FFFFFF)` sprite body. Its supplied client archive is
identified by SHA-256 in `source-pins.json`. Game classes and methods remain
unchanged. The helper uses actual constructed Worlds, Chunks, particles and NW
buffers; it defines no game subclass and allocates no uninitialized game object.

Seven tick scenes declare particle state after construction. They cover ages
0, 7, 8, -1 and `Integer.MAX_VALUE`, lifetime zero and the on-ground damping
branch. All use the genuine `sn.b(DDD)` no-clip body with an actual bounds
object. The zero-lifetime call retains the original arithmetic failure prefix.
These scenes isolate GY's postincrement, animation, upward acceleration and
damping. They do not establish ordinary World collision behavior.

Six draw scenes declare texture, scale, color, camera axes, interpolation and
origin inputs. An actual original World/Chunk supplies brightness 1 through
normal `sn.a(float)` queries; both Chunk light arrays are full. Each draw uses
a fresh actual NW object constructed through its original private constructor.
The NW default quad expansion remains enabled: four source vertices produce
six packed vertices and all 48 backing words are compared, including reserved
slots. Texture selectors 0, 7, 16, -1 and 256, negative scale, nontrivial axes
and partial ticks outside zero-to-one retain their original arithmetic.
No OpenGL context or native graphics library is used by this CPU packet.

Constructor entropy is kept separate from deterministic component comparisons.
The ordinary process records thirteen actual constructor results after one
genuine warm invocation initializes shared Math in source order. Its private
initial state is explicitly inferred by reversing eight source-pinned Float
advances. A separate read-only JDI run directly observes fourteen constructors,
including the warm call, with 112 shared Math returns, 112 private Float
returns and fourteen live GY constructor Code hashes. Each process is audited
against the bytecode formulas and its own stream progress. Their constructor
values are never forced equal, and neither shared Math nor constructor entropy
is rebound. The debugger invokes no target methods and writes no target fields.

`finite.json` contains the seven tick and six draw comparisons.
`constructor-evidence.json` retains the independent original stream observations.
The native tests compare deterministic rows in their normal control executions;
their own constructor controls audit their actual entropy context separately.
This packet does not claim whole-session, complete constructor-graph, graphics
driver or pixel equality.

Capture with JDK 17 and user-supplied original libraries:

```powershell
python -B tests/reference/entity/explosion_particle/capture_reference.py `
  --jar b1.7.3.jar `
  --libraries "$env:APPDATA/.minecraft/libraries" `
  --output .local/explosion-original
```

The output directory must be new. The controller retains ordinary/JDI process
output and verifies archive, tools, modules, libraries and authored helpers
before and after execution. Network, native-library loading and file writes
are denied in each target process; the external controller writes the reports.

```sh
python -B tests/reference/entity/explosion_particle/compare.py --mode ticks --stdout tick-stdout.txt
python -B tests/reference/entity/explosion_particle/compare.py --mode draws --stdout draw-stdout.txt
python -B tests/reference/entity/explosion_particle/test_packet.py
```
