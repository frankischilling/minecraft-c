# MouseHelper and native cursor

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

Keyboard and mouse creation use their genuine warm state after Display
construction. They do not reset queues or add another event pump. Controllers
creation and the connected input/tick/gameplay loop remain unfinished.
