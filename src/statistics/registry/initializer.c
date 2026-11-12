#include "../achievements/achievements_internal.h"
#include "registry_internal.h"

#include "../../block/registry/registry_internal.h"

/* Actual jl/gg <clinit> instructions, not a final-property table. Each row
 * is executed completely before the following source new instruction. */
typedef struct common_statistic_step {
    int32_t allocation_pc;
    int32_t id;
    const char *key;
    int32_t key_pc;
    int32_t translation_pc;
    mc_statistic_formatter_kind formatter;
    int32_t formatter_pc;
    int32_t constructor_pc;
    int32_t flag_pc;
    int32_t registration_pc;
    int32_t publication_pc;
} common_statistic_step;

static const common_statistic_step common_steps[] = {
    {50, 1000, "stat.startGame", 57, 59, MC_STATISTIC_FORMAT_INTEGER, -1, 62, 65, 68, 71},
    {74, 1001, "stat.createWorld", 81, 83, MC_STATISTIC_FORMAT_INTEGER, -1, 86, 89, 92, 95},
    {98, 1002, "stat.loadWorld", 105, 107, MC_STATISTIC_FORMAT_INTEGER, -1, 110, 113, 116, 119},
    {122, 1003, "stat.joinMultiplayer", 129, 131, MC_STATISTIC_FORMAT_INTEGER, -1, 134, 137, 140,
     143},
    {146, 1004, "stat.leaveGame", 153, 155, MC_STATISTIC_FORMAT_INTEGER, -1, 158, 161, 164, 167},
    {170, 1100, "stat.playOneMinute", 177, 179, MC_STATISTIC_FORMAT_TIME, 182, 185, 188, 191, 194},
    {197, 2000, "stat.walkOneCm", 204, 206, MC_STATISTIC_FORMAT_DISTANCE, 209, 212, 215, 218, 221},
    {224, 2001, "stat.swimOneCm", 231, 233, MC_STATISTIC_FORMAT_DISTANCE, 236, 239, 242, 245, 248},
    {251, 2002, "stat.fallOneCm", 258, 260, MC_STATISTIC_FORMAT_DISTANCE, 263, 266, 269, 272, 275},
    {278, 2003, "stat.climbOneCm", 285, 287, MC_STATISTIC_FORMAT_DISTANCE, 290, 293, 296, 299, 302},
    {305, 2004, "stat.flyOneCm", 312, 314, MC_STATISTIC_FORMAT_DISTANCE, 317, 320, 323, 326, 329},
    {332, 2005, "stat.diveOneCm", 339, 341, MC_STATISTIC_FORMAT_DISTANCE, 344, 347, 350, 353, 356},
    {359, 2006, "stat.minecartOneCm", 366, 368, MC_STATISTIC_FORMAT_DISTANCE, 371, 374, 377, 380,
     383},
    {386, 2007, "stat.boatOneCm", 393, 395, MC_STATISTIC_FORMAT_DISTANCE, 398, 401, 404, 407, 410},
    {413, 2008, "stat.pigOneCm", 420, 422, MC_STATISTIC_FORMAT_DISTANCE, 425, 428, 431, 434, 437},
    {440, 2010, "stat.jump", 447, 449, MC_STATISTIC_FORMAT_INTEGER, -1, 452, 455, 458, 461},
    {464, 2011, "stat.drop", 471, 473, MC_STATISTIC_FORMAT_INTEGER, -1, 476, 479, 482, 485},
    {488, 2020, "stat.damageDealt", 495, 497, MC_STATISTIC_FORMAT_INTEGER, -1, 500, -1, 503, 506},
    {509, 2021, "stat.damageTaken", 516, 518, MC_STATISTIC_FORMAT_INTEGER, -1, 521, -1, 524, 527},
    {530, 2022, "stat.deaths", 537, 539, MC_STATISTIC_FORMAT_INTEGER, -1, 542, -1, 545, 548},
    {551, 2023, "stat.mobKills", 558, 560, MC_STATISTIC_FORMAT_INTEGER, -1, 563, -1, 566, 569},
    {572, 2024, "stat.playerKills", 579, 581, MC_STATISTIC_FORMAT_INTEGER, -1, 584, -1, 587, 590},
    {593, 2025, "stat.fishCaught", 600, 602, MC_STATISTIC_FORMAT_INTEGER, -1, 605, -1, 608, 611}};

_Static_assert(sizeof(common_steps) / sizeof(*common_steps) == 23,
               "jl/gg f..B have twenty-three distinct source publications");

