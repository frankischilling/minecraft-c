/* Borrow genuine Registry/Session/GAME World construction helpers. The old
 * executable and its authored entropy/limit controls are never invoked. */
#define main named_unused_mob_main
#include "../test_mob_construction.c"
#undef main

#include "../../reference/entity/named_factory/native-observations.h"
#include "mc/entity/factory/factory.h"
#include "world/runtime/construction_internal.h"

typedef struct named_world {
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    bool retired;
} named_world;

static mc_nbt_string named_text(const char *text, uint16_t *units) {
    size_t n = strlen(text);
    for (size_t i = 0; i < n; ++i)
        units[i] = (unsigned char)text[i];
    return (mc_nbt_string){units, n};
}

static bool named_random(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           dbits(a->next_next_gaussian) == dbits(b->next_next_gaussian);
}

static bool named_second_world(fixture *f, const char *directory, named_world *other) {
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options store = {.session_token = 6382,
                                          .block_map = map,
                                          .block_map_length = 256,
                                          .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &store, &other->store, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_provider_create(0, NULL, &other->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &other->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = other->store, .name = f->name, .seed = 24, .dimension = other->dimension};
    CHECK(mc_world_runtime_construct_world(other->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool named_retire_world(named_world *other) {
    if (!other->retired) {
        CHECK(mc_world_runtime_retire_entity_containers(other->world, NULL) == MC_WORLD_STORAGE_OK);
        mc_world_provider_unbind(other->dimension);
        CHECK(mc_world_runtime_retire_world(other->world, NULL) == MC_WORLD_STORAGE_OK);
        other->retired = true;
    }
    return true;
}

typedef struct named_stable {
    mc_entity_snapshot entity;
    mc_mob_state mob;
    mc_entity_bounds bounds;
    mc_entity_watcher_snapshot watcher;
    mc_java_random random;
    uint64_t unique, math;
} named_stable;

static bool named_take(fixture *f, mc_entity *entity, named_stable *out) {
    CHECK(mc_entity_get_snapshot(entity, &out->entity) == MC_ENTITY_OK &&
          mc_mob_inspect(entity, &out->mob) && entity->random != NULL && entity->bounds != NULL &&
          mc_entity_watcher_get_snapshot(entity->watcher, &out->watcher) == MC_ENTITY_OK);
    out->bounds = *entity->bounds;
    out->random = *entity->random;
    out->unique = atomic_load(&f->profile->uniquifier);
    out->math = atomic_load(&f->profile->math->state);
    return true;
}

static bool named_same(fixture *f, mc_entity *entity, const named_stable *before,
                       mc_world_runtime *world) {
    named_stable after;
    CHECK(named_take(f, entity, &after));
    const mc_entity_base_fields *a = &before->entity.base, *b = &after.entity.base;
#define SAME_BASE(field) CHECK(a->field == b->field)
    SAME_BASE(id);
    SAME_BASE(aF);
    SAME_BASE(aG);
    SAME_BASE(aH);
    SAME_BASE(aX);
    SAME_BASE(aY);
    SAME_BASE(aZ);
    SAME_BASE(ba);
    SAME_BASE(bb);
    SAME_BASE(bc);
    SAME_BASE(bd);
    SAME_BASE(be);
    SAME_BASE(private_b);
    SAME_BASE(bq);
    SAME_BASE(bt);
    SAME_BASE(bu);
    SAME_BASE(bv);
    SAME_BASE(bw);
    SAME_BASE(bx);
    SAME_BASE(by);
    SAME_BASE(bz);
    SAME_BASE(private_c);
    SAME_BASE(bC);
    SAME_BASE(bF);
    SAME_BASE(bG);
    SAME_BASE(bH);
    SAME_BASE(bI);
    SAME_BASE(bM);
    SAME_BASE(client_only.bA);
    SAME_BASE(client_only.bB);
    SAME_BASE(client_only.bJ);
    SAME_BASE(client_only.bK);
    SAME_BASE(client_only.bL);
#undef SAME_BASE
#define SAME_DOUBLE(field) CHECK(dbits(a->field) == dbits(b->field))
    SAME_DOUBLE(aE);
    SAME_DOUBLE(aJ);
    SAME_DOUBLE(aK);
    SAME_DOUBLE(aL);
    SAME_DOUBLE(x);
    SAME_DOUBLE(y);
    SAME_DOUBLE(z);
    SAME_DOUBLE(velocity_x);
    SAME_DOUBLE(velocity_y);
    SAME_DOUBLE(velocity_z);
    SAME_DOUBLE(bl);
    SAME_DOUBLE(bm);
    SAME_DOUBLE(bn);
    SAME_DOUBLE(private_d);
    SAME_DOUBLE(private_e);
#undef SAME_DOUBLE
#define SAME_FLOAT(field) CHECK(fbits(a->field) == fbits(b->field))
    SAME_FLOAT(yaw);
    SAME_FLOAT(aT);
    SAME_FLOAT(aU);
    SAME_FLOAT(aV);
    SAME_FLOAT(offset);
    SAME_FLOAT(width);
    SAME_FLOAT(height);
    SAME_FLOAT(bi);
    SAME_FLOAT(bj);
    SAME_FLOAT(bk);
    SAME_FLOAT(y_size);
    SAME_FLOAT(bp);
    SAME_FLOAT(br);
    SAME_FLOAT(bE);
#undef SAME_FLOAT
#define SAME_BOUNDS(field) CHECK(dbits(before->bounds.field) == dbits(after.bounds.field))
    SAME_BOUNDS(min_x);
    SAME_BOUNDS(min_y);
    SAME_BOUNDS(min_z);
    SAME_BOUNDS(max_x);
    SAME_BOUNDS(max_y);
    SAME_BOUNDS(max_z);
#undef SAME_BOUNDS
    CHECK(
        after.entity.base.world == world &&
        before->entity.class_literal == after.entity.class_literal &&
        before->entity.phase == after.entity.phase &&
        before->entity.bounds == after.entity.bounds &&
        before->entity.random == after.entity.random &&
        before->entity.watcher == after.entity.watcher &&
        named_random(&before->random, &after.random) && before->unique == after.unique &&
        before->math == after.math && fbits(before->mob.living.aj) == fbits(after.mob.living.aj) &&
        fbits(before->mob.living.F) == fbits(after.mob.living.F) &&
        fbits(before->mob.living.G) == fbits(after.mob.living.G) &&
        before->mob.living.Y == after.mob.living.Y && before->mob.living.O == after.mob.living.O &&
        before->watcher.record == after.watcher.record &&
        before->watcher.size == after.watcher.size &&
        before->watcher.byte_value == after.watcher.byte_value &&
        before->watcher.dirty == after.watcher.dirty &&
        before->mob.watcher_16 == after.mob.watcher_16 &&
        before->mob.watcher_16_dirty == after.mob.watcher_16_dirty &&
        before->mob.watcher_dirty == after.mob.watcher_dirty);
    return true;
}

static bool named_frontiers(fixture *f, mc_entity *marker) {
    uint16_t units[32];
    mc_entity *out = marker;
    mc_entity_factory_error e;
    const int32_t id = f->entities->next_id;
    const size_t live = f->entities->live_entities, bindings = f->world->persistent_bindings;
    const uint64_t unique = atomic_load(&f->profile->uniquifier);
    const uint64_t math = atomic_load(&f->profile->math->state);
    CHECK(mc_entity_factory_named(f->entities, f->registry, named_text("Mob", units), f->world,
                                  &out, &e) == MC_ENTITY_FACTORY_UNIMPLEMENTED &&
          out == marker && e.bytecode == 52 && e.native_policy && !e.source_exception &&
          e.exception_class == NULL &&
          strcmp(e.caught_exception_class, "java/lang/InstantiationException") == 0 &&
          f->entities->next_id == id && f->entities->live_entities == live &&
          f->world->persistent_bindings == bindings &&
          atomic_load(&f->profile->uniquifier) == unique &&
          atomic_load(&f->profile->math->state) == math);
    CHECK(mc_entity_factory_named(f->entities, f->registry, named_text("Snowball", units), NULL,
                                  &out, &e) == MC_ENTITY_FACTORY_UNIMPLEMENTED &&
          out == marker && e.bytecode == 40 && e.caught_exception_class == NULL &&
          f->entities->next_id == id);
    mc_entity_classes *limited = NULL;
    const mc_entity_limits limits = {MC_ENTITY_DEFAULT_ALLOCATION_BYTES, 0};
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, &limits, &limited) ==
          MC_ENTITY_OK);
    CHECK(mc_entity_factory_named(limited, f->registry, named_text("Item", units), NULL, &out,
                                  &e) == MC_ENTITY_FACTORY_LIMIT &&
          out == marker && e.bytecode == 40 && e.native_policy && !e.source_exception &&
          e.caught_exception_class == NULL && e.construction.status == MC_ENTITY_LIMIT &&
          limited->entity_phase == MC_ENTITY_CLASS_READY && limited->next_id == 0 &&
          limited->live_entities == 0);
    CHECK(mc_entity_classes_free(limited) == MC_ENTITY_OK);
    limited = NULL;
    const mc_entity_limits map_limit = {sizeof(mc_entity_classes), SIZE_MAX};
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, &map_limit, &limited) ==
          MC_ENTITY_OK);
    CHECK(mc_entity_factory_named(limited, NULL, named_text("unknown", units), NULL, &out, &e) ==
              MC_ENTITY_FACTORY_LIMIT &&
          out == marker && e.bytecode == 0 && e.registry.status == MC_ENTITY_REGISTRY_LIMIT &&
          limited->entity_phase == MC_ENTITY_CLASS_COLD);
    const mc_entity_registry_error first_failure = e.registry;
    CHECK(mc_entity_factory_named(limited, NULL, named_text("Pig", units), NULL, &out, &e) ==
              MC_ENTITY_FACTORY_REGISTRY_FAILED &&
          out == marker && e.bytecode == 0 &&
          e.registry.status == MC_ENTITY_REGISTRY_CLASS_FAILED &&
          e.registry.cause == first_failure.cause &&
          e.registry.initializer_bytecode == first_failure.initializer_bytecode);
    CHECK(mc_entity_classes_free(limited) == MC_ENTITY_OK);
    return true;
}

