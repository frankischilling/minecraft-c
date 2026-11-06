# TileEntityRenderer null-World callback reference

The unchanged original completed both profiles on JDK17: 251 observed rows.
`world-finite.json` contains the captured rows; `world-provenance.json` records
the source, tool and helper hashes. Constructor/lookup observations remain in
their separate fixtures.

`TileEntityRendererWorldObservation.java` runs two mandatory fresh JVM profiles:

- `callbacks`: invoke unchanged `LL.a(fd)` with null three times, retaining
  every actual HY.b CV object to compare it with the immediate previous and
  all older objects. Read all 65 original declared CV fields after each call,
  including static a, raw float bits and the null XP field. Observe the same
  PO/AG objects, their model/Map identities, JE backpointers and LL fields.
  Then invoke the all-null-reference frame bind and observe its failure.
- `frame_null_cold`: invoke that same frame bind before any World callback.
  HY.b must stay null and the initialization log must show that CV stayed cold.

Only LL's h/i/j/k/l primitive fields are seeded, with declared raw words in the
capture report. Frame partial is binary32 0.5 and all four reference operands
are null. No World, Engine, Font, Living, Entity or TileEntity object is
fabricated or constructed. The actual NPE and Java stack line are retained;
PC32 is a separate unchanged-Code authority mapping, not a measured stack BCI.

Actual unsorted Map values iteration is preserved as diagnostic rows. Reading
that order does not instrument or independently measure individual virtual
callbacks. The pinned LL/HY/JE bodies establish dispatch and no-op callback
semantics; same-object and child-state rows record their visible outcomes.

`world-source-pins.json` binds the original whole classes and selected Code
attributes, including LL World/frame methods, HY/JE World callbacks, and the
CV initializer/XP constructor. The observer rejects all native links, network,
filesystem mutations and subprocesses. SDK archives supply type metadata only.
There are no assets, device calls, World allocations, nonnull World binding,
nonnull Living interpolation, renderer draws or whole-frame claims.

To repeat the capture, use matching JDK17 tools and the same pinned original JAR
and four SDK archives as the existing constructor capture:

```powershell
python tests/reference/client/tile_entity_renderer/capture_world.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/java.exe' `
  --javac 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/javac.exe' `
  --output .local/tile-entity-renderer-world-original-20261030a
```

The parent capture pins inputs/helpers before and after, requires both complete
profiles and normal zero-external-work markers, bounds each process to 60
seconds, preserves raw diagnostics, and emits `world-finite.json` only from
genuine observed rows. The recorded capture qualifies the original behavior;
native comparison is a separate check. `run_world.py` matches all 251 rows on
Windows MSVC Debug and Linux Clang18 ASan/UBSan. Each profile projects only its
original Java stack line75 to independently pinned PC32. Separate authored
controls check retained CV lifetime, bounded replacement and failed-initializer
latching. Nonnull native World lifetime controls do not establish original
nonnull World-construction or callback parity.
