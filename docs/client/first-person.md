# First-person camera component

The client has a first-person camera component over real LocalPlayer, Settings,
World and application owners. It computes the selected original projection and
modelview operations, and the terrain renderer accepts those completed frames.
Normal startup still uses the fixed inspection path. App Player publication,
the complete render tick and the playable frontend remain unfinished.

`mc_client_first_person_camera_for_application` reads the actual application's
current Settings and World at each reached source field read. The selected
LocalPlayer remains an explicit argument until the outer App h/i publication is
implemented. The separate component entry fixes owner identities and dimensions
while still rereading scalar Settings. Neither entry infers App.f from the
Player's World or the view's retained geometry.

The implemented operations include FOV, anaglyph projection, hurt/death rotation,
view bobbing, sleep/bed orientation, first-person debug behavior and raw render
origin interpolation. Fog samples keep their separate eye-height adjustment.
The constructor-normal renderer cells have their declared source values; this
does not construct a complete EntityRenderer. Positive portal distortion stops
at its missing clock owner, and third-person stops at its missing renderer cell.

World queries can load and publish actual saved Entities. Their callbacks may
replace or clear App Settings/World fields before later reads. Each captured
World receiver stays held across the query. Material immersion separately
reloads Entity.world before metadata, then checks floating-point mode before
the reached fluid-height arithmetic. These holds protect storage without
freezing source fields. A failed operation retains its completed matrix prefix
and reports `complete=false`; native admission preserves the caller's frame.

The finite view can retain the Player's real World and draw the existing section
geometry with a completed camera frame. Its application entry uses the same
actual App reads. The caller supplies fog. Chunk streaming, dynamic geometry,
sky/weather and source fog/clear calculations remain separate dependencies.
The existing inspection entry keeps its fixed camera and lifetime contract.

The combined integration passes 34 selected Windows Debug CTests and 26 cases
on each fresh sanitized Clang/GCC Debug build with leak detection. All matching
targets rebuild, including the actual client executable. Windows checks include
CPU camera, real World callback cases, native GPU drawing, original GL matrices,
startup and three actual executable terrain captures. Source inventories and
build products remain unchanged through each selected suite. These 86 selected
cases do not constitute a new full regression matrix.

The native component has 438 checks. Sixteen saved-Item callback cases have
1,198 checks, including late Settings/World replacements, null receivers,
floating-point prefixes and hold-counter failures. Protocol checks run normally
and under Python optimization. The original Windows context profile verifies
31 actual game classes and compares 42 baseline matrices plus ten callback
prefix matrices. Its absolute coefficient tolerance is explicitly `2e-6`;
this does not establish bit-exact driver output or original pixel equality.

The [reference guide](../../tests/reference/client/camera/README.md) gives
commands, selected methods, recorded scope and remaining dependencies. Native
CPU tests need no JAR, Java, assets or GL context. GPU tests require a real
context. The opt-in `MC_ENABLE_CAMERA_REFERENCE_TESTS` original profile requires
the declared Windows libraries and matching JDK17 tools. The broader
[test record](../testing.md) keeps these checks separate from earlier full
runtime and hosted results.