static bool named_skeleton(fixture *f, mc_entity *marker) {
    uint16_t units[16];
    const mc_nbt_string name = named_text("Skeleton", units);
    mc_entity *out = marker, *created = NULL;
    mc_entity_factory_error e;
    CHECK(mc_entity_factory_named(f->entities, f->registry, name, NULL, &created, &e) ==
              MC_ENTITY_FACTORY_OK &&
          created != NULL && e.mob_selected && created->base.world == NULL);
    mc_entity_release(created);
    created = NULL;
    CHECK(mc_entity_factory_named(f->entities, NULL, name, NULL, &created, &e) ==
              MC_ENTITY_FACTORY_OK &&
          created != NULL);
    mc_entity_release(created);
    mc_entity_classes *classes = NULL;
    mc_registry_classes *cold = NULL;
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &classes) == MC_ENTITY_OK &&
          registry_new(f, f->profile, &cold, false));
    CHECK(mc_entity_factory_named(classes, cold, name, NULL, &out, &e) ==
              MC_ENTITY_FACTORY_CONSTRUCTOR_FAILED &&
          out == marker && e.bytecode == 40 && !e.native_policy && e.source_exception &&
          strcmp(e.exception_class, "java/lang/ExceptionInInitializerError") == 0 &&
          e.caught_exception_class == NULL && e.mob_selected && e.mob_call.site.pc == 4 &&
          strcmp(e.mob_call.site.class_name, "fr") == 0 && classes->next_id == 0 &&
          classes->live_entities == 0);
    const mc_mob_error original = e.mob_call;
    const size_t bytes = classes->allocated_bytes;
    CHECK(mc_entity_factory_named(classes, f->registry, name, NULL, &out, &e) ==
              MC_ENTITY_FACTORY_CONSTRUCTOR_FAILED &&
          out == marker && e.source_exception && !e.native_policy && e.bytecode == 40 &&
          strcmp(e.exception_class, "java/lang/NoClassDefFoundError") == 0 &&
          e.caught_exception_class == NULL &&
          e.mob_call.first_class_failure.status == original.first_class_failure.status &&
          e.mob_call.first_class_failure.site.pc == original.first_class_failure.site.pc &&
          classes->allocated_bytes == bytes && classes->next_id == 0 &&
          classes->live_entities == 0);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK &&
          mc_registry_classes_free(cold, NULL) == MC_REGISTRY_OK);
    return true;
}

