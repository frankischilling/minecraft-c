# MouseHelper, native cursor and Controllers

The source MouseHelper (`vy`) and SDK Cursor use the existing Display/input
namespace. The normal constructor receives the live Minecraft.m value after
allocation, finishes before Minecraft.C publication and creates the actual
native cursor. A null component is the ordinary startup profile; a nonnull
borrowed component must outlive its user.

Cursor creation retains a Display context lease and independent native pixel
storage. Temporary MouseHelper buffers can retire without invalidating it.
Creating a cursor does not select it, synthesize mouse motion or pump events.
The source cursor index keeps its actual default zero. Native retirement and
source cursor operations remain separate.

Pixel conversion follows the pinned SDK constructor and `ge` image behavior.
Windows controls use asymmetric rows, channels and partial alpha so a flipped
or swapped-channel image cannot pass by symmetry. The X11 adapter queries
native cursor bounds; its admitted one-bit-alpha capability does not claim
animation or general alpha support.

The isolated Windows qualification passes 4,198 CPU checks, 65 real-window
ownership checks and 6,169 native pixel checks. The later combined startup
source reruns the two affected window/pixel cases, retaining their 65 and
6,169 checks. These are native contract and integration results, with no
unchanged-original execution or original visual-parity claim.

Keyboard and mouse creation use their existing state after Display
construction. They do not reset queues or add another event pump.

Controllers construction now uses the same SDK input owner, its canonical
lists and a real SDL joystick reference. Empty discovery completes; repeated
create is idempotent. A nonempty physical discovery retains its native prefix
and stops at SDK Native12 before concrete Controller mapping. The official
SDL HIDAPI and virtual joystick drivers are enabled. The controls use real
virtual-device attach/detach; they do not supply replacement IDs or a fake
created flag. Polling does not pump or remove shared transport events.

The [startup-tail qualification](normal-startup.md#validation-and-remaining-work)
contains Windows and sanitized Linux CPU/device results. Complete physical
Controller behavior, input timing and the connected input/tick/gameplay loop
remain unfinished.
