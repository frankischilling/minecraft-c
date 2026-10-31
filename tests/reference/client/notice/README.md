# Achievement notice checks

The authored native test requires only Python and the configured production
probe. It creates a fresh localization/map fixture and uses real Registry,
Settings, Entity/Vec3, notice/model and complete application owners:

```text
python -B tests/reference/client/notice/run_native.py
  --native-probe CONFIGURED_MC_NOTICE_PROBE --work-dir PRIVATE_OUTPUT_PARENT
python -B tests/reference/client/notice/test_capture.py
python -O -B tests/reference/client/notice/test_capture.py
```

Each invocation creates a fresh retained directory. Checks cover ten authored
notification results, 16 allocation prefixes, Random failure ordering, movement
pool isolation, model aliases, borrowed owner guards, 1000 reminders without
permanent Registry growth and cleanup. This path needs no JDK, original JAR,
assets, window, SDL or graphics/audio runtime.

The optional unchanged-original comparison currently runs on Windows x86-64:

```powershell
python -B tests/reference/client/notice/compare.py `
  --client-jar C:/private/b1.7.3.jar --libraries C:/private/lwjgl2.9.0-libraries `
  --natives C:/private/lwjgl2.9.0-windows-x64 `
  --java C:/private/jdk17/bin/java.exe --javac C:/private/jdk17/bin/javac.exe `
  --native-probe BUILD/Release/mc_notice_probe.exe `
  --resources C:/private/imported-client-jar-resources `
  --work-dir .local/notice-reference-runs
```

`--native-build-manifest JSON` records the configured native source/archive
cohort. Build all dependent headers and application archives together, including
Vec3 and the full private application storage. No older archives may be linked
against changed private headers.

The runner verifies the client JAR, 22 class/Code metadata records, four library
archives and supplied lwjgl64.dll. Native localization/map bytes must equal the
supplied JAR entries. JDK17 paths are explicit and resolve to one bin directory.
The newly compiled observer is the only helper class on the supplied game/library
classpath. `vu` independently checks its actual JAR origin. Ambient JVM option
variables are removed; unchanged game methods run with `-Xint -Xverify:all`.

Missing required private files return 77. Changed pins, tool/version mismatch,
nonzero exits, unexpected stderr, incomplete markers, changed sources/classes
or unequal records fail. Stdout/stderr have separate 4 MiB/1 MiB limits;
overflow, reader/writer failures, short writes and timeout fail with bounded
evidence. Nine capture controls also run under optimized Python.

Profiles compare raw float/double bits and actual identities/order, with no
pixels or numerical tolerance. The callback profile uses genuine `kq` and joins
its real Timer-hack daemon. Real timestamps are checked independently. Default
Random entropy is not synchronized; the sole constructor advance and Gaussian
defaults are checked. No source-empty draw succeeds. See the implementation
scope in `docs/client/achievement-notice.md`.

Only source and binary metadata belong here. Keep JARs, native libraries, assets,
generated classes and runtime outputs private.
