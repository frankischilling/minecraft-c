# Redstone World renderer reference

This packet records direct `CV.g(Block,III)` calls from the supplied Beta 1.7.3
client, SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The original classes and method bodies remain unchanged. The authored Java
helpers construct real saved Worlds, canonical Chunks, EW caches, CV objects
and the original NW singleton with its 2,097,152-word constructor backing.

Each cohort has two fresh ordinary targets and a third fresh target observed
through read-only JDI. Their complete target stdout bytes agree within that
cohort. The five cohorts have independent Worlds and NW batches; their rows do
not form one continuous 100-call batch.

| Cohort | Calls | Declared inputs |
| --- | ---: | --- |
| `flat` | 56 | All 16 floor connection masks at powers 0, 7 and 15; eight texture, override and light variants |
| `vertical` | 20 | Four lower routes, four upright routes, blocked upper routes, upper capability distinctions and simultaneous walls |
| `capability` | 10 | Both repeaters at all four orientations, a pressure plate and Stone |
| `signed` | 8 | Isolated floor and West upright at four signed float precision boundaries; twelve saved request slots, ten returned owners and genuine matching caches |
| `nullable` | 6 | Null cache/Block combinations, override before a null Block failure, and two actual Stone material failures |

The public captures contain 17,496 JDI events, 3,415 EW query entries, 2,113
static field reads and six source exceptions. Each cohort retains its own
live method Code fingerprints. Duplicate method fingerprints across cohorts
are separate observations, not additional methods.

The signed cohort also retains `owners.tsv`, an actual original receipt of each
request, returned Chunk coordinates and first equal owner slot. The source
packed keys alias request slots 10 and 11 to owners 0 and 1. Native preparation
uses the same requested slots and retires each unique owner once. It preserves
the returned coordinates and the original cache-key behavior.

`declared-inputs.json` and `profiles.txt` specify every authored operand.
Declarations retain their frozen preparation-time status; execution results
belong to `provenance.json`.
Preparation changes voxel/metadata/light bytes before invocation, the declared
CV override and selected mutable Block inputs. The nullable helper restores the
actual Stone material in `finally`; its null value also affects every other use
of that same registered Stone object. No source clock, phase, input queue,
Random stream, NW cursor, backing reference or drawing state is repaired.

Each successful body leaves the batch open. It appends two floor quads and up
to eight upright quads. The helpers compare all non-emitted backing words
against their own prior memory, preserve all NIO identities and state, and
record primitive CV/NW fields, Block bounds and emitted words. The nullable
rows retain their reached color prefixes even when no vertex is appended.

`original.stdout` preserves the exact captured CRLF bytes. `selected.txt` is
the explicitly declared ASCII LF projection. `events.jsonl` contains actual
entry, return, static-read and exception observations; `live-code.tsv` binds
the reached methods to the original JAR. `provenance.json` records portable
input/tool/helper pins and capture receipts. `manifest.json` closes the file
roster and is independently pinned by `packet.py`.

## Reproduction

Integrity and semantic checks need Python, without Java, imported assets or GL:

```sh
python -B -m unittest discover -s tests/reference/client/redstone_world -p test_packet.py
```

To repeat an original cohort, supply the verified client and its qualified
libraries with Temurin JDK 17.0.17+10. Use a new output directory:

```sh
python -B tests/reference/client/redstone_world/flat/capture_reference.py \
  --execute --jar /path/to/b1.7.3.jar --libraries /path/to/libraries \
  --output /path/to/new-capture
```

Substitute another cohort directory to run its declared inputs. Ordinary target
subprocesses have a 90-second limit. The JDI observer has a 150-second internal
deadline and its parent subprocess has a 180-second limit, with 20,000 events
and a 16 MiB journal cap. These are observation bounds, not source work budgets.

Native CPU replay uses the built fixture and synthetic saved inputs:

```sh
ctest --test-dir build --output-on-failure -R 'redstone_world'
```

Native replay compares literal source effect rows and audits its own backing
and cleanup. The original Java external-effect denial receipts remain source
evidence. A separate native-only mode checks captured XP arguments, fresh later
receiver reads, and the distinct World/EW solid-support routes. Its cold
callback cases deliberately pair a ready World Registry's Block with a separate
cold code Registry. This constructed internal split-context stress does not
establish an ordinary matching-owner CV call or original Java cross-loader
behavior. It preserves all constructor output and flushes only its own fixture
receipt between callback cases.

These observations qualify the declared CPU bodies. They do not compare a
native per-query trace, GL submission, original frame pixels, Item placement,
power propagation, audio, frame timing or unrestricted gameplay.
