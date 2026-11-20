/* Prepared genuine N/DK/Ge owners; only the entered polling child is called. */
#include "client/application_state.h"
#include "client/render_engine/ge.h"
#include "client/render_engine/internal.h"
#include "client/world/source_terrain_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "N poll check%u at%d: %s\n", checks, __LINE__, #value);                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

typedef struct input {
    const char *name, *result;
    int32_t start, end;
    bool null_array, buffer;
    size_t length;
    unsigned int ids[15], pc;
} input;

static const input cases[] = {
    {"empty_null_array", "OK", 0, 0, true, false, 0, {0}, 123},
    {"reverse_null_array", "OK", 2, 1, true, true, 0, {0}, 123},
    {"null_array", "NULL", 0, 1, true, false, 0, {0}, 12},
    {"empty_array", "INDEX", 0, 1, false, true, 0, {0}, 12},
    {"negative_index", "INDEX", -1, 1, false, true, 1, {1}, 12},
    {"past_end", "INDEX", 1, 2, false, true, 1, {1}, 12},
    {"null_cell", "NULL", 0, 1, false, true, 1, {0}, 13},
    {"idle_null_buffer", "OK", 0, 1, false, false, 1, {1}, 123},
    {"two_idle_null_buffer", "OK", 0, 2, false, false, 2, {1, 3}, 123},
    {"idle_then_null", "NULL", 0, 2, false, true, 2, {1, 0}, 13},
    {"waiting_null_buffer", "NULL", 0, 1, false, false, 1, {2}, 23},
    {"idle_then_waiting_null_buffer", "NULL", 0, 2, false, false, 2, {1, 2}, 23},
    {"skipped_waiting", "OK", 1, 2, false, true, 2, {2, 1}, 123},
    {"initial16_unclamped",
     "INDEX",
     0,
     16,
     false,
     true,
     15,
     {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
     12},
};

int main(void) {
    mc_client_render_engine_classes *graphics = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error error;
    CHECK(mc_client_render_global_allocate(graphics, NULL, &owner, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    client_application app = {0};
    owner->fields.t = &app;
    mc_client_source_dk *actors[3];
    for (size_t i = 0; i < 3; ++i) {
        actors[i] = mc_source_dk_allocate(owner, &error);
        CHECK(actors[i] != NULL);
        actors[i]->fields.x = i != 0;
        actors[i]->fields.y = i == 1;
    }
    owner->fields.M = 11;
    owner->fields.N = 12;
    owner->fields.O = 13;
    owner->fields.P = 14;
    owner->fields.Q = 15;
    mc_client_direct_int_buffer *buffer = NULL;
    const mc_client_graphics_site prepared = {"fixture", "poll_inputs", "()V", -1};
    CHECK(mc_client_ge_d(graphics, &owner->arena, 2, prepared, &buffer, &error.graphics));
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        const input *in = &cases[i];
        owner->fields.n = NULL;
        owner->n_length = in->length;
        if (!in->null_array) {
            owner->fields.n = mc_render_global_allocate(
                owner, in->length * sizeof(*owner->fields.n), prepared, &error);
            CHECK(owner->fields.n != NULL);
            for (size_t j = 0; j < in->length; ++j)
                owner->fields.n[j] = in->ids[j] == 0 ? NULL : actors[in->ids[j] - 1];
        }
        owner->fields.c = in->buffer ? buffer : NULL;
        CHECK(mc_client_ge_int_clear(buffer, prepared, &error.graphics));
        CHECK(mc_client_ge_int_position(buffer, 1, prepared, &error.graphics));
        CHECK(mc_client_ge_int_limit(buffer, 1, prepared, &error.graphics));
        mc_client_render_global_submit_call call = {0};
        call.state.owner = owner;
        call.state.frames = call.frames;
        call.state.failure = &call.failure;
        call.state.cleanup = &call.cleanup;
        call.state.active = owner->active = true;
        CHECK(mc_terrain_push(&call, "n", "a", "(Lls;ID)I"));
        const bool returned = mc_terrain_poll(&call, in->start, in->end, 357);
        call.state.active = owner->active = false;
        const bool success = strcmp(in->result, "OK") == 0;
        CHECK(returned == success && call.state.source_pc == in->pc);
        if (success) {
            CHECK(call.failure.status == MC_CLIENT_RENDER_GLOBAL_OK && call.depth == 1);
        } else {
            CHECK(call.depth == 2 && call.frames[0].pc == 357 &&
                  strcmp(call.frames[1].class_name, "n") == 0 &&
                  strcmp(call.frames[1].descriptor, "(II)V") == 0 &&
                  call.frames[1].pc == (int32_t)in->pc && !call.failure.native_policy);
            if (strcmp(in->result, "NULL") == 0)
                CHECK(call.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE);
            else
                CHECK(call.failure.status == MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX);
        }
        CHECK(call.state.query_polls == 0 && call.state.queries_issued == 0 && call.query == NULL &&
              !call.state.query_active && !call.state.matrix_pushed);
        CHECK(owner->fields.M == 11 && owner->fields.N == 12 && owner->fields.O == 13 &&
              owner->fields.P == 14 && owner->fields.Q == 15);
        CHECK(!actors[0]->fields.x && !actors[0]->fields.y && actors[1]->fields.x &&
              actors[1]->fields.y && actors[2]->fields.x && !actors[2]->fields.y);
        CHECK(buffer->position == 1 && buffer->limit == 1);
        printf("POLL %s %s 00,11,10 %d %d\n", in->name, in->result, in->buffer ? 1 : -1,
               in->buffer ? 1 : -1);
        if (i == sizeof(cases) / sizeof(*cases) - 1) {
            /* Additional bytecode-derived whole-branch prefix control: the
             * initial0..16 poll must fail before the source x=true loop. */
            mc_client_render_global_submit_call outer = {0};
            outer.state.owner = owner;
            outer.state.frames = outer.frames;
            CHECK(mc_terrain_push(&outer, "n", "a", "(Lls;ID)I"));
            owner->active = outer.state.active = true;
            int32_t unchanged = 91;
            CHECK(!mc_terrain_occlusion(&outer, &unchanged));
            owner->active = outer.state.active = false;
            CHECK(unchanged == 91 && outer.failure.status == MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX &&
                  outer.state.source_pc == 12 && outer.frames[0].pc == 357 &&
                  !actors[0]->fields.x && outer.state.query_polls == 0 &&
                  outer.state.queries_issued == 0 && !outer.state.matrix_pushed &&
                  !outer.state.query_active && outer.query == NULL);
        }
    }
    CHECK(mc_client_render_global_release(&owner, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    printf("%u N closed poll checks passed\n", checks);
    return 0;
}
