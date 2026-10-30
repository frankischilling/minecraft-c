# First-person hand drawing

The normal empty hand now draws through the actual Player renderer, Biped model,
ModelRenderer, NW and Display. A loaded dry World frame can return from
`px.a(FJ)V` at 1013. Its parent still stops at the following delay/HUD segment,
`px.b(F)V:383`. Repeated frames, complete Entity rendering, held items, wet/fire
overlays and interactive gameplay remain unfinished.

## Source order

The [outer body](../../src/client/world/entity_renderer/first_person_hand.c)
implements `px.b(FI)V`. It resets the selected matrix, applies the anaglyph eye
translation, pushes a matrix and calls the same hurt and bob bodies used by
camera setup. It reads Settings again at each source predicate. Third-person
view (`kv.A`) skips both held drawing and overlays. Hide GUI (`kv.z`) skips
held drawing only. Sleeping skips both through the actual Player predicate.

The held draw occurs before the outer pop. Overlays occur after it, followed
by another hurt effect. The final bob occurs outside the pushed matrix.
Source exceptions retain the reached stack and matrix; native retirement
releases occurrences without inventing a pop or undoing GL effects.

The [held body](../../src/client/world/entity_renderer/held_item_draw.c)
implements the empty-slot route of `ra.a(F)V`. It interpolates equipped height
and Player pitch, enables standard lighting under the actual rotation, queries
World brightness and performs the swing transforms. The actual Engine chooses
the downloaded skin or its fallback texture. RenderManager resolves the real
Player Class and caches its inherited DS renderer. The same DS Biped receives
the source pose stores before its right arm draws.

The ordinary dry `ra.b(F)V` body disables alpha testing, evaluates actual
burning, suffocation and water predicates, then restores alpha testing. Reached
fire, inside-block and water drawing remain explicit dependencies. A nonempty
held slot also retains its missing draw child; it never becomes an empty arm.

## Arm geometry and reuse

[Model drawing](../../src/client/render/model_draw.c) implements `ps.a(F)V`
with its lazy `ps.d(F)V` compilation and six `tz.a(NW,F)V` faces. Each face uses
the original reverse-subtraction, cross product and normalized pooled Vec3,
then submits four vertices through actual NW methods. NW converts each quad
to six vertices. The first compilation consumes 24 pooled vectors and stores
the source list identity and compiled flag. Later draws replay that same list.

The source translation and Z/Y/X rotation branches operate on the canonical
Display matrix. Compilation preserves the original position, UV and normal
word behavior, including untouched duplicate normal words. Failed native
compilation aborts only its own pending definition. It preserves completed
source stores, list allocations and pool cursor changes.

The Engine's `ji.a(String,String)I` query reads the actual downloaded-image Map.
Absent/unready images use the existing texture lookup. Published images can
register a new texture; refreshing an already allocated downloaded texture is
still a precise dependency at 63. This query does not start a download.

## Evidence and limits

The [original fixtures](../../tests/reference/client/first_person_hand/README.md)
pin the verified client archive and unchanged source methods. They cover 64
outer return/null-receiver outcomes, two arm compile/reuse states, all six face
payloads, all 16 persistent Biped flag combinations and four complete
empty-arm/dry-overlay states. The reference Worlds,
Players and rendering helpers are genuine original owners with declared
prepared inputs. They do not establish whole-frame or original pixel equality.

The native test uses real App startup, a constructed GAME World and Player,
authored resources, and separate processes for fresh lazy-list profiles. Its
arm image is an authored skin, not an original asset. Full Entity rendering,
ordinary World ticks and the remaining HUD composition still prevent a playable
client. See [testing](../testing.md) for the qualified hosts and results.

## Qualification

All 53 selected targets build on Windows MSVC Debug and Linux Clang18
ASan/UBSan. The selections pass 62 Windows tests in 79.72 seconds and 58 Linux
tests in 120.12 seconds, without failures, skips or sanitizer findings. All
2,341 C/helper/build inputs remain unchanged through builds and tests.
Each host passes 1,741 hand/arm checks and renders 146 authored-skin pixels.
Both have a maximum outer-matrix error of 1.7881393432617188e-7 against the
predeclared 2e-6 absolute tolerance. Other stated rows and geometry hashes match
exactly. Fresh original captures retain unchanged tools, archives, inputs and
helpers, and reproduce the public fixture bytes.

Enable `MC_ENABLE_CLIENT_DISPLAY_TESTS` to register the authored graphics test:

```sh
cmake --build build --target mc_test_first_person_hand
ctest --test-dir build -R '^client.first_person_hand$' --output-on-failure
```

Use `--config Debug` for the Windows multi-configuration build and `-C Debug`
for CTest. Linux requires a working GL3.3 display, including an Xvfb/Mesa setup
as described in [build setup](../building.md). No original assets are needed
for this authored test.
