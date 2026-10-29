# PX picking observations

[PickerObservation.java](PickerObservation.java) calls unchanged original
`px.a(F)` for the 27 authored cases in [finite.json](finite.json).
[Provenance](provenance.json) records the pinned client JAR, class and method
hashes, SDK metadata, preparation scope and original capture hashes.
The original JAR SHA256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The selected PX Code has 453 bytes and SHA256
`4cd5aaca219c77db008832d92b3360ae0022cadfbe4da37bb310e1afc025c66e`.

The observer constructs real SaveHandler, World, four Chunks, two LocalPlayers,
Minecart, FallingSand, Item and OS/PJ controllers. Only the App subclass `kq`
and PX receiver use ReflectionFactory Object-only allocation; their constructor
bodies do not run. The probe authors their reachable fields and each case's
block arrays, positions and membership through actual World insertion. World
query order, ray traversal, pickability, geometry and picker bodies execute
unchanged. This qualifies the prepared picker body, without qualifying App/PX
construction, a default Session, a rendered frame or an online account.

The cases cover empty rays; Block range and occlusion; concrete Entity hits;
excluded self; dead and nonpickable Entities; equal-distance ties; duplicate
query identities; inside-box ordering; the OS Block4/Entity3 ranges; partial0.5;
and early null-camera/null-World returns. Equal nonzero ties retain the first
Entity. Inside-box cases expose the source zero-distance sentinel: inside A
followed by farther B selects B. ENTITY VF.f copies the selected Entity's current
position. It does not contain the bounds intercept.

Rows record actual actor identities and bounds, World query order, App.y fields,
PX.m, before/after field identity, raw float/double bits, BT cursor and all used
pool slots. Slot identities come from Java `==`. The ray-start slot records
the original World traversal's in-place mutation. Only the process-local
`System.identityHashCode` of the pool list is omitted from the published rows.

The native `client.picking` test compares all 26 OS rows, including pooled
VF/vector aliases. The remaining original `pj_m_without_entity_vf` row is
preserved as an explicit exclusion because the native controller owner
currently represents OS. Native NULL, failed-construction, immutable-error,
retained-call, foreign-hold, source-field replacement and checked-cleanup
controls are separate from the original observations. Regular CTest reads the
committed fixture and needs no original JAR, Java, SDK metadata or display.

To repeat the original observation with JDK17, supply the verified original
JAR and the retained Launcher `libraries` directory:

```text
python tests/reference/client/picking/observe.py --jar b1.7.3.jar --libraries-root <libraries> --work-dir <private-output>
```

The runner authors a disposable four-Chunk World, compiles only the authored
observer, verifies SDK hashes and compares every original row. It installs or
downloads nothing. The observer rejects network connections, listeners, native
library links and writes outside that private World. The recorded original run
used Temurin17.0.17+10 with `-Xverify:all`; it made zero network/native-link
attempts, preserved JAR/SDK and World data, and changed only `session.lock`.
JDK17 emits the expected ReflectionFactory/SecurityManager compatibility and
deprecation warnings. No original classes or decompiled method bodies are
included here.
