# GuiIngame construction, clearing and ticks

The canonical `uq` constructor is connected to normal startup. Its CPU owner has the actual inherited `ub.k`, ten instance fields,
distinct List and ordinary Random, literal String identities and borrowed App.
The interface is [gui_ingame.h](../../include/mc/client/gui/gui_ingame.h).

Class activation constructs its own static ItemRenderer and publishes `d` only
after that constructor returns. It does not borrow the achievement notice's
ItemRenderer or use the RenderManager's distinct held-item renderer. Native
class ownership keeps the real model namespace; its borrowed Random profile
must outlive both owners.

Cold allocation performs no source constructor work. Construction preserves
the superclass/field order and retains failed List/Random prefixes. Native
budget/profile failures remain distinct from Java exceptions. The App is a
stable borrowed source operand and must outlive every exported occurrence.
Strict retirement rejects extra holds before revocation; child/namespace
cleanup retains retryable slots on BUSY or failure.

`client.gui_ingame_constructor` passes in the selected Windows and Clang cohorts.
The original constructor controls perform no GUI drawing, filesystem or device
operation. Constructor readiness does not mean that the HUD is attached.
Normal startup constructs the canonical instance at `702` and publishes App.v
at `705`, then constructs and initializes the main menu. The real display-screen
caller invokes message clearing at its `101` call. Strict startup retirement
preserves App and its dependencies while an external GuiIngame occurrence exists.
The actual `uq.a()V` tick decrements positive `j` before incrementing `h` with
Java wrapping arithmetic, then reads the current message List. The initialized
empty List returns at `70`; the default no-World menu calls this method each
reached tick. A null List fails at `34` after both earlier stores, and a null
message fails at `56`. Nonnull messages stop at the explicit canonical `sw`
owner/type dependency at `52`; their age updates are not replaced by a copied
message model. Message clearing retains the real List's capacity.

The extended `client.gui_ingame_constructor` case passes in the focused Windows
four-test selection alongside uploads, events and the native menu. HUD drawing,
chat interaction, nonempty message ticks and World/gameplay adoption remain
unfinished. The latest complete suite, hosted CI and Linux tick qualification
remain pending.
