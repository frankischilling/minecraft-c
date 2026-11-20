/* Include the exact canonical transport once; forward its real SDL pump.
 * This is a native control only, with no runtime hook or second poll owner.
 * Link map must exclude the archive events.o for this executable. */
#include <SDL3/SDL.h>
void loading_real_pump_then_event(void);
#define SDL_PumpEvents loading_real_pump_then_event
#include "../../../src/client/display/events.c"
#undef SDL_PumpEvents
