# Finite World Player publication reference

Run the optional comparison with the supplied private client JAR and explicit
matching JDK17 tools:

```text
python -B tests/reference/world/player_publication/compare.py
  --client-jar /private/b1.7.3.jar
  --native /build/mc_test_world_player_publication
  --java /jdk17/bin/java --javac /jdk17/bin/javac
  --work-dir /ignored/world-player-publication
```

Use quoted `.exe` paths on Windows. `--source-root` selects a matching native
source tree when it is not the nearest repository ancestor. Rebuild every ABI
consumer together; do not link these World/error headers against old archives.
The regular authored-only test needs no JAR:

```text
python -B tests/tools/world/test_player_publication.py
  --executable /build/mc_test_world_player_publication
  --work-dir /ignored/world-player-publication-contracts
```

Every run creates a fresh child directory. Compiled helpers, original/native
stdout/stderr, class-init logs, extracted resources, native Worlds and reports
stay private there. No captured answers, original classfiles or assets are
distributed. The reference runner extracts only three pinned private resources:
`lang/en_US.lang`, `lang/stats_US.lang` and `achievement/map.txt`.

`source-pins.json` identifies the exact JAR, ten original classes and selected
Code bodies/handlers, all eighteen C/H increment files, the authored execution
sources and compiled helper class. The selected Code hashes were parsed directly
from that JAR; whole class hashes bind their exact bodies and attributes. The
helper verifies the actual loaded fd code-source JAR digest. Its freshly empty
classpath contains one authored helper class plus the validated JAR. Ambient
Java options are rejected and CLASSPATH is removed. JVM class initialization
must include the real Player/World/cache/Chunk chain and exclude client, graphics
and audio bootstrap. Neither helper nor runner rewrites original classes.

The original constructs `fd(new wy(), ..., xa.a(0), 5143L)`, sets spawn through
the source public operation and constructs actual `dc(null, world, gr, 0)`.
It calls real attachment with a null Info.Player tag, repeats publication of
the same Player, then registers a genuine null WorldAccess with `fd.a(pm)` and
observes the real callback NPE after all publications. Reflection reads actual
fields/lists; it assigns none. No Unsafe, fabricated World, replacement provider,
fake callback class, entropy input or source-method replacement is used.

All three projections compare actual World `d` and `b` counts, sleeping flag J,
same-identity Chunk-slice occurrences and Entity membership coordinates/flag.
Original banners remain `16 achievements` then `151 recipes`. The native test's
two concrete fixtures retain their own `151 recipes`/`16 achievements` pairs.
Only `splitlines()` handles line terminators. Both processes must exit0 with
empty stderr; a positive final native check count is required. A failure or
sanitizer diagnostic cannot be counted as passing source rejection.

Native controls additionally exercise real callbacks/reentry, a zero provider
budget, source sleeping cells, malformed/full-base NBT failure prefixes and
terminal ownership release. They do not establish source bed-entry, renderer
WorldAccess bodies, Player NBT completion, ticks, input, rendering, networking,
audio or full-startup parity. The native attachment API reports explicit errors;
it does not emulate the source wrapper's generic printStackTrace behavior.
