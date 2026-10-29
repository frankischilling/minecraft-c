# GuiScreen and GuiMainMenu

The source GUI owners implement GuiScreen, GuiMainMenu, GuiParticle and GuiButton
construction. GuiMainMenu uses the same GuiScreen base type as the application's
current-screen field. These owners are separate from the executable's existing
title-screen renderer.

GuiMainMenu constructs its shared Random once, reads `title/splashes.txt` through
the existing class-resource stream and UTF-8 Reader, trims each line and chooses
a nonempty splash. A missing stream, an empty splash list and a read IOException
retain the original constructor's fallback behavior. Native allocation and
resource-provider failures remain explicit errors. The original constructor
leaves its Reader open; native retirement closes the retained stream.

## Screen initialization

GuiScreen initialization constructs a GuiParticle owner, stores the application,
captures the application's current bitmap Font, stores the supplied dimensions,
clears its button list and calls the concrete screen initializer. Repeated
initialization keeps the same list and retains earlier child owners until
retirement. A later application Font change does not replace the captured Font.

GuiMainMenu initialization samples the real wall clock for Calendar construction
and Date construction, applies the holiday splash checks, loads translated
button labels and constructs the original buttons. Application mode selects
the options/quit layout. A missing Session disables the multiplayer button.
Calendar currently admits the documented Gregorian locale and timezone profile
in [calendar.h](../../src/platform/time/calendar.h); unsupported defaults return
an explicit error.

The existing bitmap Font now accepts borrowed UTF-16 views for translated labels.
These calls use the same measurement and drawing bodies as its String interface.
The caller keeps the canonical String owner alive throughout the call.
GuiIngame also implements its message-list clear method, retaining list capacity
and clearing occupied slots in the selected JDK's order.

## Ownership and validation

Each registered application counts independent screen-initialization and
GuiParticle borrows. Application retirement waits for those borrows. Screen
retirement releases retained Fonts, particles, buttons and stream state before
ending its application borrows. External screen occurrences make strict
retirement return BUSY without consuming the caller's owner.

Public constructor and initialization controls cover retained failures,
repeated initialization, old Font and particle lifetimes, and application
retirement. Separate resource controls use caller-owned empty directories and
authored splash/translation files. Font device controls compare borrowed-view
measurement and drawing with the existing String interface. These checks have
their own execution results; source presence alone does not establish a pass.

Current-screen transitions, menu drawing and physical input dispatch, button
actions, world selection/creation and a connected gameplay loop remain
unfinished. Original menu, Calendar and visual correspondence remain unverified.

Seven selected Windows MSVC Debug CTests pass for GuiIngame, source-screen
construction/initialization, authored resource files, Calendar and App services.
This result covers native component behavior; Linux and original menu comparisons
remain separate work.
