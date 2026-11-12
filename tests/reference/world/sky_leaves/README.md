# Raw temperature and biome sky-color leaves

The 386 rows come from unchanged Beta 1.7.3 client methods and the pinned
Temurin JDK17 Color implementation. No original class, asset or decompiled
source is included. [Source pins](source-pins.json) contain class and selected
method Code hashes; the capture runner also verifies the supplied client JAR,
SDK metadata JARs, JDK tools and `java.desktop.jmod` before and after execution.

The original observer constructs real `xv`, `ug`, `cc` and `ei` owners. Only
the `fd` carrying that actual Info is allocated without its World constructor.
Its constructor seed reads still call real `fd.s()` and `ei.b()`; no seed
getter, noise object, returned array or sampled value is substituted. The
native replay likewise constructs the canonical GAME-bound manager from
actual Info on a prepared RAW World. This qualifies manager construction and
the leaf bodies, rather than World construction or a complete rendered frame.

The corpus contains:

- 60 raw-temperature rows: four signed seeds, five coordinate pairs including
  integer limits, and null/empty/long actual `a` arrays. It records exact
  binary64 output, replacement/reuse identity, retained length and every `a`
  element. Sentinel rainfall/auxiliary arrays remain unchanged. The raw method
  uses promoted `.025f` scales and frequency `.5`, without the processed
  temperature path's `.25` and climate correction.
- 299 sky-color rows: all 13 actual Biome statics, each with 23 binary32 inputs
  covering signed zero, subnormals, both clamp boundaries and adjacent values,
  finite extremes, infinities and signed quiet/signaling NaNs. Sky Biome `m`
  uses its actual `ry.a(F)I` constant override, with no Color allocation or FP.
- 25 direct JDK `Color.HSBtoRGB` rows: every hue sector, wrapping hue, zero
  saturation, out-of-range channels, subnormals, infinities and NaNs. Channels
  use source binary32 arithmetic, Java float-to-int conversion and wrapped RGB
  packing; there is no unit-interval validation.
- Two real `hd.b(II)D` override rows returning fixed `f` while preserving the
  actual `a` identity and elements. The native fixture materializes this actual
  manager array through its real fixed rectangle sampler, then authors the
  same element sentinels for the scalar check.

JDK Color is initialized before installing the observer's deny guard because
its platform bootstrap loads AWT. Every recorded leaf invocation then runs
with writes, networking, listeners and further native-library links denied.
Successful capture records zero network/native attempts during that guarded
scope. AWT/Toolkit class initialization is outside this loaded-runtime leaf
qualification. The C bridge translates the pinned Color arithmetic and models
the temporary Color(int) value allocation before conversion/getRGB; its
identity never escapes.

To capture from locally supplied pinned originals:

```text
python -B tests/reference/world/sky_leaves/capture_reference.py \
  --client-jar /private/b1.7.3.jar \
  --library-root /private/launcher/libraries \
  --output /private/new-sky-leaves-capture
```

The native `world.sky_leaves` replay compares every row exactly and requires
normal native completion. Additional controls cover strict-FP refusal before
array publication, a zero buffer quota, unused future buffer descriptors,
manager aliases surviving Dimension retirement, source-null Biome receivers,
and a fixed scalar returning even under an unsupported arithmetic environment.
Authored resource files are fingerprinted before and after native execution.
