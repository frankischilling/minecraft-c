#include "turn_fixture.h"

#include <inttypes.h>
#include <stdio.h>

#ifdef _WIN32
#define SCAN_NUMBERS sscanf_s
#else
#define SCAN_NUMBERS sscanf
#endif

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    FILE *plan = NULL;
#ifdef _WIN32
    if (fopen_s(&plan, argv[1], "rb") != 0)
        return 2;
#else
    plan = fopen(argv[1], "rb");
    if (plan == NULL)
        return 2;
#endif
    int exit_code = 2;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL;
    if (mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK ||
        mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) != MC_ENTITY_OK)
        goto cleanup;
    char line[256];
    size_t ordinal = 0;
    while (fgets(line, (int)sizeof(line), plan) != NULL) {
        unsigned int kind, repeat;
        uint32_t angles[4], delta[2];
        if (strchr(line, '\n') == NULL ||
            SCAN_NUMBERS(line,
                         "%u %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32
                         " %u",
                         &kind, &angles[0], &angles[1], &angles[2], &angles[3], &delta[0],
                         &delta[1], &repeat) != 8 ||
            kind > 1 || repeat < 1 || repeat > 6)
            goto cleanup;
        if ((kind == 0 ? mc_item_entity_allocate(classes, &entity, NULL)
                       : mc_falling_entity_allocate(classes, &entity, NULL)) != MC_ENTITY_OK ||
            (kind == 0 ? mc_item_entity_construct_world(entity, NULL, NULL)
                       : mc_falling_entity_construct_world(entity, NULL, NULL)) != MC_ENTITY_OK)
            goto cleanup;
        turn_prepare(entity, angles);
        for (unsigned int step = 0; step < repeat; ++step) {
            turn_guard guard;
            if (!turn_capture(entity, &guard) ||
                mc_entity_turn(entity, turn_float(delta[0]), turn_float(delta[1])) !=
                    MC_ENTITY_OK ||
                !turn_unchanged_other_fields(entity, &guard))
                goto cleanup;
            if (printf("%zu %u %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " 1\n",
                       ordinal, step, turn_bits(entity->base.yaw), turn_bits(entity->base.aT),
                       turn_bits(entity->base.aU), turn_bits(entity->base.aV)) < 0)
                goto cleanup;
        }
        mc_entity_release(entity);
        entity = NULL;
        ++ordinal;
    }
    if (!ferror(plan) && !ferror(stdout) && ordinal != 0)
        exit_code = 0;
cleanup:
    if (fclose(plan) != 0)
        exit_code = 2;
    mc_entity_release(entity);
    if (mc_entity_classes_free(classes) != MC_ENTITY_OK)
        exit_code = 2;
    mc_java_random_profile_free(profile);
    return exit_code;
}
