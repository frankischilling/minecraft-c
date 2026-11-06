# Original Entity turn comparison

`compare.py` invokes unchanged client `sn.d(FF)V` through real original Item
`hl` and Falling `ju` objects constructed with NULL World. Both classes inherit
that exact body. The native observer likewise allocates and constructs actual
production Item/Falling owners. Neither observer substitutes a Player, World
or READY Entity projection.

The client JAR SHA-256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
`provenance.json` pins complete classfiles and method Code hashes for
`sn/hl/ju/eq/ud/ma`. The 109-byte `sn.d(FF)V` Code SHA-256 is
`d8aefac68a65818cea92e7b95ae7c98762fefba0c70c7c642fcc6bd62078fe1f`.
The complete class hash also binds its double 0.15 constant. The runner rejects
different JAR/class/Code bytes before launching Java.

## Inputs and observation

`inputs.json` contains 3,280 authored cases and 3,330 turn records. Cases include
both receiver kinds, a 33 × 33 current/delta grid around zero/subnormals,
normal values, pitch clamp boundaries, float extremes, infinities and quiet/
signaling NaNs. Previous-angle values and overflow have separate cases, as do
512 reproducible authored six-word binary32 patterns per receiver and
sequences of up to six turns on one actual owner. Inputs do not depend on
observed output.

Each output record binds case/step order and projects four raw angle bit
patterns plus a successful unchanged-state assertion. There is no floating
tolerance, NaN canonicalization or expected-result injection. Only newline
endings are split across platforms. Each observer verifies unrelated Entity
fields and bounds identity/values and the Entity's private Random state.
The native guard also checks constructor diagnostics, class/allocation counts,
watcher state and native ownership bytes. The unrelated Item constructor Math
draw is neither replaced nor part of the compared turn projection.

The Java helper assigns only original public angle fields before measurement.
It uses reflection to observe the actual receivers and private Random state;
`--add-opens=java.base/java.util=ALL-UNNAMED` permits the latter without changing
the Random or game classes. No Random draw is used to inspect it.

Class-init logs must show exactly `sn/hl/ju/eq/ud/ma` among original classes.
The helper does not initialize an original Player, World, Minecraft, renderer
or audio setup. Original classfiles and decompiled source are not distributed
with these helpers.

## Run

Build `tests/entity/movement/turn_probe.c` with the production turn source and
normal simulation dependencies. Its private observation include requires
`src/entity`. Then run with Python 3.10+ and the selected JDK:

```text
python tests/reference/entity/turn/compare.py --jar /path/to/b1.7.3.jar --native /path/to/turn_probe --work-dir /path/to/ignored/reference-output
```

`--java` and `--javac` select explicit tools. The runner removes ambient Java
option variables, enables full verification, creates a fresh run directory,
and retains the exact plan, outputs, class-init log and hashes. Each subprocess
has a 60-second timeout. The native observer consumes only the bounded authored
plan; it is not a general parser.

`validation.json` records the finite results and source pins. Native argument,
phase and FP-admission errors are separate contracts, not original Java
receiver/heap equivalence. Mouse, Player and camera integration are outside
this inherited method comparison.
