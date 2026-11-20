# Lever placement, activation and lifecycle observations

Two fresh ordinary JDK17 processes execute 244 finite calls against the supplied,
unchanged Beta 1.7.3 client. `xr` is the original Lever Block69. Genuine World,
Chunks and Block instances execute the original methods; the authored `pm`
listener records callbacks without replacing game bodies. No GL, networking,
process execution or native loading is used. The helper permits writes only
inside its private authored World.

The TSV defines general and face support, World placement, placed-face handling,
activation, left click, neighbor support loss and removal. Inputs include all
sixteen metadata nibbles, both remote flags, supported and missing attachments,
NULL World exceptions and three outside-World read-only coordinates. Player is
NULL and unused by the entered Lever methods. Every call records all changes
across five arrays of four canonical Chunks, full ordered light queues, callback
arguments, published Item fields and actual Random states. Declared arrays,
minimum height and populated flag reset before each call. Queue, actor, Random,
clock and source phases remain continuous.

`ordinary-*.stdout` preserve the exact original CRLF byte streams. Each contains
all 244 LEVER/ENTROPY pairs and supporting terminal guards. `original.stdout`
selects process zero's entire literal LEVER/COMPLETE projection using LF;
`ordinary-*.entropy.jsonl` retain every own-process entropy observation using LF.
No observed field is silently normalized.

Floor placement consumes the actual default-constructed World Random. Each
process's two reached `nextInt(2)` calls independently choose orientation5 or6.
The audit checks those choices against that process's observed state. Only the
242 other full literal records are compared between original processes. This
packet does not claim cross-process orientation or default entropy equality.
The native replay audits its own actual Random stream without supplying the
original seed. It enters 211 bodies, omitting the 33 original left-click wrapper
calls until a dedicated native wrapper replay exists. Of these, 209 complete
literal effect records compare exactly. Two floor-placement records instead
check their complete array changes and callback against the independently
audited native orientation choice. Raw original and native rows stay unchanged.
Native left-click controller coverage remains separate from this comparison.

Each original process publishes 52 actual Lever Items. Own-stream audits check
208 World float steps, two World int steps and 416 Math LCG steps. The first
Math prestate is unobserved; poststate permits a formula audit only. Entity
Random seeds remain actual independent observations, with no cross-engine seed
identity claim. Constructor clock and default seed selection are outside scope.

`source-pins.json` identifies the archive, classes, method descriptors and Code
hashes, including JDK entropy bodies. The capture receipt freezes authored and
external inputs, compiled helpers and both Worlds. Only the source session lock
changes on disk. The first private attempt remains failed: a declared x/z=-1
read generated an additional Region. The fresh successful cohort uses declared
outside-World coordinates; neither failed observations nor original behavior
were altered.

Run public packet controls without Java or original assets:

```sh
python -B -m unittest discover -s tests/reference/world/lever_placement -p test_packet.py
```

The Windows MSVC Release native comparison passes all 211 entered bodies,
52 own-audited drops and 49 comparator corruption controls. Its six public
packet tests pass too. These results concern this component workload; Linux
runtime qualification requires its own run.

With testing enabled, build `mc_test_lever_placement_reference`, then run
`ctest --test-dir build-windows --output-on-failure -R
'^(world[.]lever_placement_reference|tools[.]lever_placement_packet)$'`.

Repeat local original observation with the supplied archive and pinned launcher
libraries using `capture_reference.py --jar ... --libraries ... --jdk ...
--output <new-private-directory>`. Capture always creates a new output directory.
This finite component packet does not qualify loaded gameplay, original pixels,
audio playback, arbitrary circuits or FPS.
