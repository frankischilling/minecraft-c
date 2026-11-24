# Original metadata output preparation

Authored prepared handler boundary only. Seven source calls use a genuine mx and ei built by original NBT setters and ei(nu), then invoke unchanged mx.a(ei,List). Both VMs use the same helper class bytes; ordinary and read-only JDI runs own separate roots. The helper performs reflection to resolve exact classes/constructors/methods and verifies JAR code origin. That reflection may resolve/init classes before the source call; it is not a whole-game initialization observation.

Cases in order: fresh empty List; existing current/old first write; second write with distinct Time/name; nonempty old directory; nonempty old/current directories; directory at level.dat_new; null-first List. Loaded Player input exists but source List serialization omits it in empty/null-first cases. No nonempty Player serializer, Chunk write, World save, original game startup, Linux profile, or native C execution is qualified.

Clock bounds are captured in each helper process immediately outside its unchanged mx call. LastPlayed must lie within that invocation's real interval, and is not equated across processes. Stored files/old backups retain raw compression bytes; semantic comparisons preserve exact NBT scalar kinds/values/UTF16. Compound order and gzip-library byte identity are not cross-process parity claims.

The read-only observer uses breakpoints, MethodExit, Exception events, field/argument reads and bytecodes. It does not invoke target methods, create target objects, modify target fields/locals or change clocks. Ordinary helper authored reflection and original method invocation are declared target setup, independent of debugger observation. File-open failure is genuinely caught and printed by original fm; it remains an explicit diagnostic dependency in native C.

Inputs and scenarios must be frozen before compilation, then unchanged hashes checked after each capture. Capture bounded stdout/stderr and raw NDJSON, tool/JAR/modules hashes, per-class/per-Code original and helper hashes, files and semantic outputs. All filesystem effects are confined to newly created disposable roots beneath each capture directory.

Run with an installed full JDK17, the two supplied original JARs and an output directory. Each invocation creates a fresh child directory and refuses to reuse case roots:

```text
python -B tests/reference/world/source_metadata_output/capture.py --source-root . --client-jar b1.7.3.jar --server-jar b1.7.3_server.jar --jdk-home <JDK17-home> --work-dir .local/source-metadata-reference
```

The recorded original cohort used Windows x64 Temurin17.0.17+10. The driver records the actual JDK release, executables, native libraries, modules, class/Code hashes, Python interpreter and host profile. Other platforms or JDK builds gain no execution credit from this recorded cohort. The server JAR is hash checked for authority identity only; no server class is executed.

The original caught FOS-open failure prints one retained FileNotFoundException stack trace and then returns150. Native C currently retains that cause at its printStackTrace dependency; this original return does not make the native error path complete. Native codec limits and authored write/close faults are separate controls, not observed original operating-system faults.

The private exploratory captures preceding the qualified driver are retained: the first audit omitted the as class from its Code table, and the second counted a JDK native-library File.exists as a rotation call. The corrected audit admits only direct fm.a caller frames for the source File sequence, while retaining the incidental native-library event separately. Those failed audit attempts gain no qualification credit.

The Windows cohort above documents its recorded Temurin17.0.17+10 profile. Separate [Linux handler observations](linux-provenance.json), [Linux authored stream controls](faults/linux-provenance.json), and [native/original comparisons](comparison-provenance.json) record the later Ubuntu17.0.20.1 profile and metadata checks on both hosts. The stream controls use authored OutputStream faults; they do not establish operating-system or native FOS failure parity. Full World/Chunk saves and native fm147 diagnostic completion remain unqualified.
