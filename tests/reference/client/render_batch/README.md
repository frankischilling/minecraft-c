# Whole TG batching and DK list leaves

`finite.json` contains78 CPU observations and17 observations from a real
LWJGL Display. Every targeted original method is unchanged. TG objects run
their real constructor and keep the same Ge direct IntBuffer. DK leaf inputs
are explicitly prepared actual RAW original DK instances; this fixture does
not claim DK, World or N construction.

The CPU rows cover the whole short circuits in `dk.a(I)I` and `dk.e()Z`, all
two-pass flag combinations, invalid pass indices, null pass arrays and wrapped
list-ID addition. A hidden DK returns-1 before even a null array/invalid index;
an uncompiled DK reports nonempty before touching its array. Null/index
exceptions remain separate from ordinary return values.

TG CPU rows cover its real65536-entry constructor buffer, configure and match,
relative append, reset without clearing, separately rounded d2f fields,
negative zero, underflow, infinities/NaN, empty flip, zero-limit overflow, and
null-buffer failure order. Configure stores h before its clear and leaves i
unchanged. Reset changes only h/i. Scratch source fields and all buffer cursors
remain directly observable after exceptions.

The original device profile creates a real64x64 LWJGL Display and four genuine
committed GL lists containing color/translation commands. It executes whole
nonempty TG draws, repeated replay, auto-draw on a full two-entry prepared
buffer, subsequent overwrite, suffix-only replay, a full buffer with i already
true, configure preserving i, and actual DK.d() calling C+2. It records the
actual source fields, full two-cell storage, position/limit, current GL color
and all16 modelview floats. List IDs are normalized relative to the four real
generated names. These rows compare exactly because Push/Pop restores the
prepared modelview unchanged; the direct bounds-list translation uses exact
binary fractions. They do not establish arbitrary native-driver matrix
arithmetic equality.

Each device observation queries current color before reading modelview, in the
same order as `BatchObservation.row`. Native replay uses the explicit source
getter at that point. Passive draw-state inspection alone would miss Mesa's
pending Color commands. The getter preserves unused output-buffer words.

Notably, auto-draw does not clear the source buffer. Its first draw flips and
sets i=true; GL reads position..limit without advancing either. A later append
overwrites the first cell, while a later draw reads only the remaining suffix.
A second full buffer with i=true is not flipped and therefore does not draw.
Configure clears position/limit but deliberately preserves that same i flag.

`capture_reference.py` pins the JAR, method/class Code identities, matching
JDK17 binaries, four SDK metadata JARs and the SDK native archive. Only the
pinned lwjgl64.dll and reached JDK AWT libraries may load; all network and
helper writes are denied. The CPU phase is checked to perform no native load.
The receipt hashes all inputs/helpers before and after and records the exact
native links. Two independent captures produced identical finite bytes.

`mc_test_render_batch` uses a genuine allocated N graph/arena, actual TG
constructors and the canonical Ge/NW class namespace. Its DK source fields are
prepared on a genuine allocated RAW DK. The CPU replay needs no window or
assets. `--device` creates the actual native Display and genuine command-list
range and reproduces the original device rows. No detached batch map, copied
IntBuffer, source-method override, inspection renderer or imported resource is
used. The source TG allocations remain65536 entries; the small replacement
buffer is an explicitly prepared input from real Ge.d, owned by the same arena.

Additional native controls verify strict-FP refusal at configure31 only after
h, clear and a/b/c stores. A terminal undefined-list control fails at TG78
after a successful Push, TG translation and first defined list. It retains
the transformed matrix, first-list color, flipped buffer, i=true and native
matrix_pushed evidence, and records the nested append20 caller. It does not
execute the source Pop or roll back those effects. Undefined GL names are an
explicit native frontier; this is not relabeled as an original Java exception.
Only after those failure-prefix checks does the diagnostic query the retained
color. It checks all four components and then the committed passive ledger.
Final graph/context disposal consumes native storage without crediting source
cleanup. The new TG diagnostic source_pc/matrix_pushed fields are native arena
overhead, rather than source fields or an unchanged source allocation charge.

The guarded CMake fragment registers `client.render_batch_cpu`; opt-in
`MC_ENABLE_CLIENT_DISPLAY_TESTS` also registers `client.render_batch_device`.
The native runner hashes its executable/reference inputs and compares every
row. These controls qualify the entered TG/DK leaves and real list replay
state; they do not claim a whole N terrain submission, rendered terrain pixels
or a complete World frame.

Private original capture:

```text
python -B tests/reference/client/render_batch/capture_reference.py --client-jar <original.jar> --library-root <SDK-libraries> --java <JDK17-java> --javac <JDK17-javac> --output <new-private-directory>
```

Native replay, optionally adding `--device`:

```text
python -B tests/reference/client/render_batch/run_native.py --executable <mc_test_render_batch> --work-dir <private-work-directory>
```
