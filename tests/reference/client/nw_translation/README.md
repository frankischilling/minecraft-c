# Tessellator additive translation

The packet records 106 actual `nw.c(FFF)V` calls across 36 profiles on the
unchanged Beta1.7.3 client archive, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Two fresh guarded JDK17.0.17 VMs produce byte-identical stdout. A third fresh
VM under read-only JDI returns the same bytes and observes all 318 field stores.
Each call writes s at PC8, t at PC19 and u at PC30. Watchpoints retain actual
old/new words and the live translation triple before each store, including the
preceding stores. The live method Code hash matches the untouched archive.

The real static initializer constructs `nw.a` with b=true and c=false. Its
genuine constructor allocates GE direct buffers and skips GLContext/VBO calls.
The helper uses the real `b(DDD)V` setter for authored starting translations.
Half the profiles remain outside a batch; the others use real begin and a
zero-vertex draw. No GL context or visible window is required. Reflection reads
fields; it does not allocate RAW owners, change source flags or replace buffers.

Inputs cover Cactus-sized offsets, nonzero translations, signed zero,
binary32 widening before binary64 addition, ULP ties, subnormals, infinities,
distinct NaN payloads, cancellation and repeated calls. Raw bits remain exact.
Unrelated primitive fields, actual buffer identities, NIO positions/limits/
capacities and backing-array length/content hash stay unchanged around each call.
Source guards observe zero network, native-link, write and exec attempts.
Five explicit denial controls verify the guards. Local JDI debugger transport
is separate from those game-source observations.

`provenance.json` binds the exact executed Java helpers, inputs, observations,
raw original stdout, store rows and complete NW/GE class/method fingerprints.
It records the JDK and compiled-helper hashes. Selected TSV rows use ASCII LF;
`original-stdout.txt` preserves the original transport bytes. No original class
bytes, JARs, assets or private filesystem paths are included. The public capture
wrapper and validators were added after the retained capture; their existence
does not claim another original run.

Run the public integrity controls and native replay without Java or original assets:

```sh
python -B -m unittest discover -s tests/reference/client/nw_translation -p test_packet.py
python -B tests/reference/client/nw_translation/run_native.py --executable BUILD/mc_test_nw_translation.exe --work-dir BUILD/nw-translation-controls
```

Repeat the original capture with the supplied archive, pinned library root and
matching JDK17.0.17 `java`/`javac`. The output directory must be new:

```sh
python -B tests/reference/client/nw_translation/capture_reference.py --jar b1.7.3.jar --libraries LIBRARY_ROOT --output PRIVATE_NEW_DIRECTORY
```

Native replay reaches the genuine operation guard and counted-owner cleanup,
compares all original rows, then separately checks NULL native admission,
unsupported floating controls before the first conversion/store, accepted
sticky flags and terminal resource-retirement admission. Those admission controls
are native policies, not original exceptions. This CPU fixture does not produce
a genuine Display-lease cleanup failure. Whole Cactus inventory geometry,
original pixels/audio, extended gameplay and FPS need separate qualification.
