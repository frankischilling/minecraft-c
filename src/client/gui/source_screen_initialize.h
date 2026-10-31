#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_INITIALIZE_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_INITIALIZE_H
#include "../../platform/time/calendar.h"
#include "bitmap_font.h"
#include "source_screen.h"

typedef enum mc_client_screen_init_phase {
    MC_CLIENT_SCREEN_INIT_ACTIVE,
    MC_CLIENT_SCREEN_INIT_RETURNED,
    MC_CLIENT_SCREEN_INIT_FAILED,
    MC_CLIENT_SCREEN_INIT_RETIRING,
    MC_CLIENT_SCREEN_INIT_RETIRED
} mc_client_screen_init_phase;

struct mc_client_screen_init_error {
    mc_client_screen_error screen;
    mc_client_screen_site caller, entry;
    mc_platform_calendar_error calendar;
    mc_client_bitmap_font_error font;
};

typedef struct mc_client_screen_init_state {
    mc_client_screen_init_phase phase;
    bool da_entry, app_held, app_counted, date_allocated, calendar_live;
    client_application *app_argument;
    mc_client_source_du *held_particles;
    mc_client_bitmap_font *held_font;
    const mc_platform_calendar *calendar;
    mc_platform_date date;
    unsigned int da_pc, fu_pc, store_count, button_count;
    mc_client_screen_site stores[8];
    const mc_client_source_ke *buttons[5], *replaced_l;
    const mc_translation *translation;
    bool applet_branch, session_present;
    int32_t row_origin;
    const mc_client_screen_init_error *failure, *cleanup_failure;
} mc_client_screen_init_state;

/* Real da.a(App,II): du1/6/h9/b14/q19/g22/c27/d32/e.clear39/virtual b45/return48.
 * Repeated calls create distinct retained records and du owners on the SAME
 * screen/List. Source fields commit in order and are never replayed/rolled back.
 * Each da record and each du owns a separate registered screen-domain App borrow.
 * RAW/null prepared operands use the explicit stable caller contract; RAW storage
 * cannot be registered/retired/freed while any uncounted borrower/view remains.
 * Registered App screen-domain teardown guards are a required dependency.
 * Each nonnull captured q is independently retained; q rebinding does not free
 * this actual g. Previous records/Font/du/Buttons remain until screen retirement.
 * No new App/source layout or current App.r inference is used.
 *
 * Initialization state is separate from constructor READY. Failed prefixes stay
 * attached to the owner; later calls are NEW source invocations. Native arena
 * limits bound retained records, not Java OOME/GC timing. No date injection,
 * dummy screen callback, diagnostic GUI, implicit FP repair or native draw.
 * Strict retirement revokes views only after external Screen holds are refused,
 * retires all children and final namespace cleanup, then drops App borrows last.
 * Busy/errors preserve remaining slots/count flags for retry. Native RETIRING
 * revokes source child views; inspection of initialization records exposes only
 * still-held child slots. Native retirement nulls consumed occurrence slots
 * without crediting source List.clear/modCount or source field stores. */
mc_client_screen_status mc_client_screen_initialize(mc_client_source_da *, client_application *,
                                                    int32_t width, int32_t height,
                                                    mc_client_screen_init_error *);
/* Actual protected fu.b()V entry, without da setup or a clear. It preserves late
 * b265/b405 reads; no early nullable-b validation occurs. Established da fields
 * retain their actual dependencies. Manual RAW/null fixtures must be stable. */
mc_client_screen_status mc_client_screen_menu_initialize(mc_client_source_fu *,
                                                         mc_client_screen_init_error *);
bool mc_client_screen_initialization_inspect(mc_client_source_da *, size_t index,
                                             mc_client_screen_init_state *);

typedef struct mc_client_screen_list_state {
    size_t size, capacity;
    int32_t mod_count;
    bool native_live;
} mc_client_screen_list_state;

bool mc_client_screen_list_inspect(const mc_client_screen_list *, mc_client_screen_list_state *);
bool mc_client_screen_list_button(const mc_client_screen_list *, size_t index,
                                  const mc_client_source_ke **);
#endif
