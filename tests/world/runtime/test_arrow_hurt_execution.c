/* New unbundled hurt cohort on the genuine natural-flight World bootstrap.
 * Historical natural-flight controls/captures are not rewritten by this red. */
int arrow_hurt_unused_natural_main(int, char **);
#define MC_ARROW_NATURAL_FLIGHT_MAIN arrow_hurt_unused_natural_main
#include "test_arrow_natural_flight.c"
#undef MC_ARROW_NATURAL_FLIGHT_MAIN

typedef struct arrow_hurt_fixture {
    nf_fixture flight;
    mc_world_access *observer;
    size_t sounds, bowhits;
    bool valid;

    struct {
        char name[48];
        double x, y, z;
        float volume, pitch;
    } sound_rows[4];
} arrow_hurt_fixture;

static mc_world_storage_status ah_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *error) {
    arrow_hurt_fixture *s = context;
    mc_registry_string_view text = {0};
    static const char bowhit[] = "random.drr";
    const size_t row = s->sounds;
    ++s->sounds;
    s->valid &= world == s->flight.world->worlds[0] && volume == 1.f &&
                mc_registry_string_read(name, &text);
    if (row < COUNT(s->sound_rows) && text.length < sizeof(s->sound_rows[row].name)) {
        for (size_t i = 0; i < text.length; ++i)
            s->sound_rows[row].name[i] = text.units[i] < 128 ? (char)text.units[i] : '?';
        s->sound_rows[row].x = x;
        s->sound_rows[row].y = y;
        s->sound_rows[row].z = z;
        s->sound_rows[row].volume = volume;
        s->sound_rows[row].pitch = pitch;
    }
    bool impact = text.length == sizeof(bowhit) - 1;
    for (size_t i = 0; impact && i < text.length; ++i)
        impact = text.units[i] == (uint16_t)bowhit[i];
    if (impact) {
        ++s->bowhits;
        s->valid &= mc_entity_living_fields(s->flight.victim)->Y == 6 && !s->flight.arrow->base.be;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool ah_pig_impact(arrow_hurt_fixture *s) {
    bool ok = false, inserted = false;
    nf_fixture *n = &s->flight;
    const mc_entity_class_literal *kind = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(n->world->entity_classes[0], MC_ENTITY_CLASS_KIND_PIG,
                                      &kind) == MC_ENTITY_OK &&
          mc_mob_allocate(n->world->entity_classes[0], n->world->registry[0], kind, &n->victim,
                          &error) == MC_MOB_OK &&
          mc_mob_construct(n->victim, n->world->worlds[0], &error) == MC_MOB_OK &&
          mc_entity_set_position(n->victim, 9.6, 65.75, 8.5) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(n->world->worlds[0], n->victim, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted && mc_entity_living_fields(n->victim)->Y == 10);
    const mc_world_access_callbacks callbacks = {.sound = ah_sound};
    CHECK(mc_world_access_create(&callbacks, s, &s->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(n->world->worlds[0], s->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    const mc_world_storage_status status = mc_world_runtime_tick_entity(
        n->world->worlds[0], n->arrow, &tick_context, NULL, tick_error);
    const bool observed =
        status == MC_WORLD_STORAGE_OK && mc_entity_living_fields(n->victim)->Y == 6 &&
        !mc_entity_living_fields(n->victim)->ah && s->valid && s->sounds == 2 && s->bowhits == 1 &&
        n->arrow->base.be && n->arrow->base.bt == 1 && n->arrow->arrow->fields.k == 1 &&
        ad_bits(n->arrow->base.x) == UINT64_C(0x4022333334000000) &&
        ad_bits(n->arrow->base.velocity_x) == UINT64_C(0x3fe3020c5978d500) &&
        ad_bits(n->arrow->base.velocity_y) == UINT64_C(0xbf9eb851e0000000);
    /* Always perform genuine checked cleanup before reporting this red. */
    const bool retired = mc_world_entity_tick_error_retire(tick_error);
    if (!observed) {
        fprintf(stderr,
                "Arrow Pig impact: status=%u health=%d sounds=%zu bowhits=%zu dead=%d "
                "cleanup=%d; expected actual damage4 and full impact tail\n",
                (unsigned int)status, mc_entity_living_fields(n->victim)->Y, s->sounds, s->bowhits,
                n->arrow->base.be, retired);
    }
    CHECK(retired && n->arrow->arrow->tick_call == NULL);
    CHECK(observed);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    arrow_hurt_fixture s = {0};
    char root[8192];
    if (argc != 3 || (strcmp(argv[1], "pig-impact") != 0 && strcmp(argv[1], "natural-pig") != 0) ||
        !decode_path(argv[2], root, sizeof(root)))
        return 2;
    printf("ARROW_HURT_EXECUTION_BEGIN mode=%s\n", argv[1]);
    fflush(stdout);
    s.valid = true;
    CHECK(nf_setup(&s.flight, root, NULL) && nf_construct(&s.flight));
    CHECK(strcmp(argv[1], "natural-pig") == 0 ? nf_victim(&s.flight) : ah_pig_impact(&s));
    result = 0;
cleanup:
    if (s.observer != NULL) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(s.flight.world->worlds[0], s.observer, &removed,
                                                 NULL) != MC_WORLD_STORAGE_OK ||
            !removed || mc_world_access_free(s.observer, NULL) != MC_WORLD_STORAGE_OK)
            return 1;
    }
    if (!nf_cleanup(&s.flight)) {
        fputs("Arrow hurt cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("PASS: %u actual Arrow hurt integration checks;mode=%s\n", checks, argv[1]);
    return result;
}
