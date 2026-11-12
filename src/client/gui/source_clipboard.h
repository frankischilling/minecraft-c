#ifndef MC_CLIENT_GUI_SOURCE_CLIPBOARD_H
#define MC_CLIENT_GUI_SOURCE_CLIPBOARD_H

#include "source_screen.h"

/* Native DA.d() clipboard String bridge. Enter only for a reached paste on the
 * video thread. Success transfers a nullable canonical UTF16 String occurrence. */
bool mc_client_screen_clipboard_read(void *, mc_player_string **, mc_client_screen_error *);

#endif
