# CV faces and NW float color

The observer invokes the unchanged original `cv.a` through `cv.f` methods with
descriptor `(Luu;DDDI)V`, and `nw.a(FFF)V`, from the pinned original client JAR.
`source-pins.json` records the JAR, class and selected method hashes. The capture
requires JDK 17, verifies its loaded game classes come from that JAR, and denies
network access, native library loads and writes during observation.

The 1,656 face rows cover all six faces, four ordinary UV rotations, two unknown
rotation values, texture flip, AO corner colors, overrides, negative and large
texture indices, full/partial/outside/reversed Block bounds and translated
coordinates. The 24 float-color rows include clamp boundaries, signed zero,
subnormals, infinities and NaNs with and without the source color lock.

These are prepared fields of actual Block, CV and NW identities. Each face row
records the source NW counters, final UV, feature flags, packed color and every
word in the first 48 backing slots, including stores made before an exception.
NULL Block, NULL NW array and arrays of 0, 4, 8, 16 and 32 words fail at their
actual source access. A 47-word array succeeds: this vertex layout leaves its
last padding word unused even though the final source cursor is 48.

The native executable uses the canonical registry Block bounds, real CV fields
and actual NW singleton. It compares all 1,680 rows exactly, checks balanced
operation guards and verifies the faces leave CV fields unchanged. A separate
native control checks refusal under an unsupported floating environment; it
is not presented as a Java exception or an original differential row.

This profile does not qualify DK compilation, World access, draw submission,
OpenGL or a complete terrain frame. It does not replace source effects with an
inspection mesh.
