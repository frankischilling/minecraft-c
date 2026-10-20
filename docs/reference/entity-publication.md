# Original Entity publication observations

Authority is the unchanged [client and server archives](binaries.md). The
project-owned observer is
[`WorldEntityPublicationObservation.java`](../../tests/reference/world/WorldEntityPublicationObservation.java).
It reuses the unchanged setup and read-only Context adapters from
`MutationObservation` and `SolidSupportObservation`. Original classes load from
the supplied archive without bytecode changes.

| Operation | Client | Base server | Relevant order |
| --- | --- | --- | --- |
| World insertion | `fd.b(Lsn;)Z` | `dj.b(Llq;)Z` | Live coordinates 1/13; presence 45; acquisition 83; Chunk add 87; World.b add 95; observer 103 |
| Base observer hook | `fd.c(Lsn;)V` | `dj.c(Llq;)V` | u 4/size 7; reload 16/get 20/cast 25/invoke 29; return 40 |
| Normal Chunk add | `lm.a(Lsn;)V` | `hi.a(Llq;)V` | q 2; X/Z 6/18; Y 74; membership 115/123/129/137; list append 148 |
| Blank Chunk add | `li.a(Lsn;)V` | `hg.a(Llq;)V` | Literal return 0, including NULL |
| Full ItemEntity constructor | `hl(fd,DDD,iz)` | `ez(dj,DDD,fy)` | Genuine base/position/Stack and Random/Math operations |
| Numeric Stack constructor | `iz(III)` | `fy(III)` | Actual ID/count/damage stores |

The finite experiment constructs two genuine Worlds in the same original class
loader. Positions, nullable Stack and World references are constructor inputs.
It never prepares Entity membership, Chunk.q, cache contents or source lists by
field writes. It records actual source outcomes and leaves all resulting state
intact through JVM exit.

Both sides completed 21 reflected subject calls, 44 full snapshots and 88 raw
Context records. Both acquired genuine present normal Chunk substrates and
recorded cross-World calls. The sequence includes ordinary false for an absent
nonplayer, NULL failure, normal NULL prefix, repeated World publication, low/high
Y constructors, direct normal duplicates, direct Blank calls and repeated
publication of the same Entity into the second World.

Each snapshot retains full declared World, Entity, ItemEntity, AABB, watcher and
Stack fields with raw-width primitive bits. It includes every World.b occurrence,
all eight Chunk lists and complete bytes for all five Chunk arrays. Full images
cover actual fallback, cache-map values, insertion-only identities and returned
receivers. The client capture contains 128 Chunk images and 10,518,528 array bytes;
the server contains 176 images and 14,462,976 array bytes. Context-local identities
remain unchanged and bridge to global identities through actual object equality.
The parser verifies Context/image array digests and ordered Entity occurrences.

World and Entity Random states retain their actual seed owners and Gaussian
cache bits. Shared Math and the Entity counter remain explicitly unobserved
before a genuine ItemEntity constructor returns. No stream is reseeded, consumed
for calibration or reconstructed from clocks.

The recorded run used JDK 17.0.17, interpreter mode and a 1 GiB heap. Actual archive,
three helper sources, selected JDK files and 23 compiled helper classes matched
before and after execution. Both version commands, compilation and two subject
JVMs exited naturally with zero status. The strict parser rejects 12 framing,
count, Context hash/length, array length and primitive-width corruptions per side.
Retained immediate source outcomes match the final protocol records; their
numeric stderr indices are also checked separately during artifact review.

Reproduce compilation with the three sources in an owned directory:

```text
javac -encoding UTF-8 --release 17 -d classes MutationObservation.java SolidSupportObservation.java WorldEntityPublicationObservation.java
java -Xint -Xmx1g --add-opens java.base/java.util=ALL-UNNAMED --add-opens java.base/java.lang=ALL-UNNAMED -cp classes WorldEntityPublicationObservation <original-jar> client <absent-world-A> <absent-world-B>
```

Use `server` and the server archive for the other side. Both absent roots need
existing parents. Exit 77 means unavailable genuine setup; exit 1 means failed
measurement. A finite capture can explicitly skip normal/cross-World cases when
no genuine substrate is available, so inspect those status fields before claiming
coverage. Original stdout is redirected to stderr; stdout contains one ASCII
protocol object followed by LF. Keep worlds, captures, archives and compiled
helpers outside public artifacts.

This is original-only evidence. It does not replay the state through C or prove
native control equivalence, original removal/unload, nonempty observers,
dedicated tracking, player behavior, Entity persistence, fluid completion or
population acceptance. See the separate [native ownership contract](../world/entities.md).
