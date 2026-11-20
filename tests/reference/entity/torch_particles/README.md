# Smoke and Flame finite reference

This packet records unchanged client `xn` (Smoke), `qu` (Flame), their real
`xw`/`sn` ancestors, real World brightness queries and CPU `nw` sprite writes.
The original archive is pinned in `source-code.json`. Authored helpers use genuine
World, provider, Chunk, particle and tessellator constructors. They deny network,
native-library loading, writes and process execution; five denial controls run
after all game calls. The debugger reads fields, arguments, returns, frames and
live bytecode. It never invokes game methods or writes target state.

Two ordinary VMs and a third read-only JDI VM completed the frozen 36 profiles
in 118.88 seconds on the recorded Windows JDK17 environment. Each also constructs
one genuine warm default Smoke before reading shared Math state. The 30 component
rows agree exactly across the three VMs: 12 ticks, eight successful draws, four
draw failures and six Flame brightness calls. Each VM checks every backing word
outside the reached append range against that same owner's prior contents:
25,165,440 unchanged words. Successful sprites contain all48 words, including
the real quad-to-triangle duplication and untouched normal/padding words.

Every constructor uses its actual shared Math and private Random stream. The
ordinary audit explicitly infers private initial states by reversing the pinned
Float count; JDI independently reads each initial state before the first actual
Float call. It observes 242 Math doubles, 250 private Floats and 126 constructor
method entries across37 roots. Random values differ between VMs and are audited
within their own VM. No seed, Random binding or constructor sample is supplied.
The default Smoke constructor's real6D-to6DF delegation is retained. Flame's six
extra private Floats change only local constructor coordinates; published Entity
position remains the original input.

Component fields are declared in `profiles.tsv` and `declared-inputs.json` after
the actual constructor returns. They distinguish mutable inherited `g` from
subtype `a`, exercise Smoke's growth clamp and scale2.5 constructor, Flame's
unclamped render size and clamped brightness, age postincrement/wrap, expiry,
ground damping, Smoke's unchanged-Y boost, zero lifetime and null World/NW.
All tick rows explicitly use no-clip movement. They qualify the real `sn.b` and
`eq.d` arithmetic, without claiming collision parity. Each draw starts a fresh
genuine2M-word `nw` and one actual begin; no draw/GL body is replaced.

JDI records1,173 events and24 live Code fingerprints, including11 actual mover,
32 packed-vertex and18 inherited brightness calls. Source failures are exact:
Smoke integer division at60, inherited brightness atSN118, and null tessellator
color atXW150. Flame's nested brightness frame atQU32 remains in the trace.
Three internally caught JDK signature/disabled-algorithm exceptions during the
first Block bootstrap are retained separately with their actual frames/catches.

`provenance.json` contains sanitized process receipts, stable before/after input
and helper hashes, transport hashes, per-VM semantic hashes and prior failures.
Attempt1 exposed an invalid second begin in the probe. Attempt2 completed all
targets, but its first validator rejected the caught JDK verification exceptions.
Both attempts and their producer bytes remain private. The corrected, frozen
attempt3 supplied this packet. Selected journals are parsed and published as
explicit LF JSON data; raw CRLF transport hashes remain separately recorded.
No original class, asset, JAR or JDK file is published.

Run the packet controls with:

```text
python -B -m unittest discover -s tests/reference/entity/torch_particles -p test_packet.py
```

To capture fresh evidence, provide your own pinned client archive and libraries:

```text
python -B tests/reference/entity/torch_particles/capture_reference.py --jar CLIENT.jar --libraries LIBRARIES --output NEW_DIRECTORY
```

The capture refuses an existing output directory and retains failures. It freezes
producer, helper, archive, dependency and JDK bytes before execution and verifies
them afterward. Native replay runs without Java or original assets, using the
existing authored resource fixture. It compares deterministic rows exactly and
audits each native constructor's independent entropy. Native admission, retained
owner cleanup and strict floating-control rejection are separate controls.
This packet makes no GL, pixel, collision, entropy-distribution or whole-game
claim.
