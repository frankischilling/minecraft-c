#include "mc/client/render/biped.h"
#include "mc/client/render/model.h"
#include "random_profile_internal.h"
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FP control line%d\n", __LINE__);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    CHECK(argc == 2);
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_classes *classes = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, NULL, 0) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(entities, profile, &classes) == MC_CLIENT_MODEL_OK);
    const unsigned before = _mm_getcsr();
    if (strcmp(argv[1], "item") == 0) {
        mc_client_item_renderer *item = NULL;
        mc_client_model_error error;
        CHECK(mc_client_item_renderer_allocate(classes, 0, &item) == MC_CLIENT_MODEL_OK);
        _mm_setcsr(0x9fc0u);
        const mc_client_model_status status = mc_client_item_renderer_construct(item, &error);
        _mm_setcsr(before);
        CHECK(status == MC_CLIENT_MODEL_VECTOR_FAILED &&
              error.vector.status == MC_ENTITY_UNSUPPORTED_FP && error.vector.bytecode == 8);
        const mc_client_item_renderer_fields *fields = mc_client_item_renderer_fields_get(item);
        mc_entity_vector_snapshot vectors;
        CHECK(mc_entity_vector_get_snapshot(entities, &vectors) == MC_ENTITY_OK);
        CHECK(fields->base_model == NULL && fields->base_blocks == NULL && fields->e == NULL &&
              fields->f == NULL);
        printf("LEGACY_FP %d %s %d %d %u %d %zu %d %zu %zu\n", status, error.source_class,
               error.source_pc, error.vector.status, (unsigned)error.vector.bytecode, vectors.phase,
               vectors.holds, vectors.cursor, vectors.size,
               mc_client_item_renderer_allocation_bytes(item));
        CHECK(mc_client_item_renderer_free(&item) == MC_CLIENT_MODEL_OK);
    } else {
#if !defined(MC_BIPED_OLD_ONLY)
        CHECK(strcmp(argv[1], "biped") == 0);
        mc_client_biped_owner *owner = NULL;
        mc_client_biped_error error;
        mc_client_biped_state state;
        CHECK(mc_client_biped_allocate(classes, 0, &owner, NULL) == MC_CLIENT_MODEL_OK);
        _mm_setcsr(0x9fc0u);
        const mc_client_model_status status = mc_client_biped_construct_zero(owner, &error);
        _mm_setcsr(before);
        CHECK(status == MC_CLIENT_MODEL_VECTOR_FAILED &&
              error.model.vector.status == MC_ENTITY_UNSUPPORTED_FP &&
              error.model.vector.bytecode == 8);
        CHECK(mc_client_biped_inspect(owner, &state) && state.phase == MC_CLIENT_BIPED_FAILED &&
              state.allocations == 4 && state.last_root_store.pc == 29);
        CHECK(state.fields->i != NULL && state.fields->i->j != NULL && state.fields->i->k != NULL);
        for (size_t i = 0; i < 8; ++i)
            CHECK(state.fields->i->j[i] == NULL);
        for (size_t i = 0; i < 6; ++i)
            CHECK(state.fields->i->k[i] == NULL);
        CHECK(error.frame_count == 4 && error.frames[0].pc == 7 && error.frames[1].pc == 101 &&
              error.frames[2].pc == 47 && error.frames[3].pc == 3);
        puts("BIPED_FP delayed ib7 bt8; i29 j/k published; eight/six NULL slots; four nodes "
             "retained");
        CHECK(mc_client_biped_release(&owner, NULL) == MC_CLIENT_MODEL_OK && owner == NULL);
#else
        CHECK(false);
#endif
    }
    CHECK(mc_client_render_classes_free(classes) == MC_CLIENT_MODEL_OK &&
          mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    return 0;
}
