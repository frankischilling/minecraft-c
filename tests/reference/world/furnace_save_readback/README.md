# Finite native Furnace save read by the original client

Two fresh Windows OpenJDK17 processes (`-Xint` and default) loaded separate,
byte-identical copies of the same native SDK save through the untouched original
client JAR. Both recovered Block62/metadata2 at (0,64,2), ore15x1, coal263x1,
Burn1397/Cook4, Worldtime211, and serialized Player iron265x1 in slot8. Original
SK recomputed fuel duration1600 from coal. The actual saved SK already existed in
the Chunk Map before lazy World lookup; that lookup retained the same identity.

The original WorldInfo.Player Compound was read using genuine IX(null).b(SP).
This is a serialized inventory boundary, not construction of a GS Player entity.
The World name constructor argument came directly from Data.LevelName. No
postconstruction field repair, original tick, original save, gameplay, rendering,
pixel comparison, JDI/live-Code trace, or bidirectional roundtrip is claimed.
This historical native save's receipt contains 9242 checks; it does not qualify
later native source changes.

The source saves root contained 13 files (alpha contained 9). All source bytes
remained unchanged. Each owned copy retained all 13 files with identical bytes,
except alpha/session.lock. Original QJ requests rw permission when opening an
existing region file, so the guard permitted only those enumerated copied region
paths plus the owned session lock; postread byte inventories remained mandatory.
Two failed preparations are retained separately by hash and scope in provenance.
They are excluded from the successful two-process cohort.

`interpreter.stdout` and `default.stdout` are complete original selected-output
streams. `finite.json` must reparse exactly from both. Raw stderr stays private
because its JDK warning names a local absolute path; its exact byte count/hash and
successful physical validation are recorded. No saved World/NBT bytes, original
classes, JAR, assets, or compiled helper binaries are included. Class/Code hashes
identify archive methods; they do not establish entered runtime methods.

`FurnaceReadback.java` is exactly the executed observer source. Portable capture,
decoder, publisher, and packaging/control helpers were added after execution.
Run `python -B packet.py` and `python -B test_packet.py` (also `-O`) without game
inputs. Controls exercise the retained real streams and reject changed rows,
stream transport, scope, identity pins, copy manifests, and unexpected artifacts.

To repeat the finite boundary with external owned inputs, use JDK17 and:

```
python -B capture.py --source SAVES_ROOT --jar ORIGINAL_CLIENT_JAR --libraries LIBRARIES_ROOT --java-home JDK17_ROOT --output NEW_CAPTURE
python -B publish.py --capture NEW_CAPTURE --output NEW_REVIEW_PROJECTION
```

The capture copies the complete source twice and never loads the retained source.
It validates typed NBT independently, enforces the declared finite values, checks
bounded child status/counts/hashes, pins JAR/helpers/products/JDK before and after,
and permits only the owned session lock to differ after loading. The publisher
rechecks physical inputs, both retained raw streams, the exact report projection,
and copied/source bytes before writing any new review projection. It joins exact
commands to pinned executables, helper products, classpath, mode flags, copied
root, target, and decoded constructor name. The CLI emits a two-file review
projection (`finite.json` and wrapped `source-pins.json`), not this complete
19-file publication packet. Complete packet packaging has a separate audit.