static bool named_constructor_failure(fixture *f, mc_entity *marker) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_registry_classes *registry = NULL;
    mc_entity *out = marker;
    mc_entity_factory_error e;
    uint16_t units[3];
    /* Real configured native Math allocation refusal inside the constructor,
     * after SN's actual ID/Random/watcher prefix; never a Java catch/NULL. */
    CHECK(mc_java_random_profile_create(sizeof(*profile), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK &&
          registry_new(f, profile, &registry, true));
    CHECK(mc_entity_factory_named(classes, registry, named_text("Pig", units), NULL, &out, &e) ==
              MC_ENTITY_FACTORY_CONSTRUCTOR_FAILED &&
          out == marker && e.bytecode == 40 && e.native_policy && !e.source_exception &&
          e.caught_exception_class == NULL && e.mob_selected &&
          e.mob_call.entity.profile == MC_JAVA_RANDOM_PROFILE_LIMIT && classes->next_id == 1 &&
          classes->live_entities == 0 && profile->math == NULL);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK &&
          mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    mc_java_random_profile_free(profile);
    return true;
}

static bool named_cases(fixture *f, named_world *other) {
    uint16_t units[32], equal_pig[] = {'P', 'i', 'g'};
    mc_entity_factory_error e;
    mc_entity *out = NULL, *pigs[3] = {0};
    mc_entity_registry_view maps;
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(f->entities, &before) == MC_ENTITY_OK &&
          before.next_id == 0);
    const char *unknown[] = {"unknown", "", "pig"};
    for (size_t i = 0; i < 3; ++i)
        CHECK(mc_entity_factory_named(f->entities, NULL, named_text(unknown[i], units), NULL, &out,
                                      &e) == MC_ENTITY_FACTORY_OK &&
              out == NULL && e.bytecode == 56 && !e.native_policy);
    CHECK(mc_entity_factory_named(f->entities, NULL, (mc_nbt_string){0}, NULL, &out, &e) ==
              MC_ENTITY_FACTORY_OK &&
          out == NULL && mc_entity_classes_get_snapshot(f->entities, &after) == MC_ENTITY_OK &&
          after.next_id == before.next_id && after.entity_class == before.entity_class &&
          mc_entity_registry_inspect(f->entities, &maps) == MC_ENTITY_REGISTRY_OK &&
          maps.phase == MC_ENTITY_CLASS_READY);
    for (size_t i = 0; i < 4; ++i)
        CHECK(maps.maps[i] != NULL && maps.counts[i] == 24);
    for (size_t i = 0; i < 3; ++i) {
        const mc_named_factory_pig_reference *r = &mc_named_factory_pig_references[i];
        mc_nbt_string name = i == 1 ? (mc_nbt_string){equal_pig, 3} : named_text("Pig", units);
        CHECK(mc_entity_factory_named(f->entities, i == 1 ? NULL : f->registry, name,
                                      r->world_nonnull ? f->world : NULL, &pigs[i],
                                      &e) == MC_ENTITY_FACTORY_OK &&
              pigs[i] != NULL && e.bytecode == 56 && e.mob_selected);
        mc_entity_snapshot entity;
        mc_mob_state mob_state;
        mc_entity_watcher_snapshot watcher;
        mc_player_string_view texture;
        CHECK(mc_entity_get_snapshot(pigs[i], &entity) == MC_ENTITY_OK &&
              mc_mob_inspect(pigs[i], &mob_state) && entity.phase == MC_ENTITY_READY &&
              entity.base.id == r->expected_id &&
              entity.base.world == (r->world_nonnull ? f->world : NULL) &&
              fbits(entity.base.width) == r->width_bits &&
              fbits(entity.base.height) == r->height_bits && mob_state.living.Y == r->health &&
              mc_player_string_read(mob_state.living.O, &texture) &&
              texture.length == strlen(r->texture_ascii) &&
              mc_entity_watcher_get_snapshot(entity.watcher, &watcher) == MC_ENTITY_OK &&
              watcher.byte_value == r->watcher0 && mob_state.watcher_16_present &&
              mob_state.watcher_16 == r->watcher16);
        for (size_t j = 0; j < texture.length; ++j)
            CHECK(texture.units[j] == (unsigned char)r->texture_ascii[j]);
        for (size_t j = 0; j < i; ++j)
            CHECK(pigs[i] != pigs[j]);
    }
    named_stable stable;
    CHECK(named_take(f, pigs[0], &stable));
    const mc_named_factory_world_setter_reference *setter =
        &mc_named_factory_world_setter_reference_value;
    CHECK(stable.entity.base.id == setter->expected_id &&
          (stable.entity.base.world != NULL) == setter->world_before_nonnull);
    const size_t bindings = f->world->persistent_bindings,
                 other_bindings = other->world->persistent_bindings;
    mc_entity_error world_error;
    CHECK(mc_entity_set_world(pigs[0], f->world, &world_error) == MC_ENTITY_OK &&
          world_error.bytecode == 5 && setter->world_after_nonnull &&
          named_same(f, pigs[0], &stable, f->world) &&
          f->world->persistent_bindings == bindings + 1);
    CHECK(mc_entity_set_world(pigs[0], f->world, &world_error) == MC_ENTITY_OK &&
          f->world->persistent_bindings == bindings + 1 &&
          named_same(f, pigs[0], &stable, f->world));
    CHECK(mc_entity_set_world(pigs[0], other->world, &world_error) == MC_ENTITY_OK &&
          f->world->persistent_bindings == bindings &&
          other->world->persistent_bindings == other_bindings + 1 &&
          named_same(f, pigs[0], &stable, other->world));
    CHECK(mc_entity_set_world(pigs[0], NULL, &world_error) == MC_ENTITY_OK &&
          other->world->persistent_bindings == other_bindings &&
          named_same(f, pigs[0], &stable, NULL));
    CHECK(mc_entity_set_world(pigs[0], f->world, &world_error) == MC_ENTITY_OK &&
          named_same(f, pigs[0], &stable, f->world));
    CHECK(named_retire_world(other));
    /* Native retirement consumes the Dimension's own World binding before
     * this setter invocation. Compare refusal against that genuine poststate. */
    const size_t retired_bindings = other->world->persistent_bindings;
    CHECK(other_bindings == retired_bindings + 1 &&
          mc_world_provider_world(other->dimension) == NULL &&
          other->world->construction.phase == MC_WORLD_RUNTIME_RETIRED);
    const mc_entity_status retired_status =
        mc_entity_set_world(pigs[0], other->world, &world_error);
    const bool retired_stable = named_same(f, pigs[0], &stable, f->world);
    CHECK(retired_status == MC_ENTITY_WORLD_FAILED && world_error.bytecode == 2 &&
          world_error.world == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          other->world->persistent_bindings == retired_bindings &&
          f->world->persistent_bindings == bindings + 1 && retired_stable);
    out = pigs[0];
    CHECK(mc_entity_factory_named(f->entities, NULL, named_text("unknown", units), other->world,
                                  &out, &e) == MC_ENTITY_FACTORY_OK &&
          out == NULL); /* Unknown never touches even this actual retired World. */
    CHECK(named_frontiers(f, pigs[0]) && named_skeleton(f, pigs[0]) &&
          named_constructor_failure(f, pigs[0]));
    static const char *const constructor_names[] = {"Item", "FallingSand", "Minecart", "Arrow",
                                                    "PrimedTnt"};
    for (size_t i = 0; i < sizeof(constructor_names) / sizeof(constructor_names[0]); ++i) {
        out = NULL;
        CHECK(mc_entity_factory_named(f->entities, f->registry,
                                      named_text(constructor_names[i], units), NULL, &out,
                                      &e) == MC_ENTITY_FACTORY_OK &&
              out != NULL && out->phase == MC_ENTITY_READY && out->base.world == NULL);
        mc_entity_release(out);
    }
    for (size_t i = 0; i < 3; ++i)
        mc_entity_release(pigs[i]);
    CHECK(f->world->fields.b->count == 0 && f->world->fields.d->count == 0 &&
          other->world->fields.b->count == 0 && other->world->fields.d->count == 0 &&
          f->world->persistent_bindings == bindings - 1 &&
          other->world->persistent_bindings == retired_bindings);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || !mc_math_trig_init(&trig))
        return 2;
    fixture f = {0};
    named_world other = {0};
    if (!setup(&f, argv[1], argv[2]) || !named_second_world(&f, argv[3], &other) ||
        !named_cases(&f, &other) || !named_retire_world(&other))
        return 1;
    if (mc_world_runtime_free(other.world, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    mc_world_provider_release(other.dimension);
    if (mc_world_store_free_checked(&other.store, NULL) != MC_WORLD_STORAGE_OK || !teardown(&f))
        return 1;
    printf("PASS: %u named Entity factory/World binding checks\n", checks);
    return 0;
}
