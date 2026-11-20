# TileEntityRenderer frame arithmetic reference

The unchanged original completed all 24 cases on JDK17: 97 observed rows.
`frame-finite.json` retains the raw rows, and `frame-provenance.json` records
the source, tool, helper and input hashes. The existing constructor/lookup and
null-World observations remain separate.

The bytecode constructor chain `wh -> bg -> ii -> ls -> sn` admits an attempted
genuine `wh(fd=null)` without an authored subclass or fabricated reference.
Inspected reached bodies construct a real AABB, ordinary Random and Watcher,
register Pig watcher data, perform four LS Math.random calls, and assign local
fields; they do not dereference the World operand. The helper still requires
that actual original constructor to return. An actual failure is recorded and
stops capture before arithmetic; no fake World or bypass substitutes for it.
The existing `entity/view` evidence independently used a real `ls(null)`
superclass constructor through an authored concrete subclass; this helper uses
the original concrete Pig itself.

`frame-inputs.json` contains 24 authored inputs and no expected outputs:
partial0/.5/1/negative-zero, adjacent rounding values, signed-zero operands,
overflowing subtraction at endpoint partials, distinct left/right/both NaN
payloads, partial NaNs/infinities, subnormals and ordered-rounding operands.
Only the ten SN pose fields read by LL and five LL scalar sentinels are seeded.
Declared operand bits and independently reread stored field bits are separate
rows, including signaling-NaN representations. Arithmetic and narrowing execute
only in the unchanged LL frame method. All three Engine/Font/World arguments
are null, and the actual Pig is the nonnull Living operand. No World callback
should be reached because LL.f and its argument are the same null reference.

Every whole frame records raw h/i/j/k/l values, actual reference identities,
and whether it changed Random's shared uniquifier, either actual Random state,
the Entity ID counter, pose fields, unrelated LL state or Watcher entry count.
The observer reads the actual Math Random only after the original LS constructor
has reached it. Nondeterministic Random seeds are compared before/after each
frame and are not fixture answers. The constructor row records genuine Entity
ID/counter and Watcher-size outcomes separately from arithmetic.

`frame-source-pins.json` binds31 whole original classes and selected Code
attributes for the constructor chain, its reached local children and LL frame
method. The parent pins archives, helpers, inputs and JDK17 tools before/after;
the Java guard rejects native links, network, writes/deletes and subprocesses.
Initialization logs must show the real constructor chain and Math holder while
World, Engine, Font, CV, TileEntity and SDK classes remain uninitialized.

To repeat the capture:

```powershell
python tests/reference/client/tile_entity_renderer/capture_frame.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/java.exe' `
  --javac 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/javac.exe' `
  --output .local/tile-entity-renderer-frame-original-20261030a
```

One fresh JVM must complete all24 frames and97 rows within60seconds before an
observed `frame-finite.json` is emitted. No expected arithmetic formula, NaN
normalization, tolerance or profile skip is used. This does not qualify native
Pig construction, nonnull World changes, assets, devices, rendering or complete
world frames. Native arithmetic comparison is a separate check; the genuine
original Pig constructor row is a reference prerequisite, with no native
Pig-constructor parity claim.

Native replay matches all 96 method rows from all 24 cases on Windows MSVC
Debug and Linux Clang18 ASan/UBSan. It uses a genuine RAW LocalPlayer (`dc`),
with the common Living/Entity pose fields set to the separately observed stored
input words. The genuine original Pig (`wh`) constructor remains original-only.
Runtime Class and constructor equivalence, reflection setter parity, and native
absent Random/Math/Watcher state versus the original instantiated objects are
explicitly uncredited in `run.json`. Only input rows reach the executable;
expected result rows stay in the comparison parent.

Two separate native controls check late-component numeric refusal and strict
floating-control refusal. They preserve completed stores without claiming
original equality for an unsupported input. An operation with two NaNs and at
least one signaling operand remains explicitly unsupported. Floating exception
flags are outside the measured value comparison.
