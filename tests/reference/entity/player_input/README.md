# Finite original LocalPlayer input comparison

The optional comparison needs the supplied private client JAR and matching
explicit JDK17 executables:

```text
python -B tests/reference/entity/player_input/compare.py
  --client-jar /private/b1.7.3.jar
  --native /build/mc_test_player_input
  --java /jdk17/bin/java --javac /jdk17/bin/javac
  --work-dir /ignored/player-input-reference
```

Use quoted `.exe` paths on Windows. `--source-root` selects a matching complete
native source tree when it is not the nearest repository ancestor. Rebuild all
input/Player ABI consumers together. The authored-only public test needs no JAR:

```text
python -B tests/tools/entity/test_player_input.py
  --executable /build/mc_test_player_input
  --work-dir /ignored/player-input-contracts
```

Each run uses a new child directory. Compiled helpers, extracted private
resources, Worlds, class-init logs, stdout/stderr and reports stay private.
No captured answers, original classes or assets are distributed. Only the
three pinned language/statistics/achievement resources are extracted by the
original runner for the native Registry initialization.

`source-pins.json` binds the exact JAR, eleven actual class identities, twenty
selected original Code bodies/attributes, nine native C/H increment files,
the two authored execution sources and the compiled helper. Whole class hashes
bind the selected bodies. The helper checks the loaded dc code-source JAR.
The runner compiles exactly one authored helper class into a freshly empty
directory and runs it with only that directory plus the verified JAR. It rejects
ambient Java options, clears CLASSPATH, checks explicit same-directory JDK17
tools, verifies required original class initialization and excludes Minecraft,
graphics and audio bootstrap. Neither helper nor runner replaces original code.

The original constructs a genuine direct-metadata World and two real completed
`dc(null, world, gr, 0)` Players. Their input starts null. Authored public input
field assignments then exercise actual base/options inputs, same/shared/replaced
identities, native-order key/reset forwarding, stored sneaking, current settings
rebinding, null input and nested null settings/binding errors. Reflection only
reads the actual private ten-cell input array. A public sleeping-cell assignment
is an explicit authored input; it does not establish bed-entry gameplay.

The twenty-one projections compare source public float bits, three booleans,
kind/nullness, all ten key-cell bits, sleeping and the actual `dc.t()` result.
Source recipe/achievement banners remain in their actual order. Only
`splitlines()` handles line terminators. Original and native must both exit0
with empty stderr. A positive native check count is required; an exception,
sanitizer diagnostic or crash cannot stand in for successful source rejection.

Native tests additionally qualify counted lifetime across caller/Player holds,
replacement failure at a native counter bound, RAW/failed receiver prefixes and
final disposal. These controls do not claim Java GC/refcount equivalence or
original execution on fabricated uninitialized receivers. GameSettings remains
borrowed until the final input hold, with its existing exclusive-live contract.

No Unsafe, fabricated World/Player/Minecraft/controller, supplied constructor
entropy, substituted source method, full Minecraft attachment flow, GUI,
Player/Living/World tick, input event route, movement or renderer is included.
The source store at Minecraft PC205 is identified as authority for the direct
field operation; the helper does not execute that complete application method.
