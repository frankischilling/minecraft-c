#include "client/startup/diagnostic_builder.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define NEED(x)                                                                                    \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, #x);                                 \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)

int main(void) {
    int result = 1;
    mc_client_diagnostic_builder *b = NULL;
    mc_client_diagnostic_builder_error e;
    mc_client_diagnostic_builder_state state;
    mc_player_string *empty = NULL, *allocated = NULL, *text = NULL, *copy = NULL;
    mc_player_string_view view;
    NEED(mc_client_diagnostic_builder_allocate(&b, &e) == MC_PLAYER_OK);
    NEED(mc_client_diagnostic_builder_construct(b, &e) == MC_PLAYER_OK);
    NEED(mc_client_diagnostic_builder_inspect(b, &state) && state.count == 0 &&
         state.capacity == 16);
    NEED(mc_client_diagnostic_builder_string(b, &empty, &e) == MC_PLAYER_OK);
    NEED(empty == mc_player_empty_literal());
    NEED(mc_player_string_create(NULL, 0, &allocated) == MC_PLAYER_OK && allocated != empty);
    NEED(mc_client_diagnostic_builder_append(b, NULL, &e) == MC_PLAYER_OK);
    NEED(mc_client_diagnostic_builder_append_int(b, INT32_MIN, &e) == MC_PLAYER_OK);
    NEED(mc_client_diagnostic_builder_append_int(b, INT32_MAX, &e) == MC_PLAYER_OK);
    NEED(mc_client_diagnostic_builder_append_int(b, 0, &e) == MC_PLAYER_OK);
    NEED(mc_client_diagnostic_builder_string(b, &text, &e) == MC_PLAYER_OK);
    NEED(mc_client_diagnostic_builder_string(b, &copy, &e) == MC_PLAYER_OK && text != copy);
    NEED(mc_player_string_read(text, &view));
    static const char expected[] = "null-214748364821474836470";
    NEED(view.length == sizeof(expected) - 1u);
    for (size_t i = 0; i < view.length; ++i)
        NEED(view.units[i] == (uint8_t)expected[i]);
    static const uint16_t extra[] = {0, 0xd800, 'x'};
    const mc_player_string_view more = {extra, 3};
    NEED(mc_client_diagnostic_builder_append(b, &more, &e) == MC_PLAYER_OK);
    NEED(mc_player_string_read(text, &view) && view.length == sizeof(expected) - 1u);
    const mc_player_string_view oversized = {extra, MC_PLAYER_MAX_TEXT_UNITS + 1u};
    NEED(mc_client_diagnostic_builder_append(b, &oversized, &e) == MC_PLAYER_LIMIT);
    NEED(mc_client_diagnostic_builder_inspect(b, &state) && state.failed &&
         state.count == sizeof(expected) + 2u);
    result = 0;
done:
    mc_player_string_release(empty);
    mc_player_string_release(allocated);
    mc_player_string_release(text);
    mc_player_string_release(copy);
    if (mc_client_diagnostic_builder_retire(&b, &e) != MC_PLAYER_OK)
        result = 1;
    if (result == 0 && printf("Diagnostic builder: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
