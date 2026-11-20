# Lava texture animation

This package compares the unchanged client `cg` constructor and `cg.a()V`
with the concrete native Lava texture owner. Flowing Lava is the actual
`uu.D` Block, with ID10 and texture237. The inherited `aw` constructor
allocates its1024-byte RGBA array before publishing its defaults; `cg` then
allocates four distinct256-element float arrays. A failed native allocation
keeps every completed constructor store and allocation until retirement.

Run with explicit local inputs:

```text
python -B tests/reference/client/lava_animation/compare.py \
 --client-jar <b1.7.3.jar> --native <mc_test_lava_animation> \
 --java <JDK17-bin/java> --javac <same-JDK17-bin/javac> \
 --work-dir <ignored-directory>
```

Animation follows the source's nested x/y loop. Every cell reads nine old
`g` values and four live `i` values, writes `h`, updates `i` and `j`, then
calls the genuine shared Math holder at311. Earlier cells' `i` stores remain
visible to later cells. The two trig calls use the application's existing
initialized `in` table. The `h` store356 precedes `g` store361 when swapping
arrays. Pixel conversion, clamping, integer anaglyph weights and all RGBA
stores retain their source order.

The23 finite cases execute31 animation calls and produce348 ordered records.
They cover evolving waves, anaglyph, gradual underflow, signed zero,
infinities, noncanonical quiet NaNs and null/short arrays. The comparison
retains complete array bits, identities, lengths, RGBA bytes and the actual
Math/Random Gaussian cache and48-bit state. Each completed frame draws256
times. The short-h1 and short-j1 cases draw once before failing; late RGBA
failures keep all256 draws and the completed swaps. Only physical LF/CRLF
record endings differ between hosts.

Fixtures warm the actual bootstrap Math holder and explicitly call its
ordinary `Random.setSeed`. The native fixture sets the corresponding retained
holder state. Neither side replaces a source game method or supplies Random
outputs to animation. Native controls separately exercise cold holder
construction, shared continuation, allocation failure, delayed unsupported-FP
failure, constructor quotas and native lifetime rules. The finite raw-NaN
profile follows the observed JDK17 x86-64 interpreter's right-operand fadd
payload selection; no universal Java NaN payload rule is claimed.

Whole-JAR and complete selected class/Code pins are checked before execution.
The observer binds genuine game classes to that archive and Math/Random to
bootstrap java.base. It compiles to one authored class. Original assets are
extracted privately only after their hashes match. Ambient Java options are
rejected, CLASSPATH is cleared, and initialization logs reject unexpected
application, LWJGL or statistics-worker startup. Bounded capture requires
actual completion, empty reader errors, reaping and exact physical streams.
Controller controls also run under optimized Python without a JAR.

This implements CPU constructor and animation behavior only. Application
`ae` publication, the normal application constructor, `aw.a(ji)`, texture
registration and GPU upload remain separate unfinished operations. Linux
native captures may compare retained Windows original observations; that is
distinct from executing the original on Linux.
