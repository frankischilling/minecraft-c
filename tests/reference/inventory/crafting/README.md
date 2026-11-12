# Crafting CPU reference

This reference invokes the unchanged Beta 1.7.3 client and dedicated-server
classes. It compares real InventoryCrafting/result/InventoryPlayer operations,
Player/Workbench Container constructors and Slot graphs, and the registered 151
recipe match/output-preview closure. The runtime implementation remains C17;
Java is used only by this opt-in reference test.

```
python tests/reference/inventory/crafting/compare.py \
  --client-jar /private/b1.7.3.jar --server-jar /private/b1.7.3_server.jar \
  --native /build/mc_crafting_probe --java /jdk17/bin/java \
  --javac /jdk17/bin/javac --work-dir /build/crafting-reference
```

The runner requires the exact archives in `class-code-pins.json`. It validates
whole-JAR and selected class hashes, complete field/method descriptors, Code
bytes, Code attributes and handlers before original execution. It also verifies
the authored helper source, generated helper class/Code and native source pins
in `provenance.json`. A changed source or fixture requires explicit pin review.
`--source-root` can identify a separate source checkout; otherwise the runner
locates the source root through its ancestors.

Every invocation creates a fresh child of `--work-dir`; it does not delete or
reuse old work. Processes have finite time/output bounds. JDK 17 is the measured
profile, and external JVM option injection is rejected. The original classpath
contains only the freshly compiled helper directory and validated original JAR.
The helper directory must contain exactly its own uniquely named class. Runtime
bindings rejoin every selected class resource hash and code origin to that JAR.
Class-initialization logs require the crafting classes and exclude Player,
World, Minecraft, LWJGL and SoundSystem initialization where applicable.

Only `/lang/en_US.lang`, `/lang/stats_US.lang` and `/achievement/map.txt` are
extracted, with pinned hashes, into the private work directory for native class
initialization. They are not published with this reference. No original
classfiles, assets or output captures belong in this directory. The Java
reflection helper is authored test code; it contains no copied game method
body or replacement game class.

The per-side case files contain 1,047 authored input layouts and perturbations.
They use catalog facts already retained by the recipe-bootstrap tests, rather
than expected output state. They cover every registered recipe, 2x2/3x3 placement,
mirroring/translation, extra/wrong-damage inputs, wildcard damage, zero/negative
counts and aliased inputs. The native probe receives only side, private resource
root and input-plan path. Original/native processes must exit 0 with empty
stderr. All 1,153 projected rows per side are compared exactly; only host line
separators are normalized with `splitlines`. Actual `151 recipes` and
`16 achievements` console banners remain in the compared output.

The original constructors receive genuine supported null Player/World
references. Those references remain unread by every executed constructor and
matching method. This is not a fabricated Player/World, and it does not verify
full Player construction/gameplay. Output pickup, cursor/stat/achievement hooks,
ItemMap World effects, click/merge/close/drop, rendering, audio and networking
remain outside the measured profile. Class-initialization logs are an execution
boundary check, not a general method trace or whole-game reachability proof.

The public native contract wrapper is
`tests/tools/inventory/test_crafting.py --executable <mc_test_crafting>
--work-dir <build>/crafting-contracts`. It generates authored language/map
resources and executes both complete C test bodies. No private original assets
are required. Native assertion counts may vary with platform allocation sizes;
the tests themselves are not skipped or weakened.

The protocol driver is
`tests/tools/inventory/test_crafting_probe.py --executable <mc_crafting_probe>
--work-dir <build>/crafting-protocol`. It checks 48 accepted/rejected cases across
both sides, including LF/CRLF/final EOF, the 1023-byte row boundary, embedded NUL,
controls/non-ASCII, invalid dimensions/indices/int32s/tuples/groups and conflicting
aliases. Input plans are ASCII printable/tab rows; blank/overlong rows and bare
CR are invalid. Empty files intentionally request constructor-graph-only output.
These are native test-protocol checks, not additional gameplay equivalence.
