# GuiMainMenu renderer and observation probe

`mc_gui_main_menu` owns title drawing for the client and a conditional native
probe. It borrows the display and asset cache, owns its FontRenderer and trig
table, and leaves texture retirement to the display. Creation loads the font
image at the application's existing point before title Random and splash
selection. Font mapping and trig initialization remain lazy during drawing.
The caller still owns layout, translation, splash selection, calendar handling,
input, Timer, swaps and capture.

Drawing samples the supplied millisecond callback after both logo quads,
at the original `fu.a(IIF)V` clock position. The application supplies its
platform clock or explicit diagnostic time. The probe supplies a conditional
value recorded from outside the native producer. Neither caller supplies font
advances, pulse scale or expected pixels. The source's unread partial tick
remains unread, including nonfinite bit patterns.

The supported path is the default-pack, non-anaglyph, no-World title with
ordinary GuiButtons. At most 32 button records are admitted; visibility,
enabled state, half-open hover and Java integer arithmetic determine drawing.
Nonzero z levels, arbitrary button subclasses and a World background remain
unsupported. This module does not complete child screens or gameplay.

## Conditional scene transport

`mc_gui_main_menu_probe RESOURCE_ROOT SCENE_FILE` takes the extracted JAR
resource directory, such as the import root's `jar/` directory. Scene files use
ASCII, single spaces and LF, with this ordered structure:

```text
MC_GUI_MAIN_MENU_SCENE_V1
display WIDTH HEIGHT
gui-scale SIGNED_INT
mouse X Y
partial-tick EIGHT_LOWERCASE_HEX_BITS
clock-millis SIGNED_LONG
splash UNIT_COUNT [FOUR_HEX_UTF16_UNITS...]
buttons COUNT
button ID X Y WIDTH HEIGHT ENABLED VISIBLE LABEL_COUNT [UNITS...]
end
```

Repeat the button line exactly COUNT times. String count `-1` means null;
zero means empty. Enabled and visible are `0` or `1`. Decimal integers have
no leading plus, leading zeroes or negative zero. UTF-16 units use exactly four
lowercase hexadecimal digits. Display dimensions are positive, at most 4,096
per axis and 4,194,304 pixels. Files are bounded to 64 KiB; each String has at
most 4,096 units. The parser rejects trailing tokens and lines.

The probe computes ScaledResolution from physical dimensions and the GUI scale
setting. An actual drawable size differing from the requested scene fails.
It draws once through the same production module, reads top-down raw RGBA8,
queries the actual framebuffer, and retires its resources. Success writes one
JSON line, then exactly `stride * height` RGBA bytes and EOF. Alpha is preserved.
Use binary process capture, especially from Windows PowerShell, to retain those
bytes. Allow up to 17 MiB for the complete stream and impose an external deadline.

Metadata records consumed scene values, computed scale, the one clock read,
driver strings and effective framebuffer properties. The framebuffer query
reads actual attachment sizes, color encoding, buffering and sample state
using the [OpenGL 3.3 interface](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf).
It does not force a framebuffer match with the original offscreen experiment.

## Verification scope

The public `tools.client_main_menu_probe` case checks scene admission before
graphics initialization. Accepted parser controls then deliberately fail an
empty resource-root argument; they do not prove drawing. Actual rendering and
original/native comparisons require separate resource-dependent runs. A frame,
clock value or coherent scene alone does not qualify full startup, constructor
history, input timing or pixel equality. See the
[original title evidence](../reference/client-title.md) for the retained source
experiment and its limits.

A focused native checkpoint passes all eight frontend cases on MSVC 19.51,
GCC 13.3 and Clang 18.1, with ASan/UBSan on Linux. It includes 50 real-display
checks and 19 accepted/34 rejected scene controls. Each compiler also completes
the bounded 200-frame application invocation and the conditional probe using
the retained original scene. FontRenderer retains its original 220 checks and
adds 262,379 command checks, including complete shadow/main passes, color and
blank-glyph advances, independent scratch lifetimes and the maximal run.
The application still uses the flat glyph view in this checkpoint; ordered
modelview replay remains separate implementation work.

The independent raw-image comparison checks all 409,920 pixels after reversing
only the original capture's row order. It fails at one pixel on Windows and
one different pixel on both Linux compilers; alpha matches everywhere. Windows
differs at `(553,141)`, Linux at `(658,85)`. Changing quad topology retained
the earlier images; moving source binary32 projection to the GPU removed one
of Linux's two earlier differences. Per-glyph matrix accumulation and the
legacy driver's transform arithmetic remain under investigation. No color
normalization, pixel tolerance or coordinate correction hides these failures.
