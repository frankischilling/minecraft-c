# Display SDK controls

The production source and controls in this package are authored and unrun.
Formatting, C17 compilation, native execution, Python controls, importer fixtures
and original SDK observations require separate qualification. Source metadata
is an input binding, not a passing result.

The four native observers use real owners:

* `test_sdk_queue.c` exercises actual ByteBuffer cursors, all 200 queue slots,
  the non-consuming full result, mismatch priority, partial copying and signed
  constructor multiplication.
* `test_sdk_icons.c` opens real authored files through resource_files, forwards
  real filesystem closes, attempts callback reentry and preserves failed-close
  retirement for retry. Authored RGBA inputs are independent of SDK pixels.
* `test_sdk_transport.c` creates real SDL windows and uses actual SDL temporary
  event payload memory. It checks retained lifetime across a later real pump,
  per-window ordering, tickets, bounded failure and opaque-head preservation.
* `test_sdk_window.c` creates real imported-file, staged Display, input, native
  icon and GL owners. It checks canonical identity, source dimensions, key and
  mouse buffers, source-specific grab resets, unrelated record retention,
  actual close flags, conditional update and active callback admissions. Its
  exact included transport TU forwards the real SDL pump before a real event
  watch. Qualification must prove that no second events object is linked.

The native wrapper CLI is:

```text
python -B tests/tools/client/test_display_sdk.py --executable EXE --kind queue|icons|transport|window --work-dir DIR
```

Each invocation creates a unique retained child directory, validates the direct
executable and completed bounded process receipt, empty stderr, actual streams,
positive footer, current source pins and unchanged authored resource inventory.
Its capture/identity primitives are shared with the existing statistics and
render-resource controllers. The wrapper does not infer original or physical
input-record parity from synthetic SDL queue controls.

`test_protocol.py` contains four unittest methods, eight positive fixtures and
21 negative fixtures. Only LF/CRLF and the exact corresponding positive footer
are admitted. `tests/tools/resources/test_lwjgl_icons.py` contains four methods
using generated structural class/ZIP fixtures and explicit fixture-only mocks;
the production original pins must reject generated input. Neither fixture is a
replacement game class, an original answer or an SDK pixel asset.

The additive CMake proposal declares eight cases. Queue, icon and both pairs of
Python controls are CPU cases. Transport/window cases require client/display
test options and real Windows or authenticated X11 graphics. Original profiles
are not silently enabled or substituted. The selected Windows/X11 SDK classes
and complete Code hashes are in `source-pins.json`; no literal asset bytes or
decompiled bodies are included there.

Before an execution result is accepted, use a coherent complete App composition,
fresh ALL/frontend linkage, default 1 MiB Windows stack, sanitized Clang/GCC with
leak checks, unchanged complete source/tools/dependencies/config/products and
actual old/new CTest inventories. SDK class/original-device, importer and
physical input observations are distinct later gates. Existing loading and
startup frontiers remain explicit.
