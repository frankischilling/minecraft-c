# Original client11 Mob NBT reference

This recipe observes the actual client constructors, registered known-ID factory, inherited NBT readers/writers and Boolean storage gate for Spider, Zombie, Skeleton, Creeper, Slime, Pig, Sheep, Cow, Chicken, Wolf and Squid. It ran ordinary and independent read-only JDI VMs on Windows OpenJDK17.0.17+10 and Linux OpenJDK17.0.20.1+1. Each host produced62 raw NBT outputs per VM, independently decoded from their actual binary bytes, with3086 JDI events and74 observed original Code hashes. Original classes/assets and compiled helper classes are not distributed.

Every Mob receives actual NULLWorld, after the actual Session constructor prepares registries. Known registered IDs go through unchanged JC.a(compound,NULLWorld), genuine constructors and full SN.e. These prepared source owners do not qualify an actual Chunk/World caller, gameplay reload, native non-NULLWorld factory admission, server or Nether. Those require separate evidence.

## Reproduce

Use the verified original client JAR and a full installed JDK17. The driver checks JAR SHA256 af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c and JDK17, freezes helpers/scenarios/driver/tool/JAR inputs before javac, runs isolated ordinary/JDI processes, retains bounded complete stdout/stderr, raw typed NBT, receiver/watchers and class/Code pins, and verifies unchanged inputs/helpers. It never assigns original fields, Random seeds, clocks or debugger state.

```text
python -B tests/reference/entity/mob_nbt/capture.py --source-root . --client-jar /path/to/b1.7.3.jar --jdk-home /path/to/full-jdk17 --work-dir /path/to/captures
python -B tests/reference/entity/mob_nbt/audit_trace.py --receipt /path/to/retained-run/receipt.json --output /path/to/trace-audit.json
```

The optional CMake include adds reference.client_mob_nbt only when MC_ENABLE_CLIENT_MOB_NBT_REFERENCE_TESTS is explicitly enabled with MC_REFERENCE_CLIENT_JAR and a discoverable full Java17 JDK. It defaults OFF and performs no original runs in normal CTest.

## Observed semantics

Living writes Health/HurtTime/DeathTime/AttackTime as Shorts. A missing Health first stores the getter default0, then checks presence and stores10; timer defaults are0. Incorrect Health Byte throws at the actual Compound Short getter before the Living first store. Full SN.e dispatches own reader311; SN.d dispatches own writer145. Boolean SN.c performs exact-runtime-class ID lookup before the removed flag, adds id and calls full writer only for an admitted registered entity. Direct SN.d bypasses that gate. The ID-null branch is pinned static bytecode, without a dynamic unregistered-subtype claim.

| Species | Own persisted fields beyond Living |
| --- | --- |
| Spider, Zombie, Skeleton, Cow, Chicken, Squid | none |
| Pig | Saddle Byte Boolean |
| Sheep | Sheared Byte Boolean; Color Byte masked to15 |
| Creeper | powered Byte Boolean only when actual watcher Byte equals1; otherwise an existing key remains |
| Slime | Size Int from signed watcher Byte minus1 |
| Wolf | Angry/Sitting Byte Boolean; Owner String |

Slime reads stored Size Int plus1 as full Java int, narrows only the watcher Byte, applies dimensions from the full requested int, and overwrites Living Health with its full int square. Stored Size127 gives health16384, watcher-128 and written Size-129. Size255 gives health65536, watcher0, written Health Short0 and Size-1. Size-1 gives health0 and written Size-1. Wolf missing/empty Owner does not clear its previous owner/tame bit; missing booleans clear sitting/angry. The tested owner contains NUL, Omega and a surrogate pair, preserved as UTF16 units through real modified UTF8 output.

No Age, EggLayTime, Fuse, ExplosionRadius or unknown authored field is emitted. Chicken's actual constructor egg timer remains unchanged by NBT. Constructor-only Slime entropy correlates within its receiver; fresh removed-Pig direct-writer Rotation retains constructor yaw and is excluded only from cross-VM equality. No cross-process HashMap ordering or compressed-byte identity is assumed.

See scenarios.json, source-Code-pins.json, helper-Code-pins.json, windows-results.json, linux-results.json, the separate passive trace audits and provenance.json. PREPARATION.md and ATTEMPTS.md are exact capture inputs; the latter preserves the two failed driver-audit attempts. This README is a later package description and was not an original capture input.
