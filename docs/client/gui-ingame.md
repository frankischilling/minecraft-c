# GuiIngame constructor

The canonical `uq` constructor is implemented separately from its later startup
caller. Its CPU owner has the actual inherited `ub.k`, ten instance fields,
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
These CPU controls perform no GUI drawing, frame update, filesystem or device
operation. Constructor readiness does not mean that the HUD is attached.
Normal startup still stops at resource-thread allocation664, before `uq`
allocation697 and App.v publication705. Chat clearing, HUD drawing and caller
adoption remain unfinished.