static mc_registry_status diagnostic(mc_registry_error *error, mc_registry_status status,
                                     const char *message) {
    if (error != NULL)
        *error = (mc_registry_error){.status = status,
                                     .message = message,
                                     .initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return status;
}

static mc_registry_status common_statistic(mc_registry_classes *classes,
                                           const common_statistic_step *step, size_t index) {
    mc_registry_initializer_stage(classes, step->allocation_pc);
    mc_statistic *statistic = NULL;
    /* new yd/pm actively initializes vr/ns before evaluating any display
     * argument. The allocator performs that real ancestor active use first. */
    mc_registry_status status =
        mc_registry_statistic_allocate(classes, MC_STATISTIC_GENERAL, &statistic);
    if (status != MC_REGISTRY_OK)
        return status;

    mc_registry_initializer_stage(classes, step->key_pc);
    const mc_registry_string *key = NULL;
    status = mc_registry_literal_ascii(classes, step->key, &key);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, step->translation_pc);
    const mc_registry_string *name = NULL;
    status = mc_registry_translate(classes, key, &name);
    if (status != MC_REGISTRY_OK)
        return status;

    const mc_statistic_formatter *formatter;
    if (step->formatter_pc >= 0) {
        mc_registry_initializer_stage(classes, step->formatter_pc);
        status = mc_registry_statistic_base_active_use(classes);
        if (status != MC_REGISTRY_OK)
            return status;
        formatter = step->formatter == MC_STATISTIC_FORMAT_TIME ? classes->statistic_base.j
                                                                : classes->statistic_base.k;
    } else {
        /* The two-argument source constructor forwards actual vr/ns.i.
         * NULL is a real reference if reached recursively, not a sentinel. */
        formatter = classes->statistic_base.i;
    }
    mc_registry_initializer_stage(classes, step->constructor_pc);
    status = mc_registry_statistic_construct(statistic, step->id, name, formatter, 0);
    if (status != MC_REGISTRY_OK)
        return status;
    if (step->flag_pc >= 0) {
        mc_registry_initializer_stage(classes, step->flag_pc);
        status = mc_registry_statistic_flag(statistic);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    mc_registry_initializer_stage(classes, step->registration_pc);
    /* Actual GENERAL dispatch includes the subtype list append after base
     * registration, even when the bytecode call is declared on vr/ns. */
    status = mc_registry_statistic_register(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, step->publication_pc);
    classes->statistics.named[index] = statistic;
    return MC_REGISTRY_OK;
}

static mc_registry_status initialize(mc_registry_classes *classes) {
    mc_registry_status status = mc_registry_statistic_collections(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    for (size_t index = 0; index < sizeof(common_steps) / sizeof(*common_steps); ++index) {
        status = common_statistic(classes, &common_steps[index], index);
        if (status != MC_REGISTRY_OK)
            return status;
    }

    mc_registry_initializer_stage(classes, 614);
    const mc_registry_string *mine_block = NULL;
    status = mc_registry_literal_ascii(classes, "stat.mineBlock", &mine_block);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, 616); /* int argument 16777216 */
    mc_registry_initializer_stage(classes, 618);
    mc_statistic **blocks = NULL;
    status = mc_registry_statistic_block_array(classes, mine_block, INT32_C(16777216), &blocks);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, 621);
    classes->statistics.C = blocks;
    mc_registry_initializer_stage(classes, 624);
    status = mc_registry_achievements_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    /* ep/cy.a() itself returns immediately after actual class active use. */
    mc_registry_initializer_stage(classes, 628);
    classes->statistics.G = false;
    mc_registry_initializer_stage(classes, 632);
    classes->statistics.H = false;
    mc_registry_initializer_stage(classes, 635);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistics_active_use(mc_registry_classes *classes) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistics active use requires admitted initialization", NULL);
    mc_registry_class_state *state = &classes->statistics_state;
    if (state->phase == MC_REGISTRY_READY || state->phase == MC_REGISTRY_INITIALIZING)
        return MC_REGISTRY_OK;
    if (state->phase == MC_REGISTRY_FAILED) {
        classes->last_error = state->failure;
        classes->last_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "jl", "gg", &state->pc);
    const mc_registry_status status = initialize(classes);
    state->phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    if (status != MC_REGISTRY_OK)
        state->failure = classes->last_error;
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_classes_initialize_statistics(mc_registry_classes *classes,
                                                             mc_registry_error *error) {
    if (classes == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Statistics registry owner");
    if (classes->entered != 0 || classes->statistics_state.phase == MC_REGISTRY_INITIALIZING)
        return diagnostic(error, MC_REGISTRY_BUSY, "registry initialization entered");
    const mc_registry_class_state *state = &classes->statistics_state;
    if (state->phase == MC_REGISTRY_READY)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (state->phase == MC_REGISTRY_FAILED) {
        if (error != NULL) {
            *error = state->failure;
            error->status = MC_REGISTRY_FAILED_INITIALIZATION;
        }
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    ++classes->entered;
    const mc_registry_status status = mc_registry_statistics_active_use(classes);
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}
