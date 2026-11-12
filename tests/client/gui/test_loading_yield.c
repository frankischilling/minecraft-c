/* Include the exact caller once, forwarding its genuine C17 yield before
 * authored inspection. There is no throwing/replacement game-child body.
 * Link map must exclude the archive loading_draw_body.o here. */
#include <threads.h>
void loading_real_yield_then_probe(void);
#define thrd_yield loading_real_yield_then_probe
#include "../../../src/client/gui/loading_draw_body.c"
#undef thrd_yield
