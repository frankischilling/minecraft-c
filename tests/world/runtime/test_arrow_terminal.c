/* Authored native ownership controls over actual Arrow/World/Chunk owners.
 * No Arrow tick, renderer, NBT, Skeleton parent or original execution claim. */
int mc_arrow_terminal_unused_publication_main(int, char **);
#define main mc_arrow_terminal_unused_publication_main
#include "test_entity_publication.c"
#undef main

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/arrow/internal.h"
#include "../../../src/entity/arrow/references_internal.h"
#include "../../../src/entity/mob/internal.h"
#include "../../../src/entity/mob/references_internal.h"
#include "../../../src/world/chunk/object_state_internal.h"
#include "../../../src/world/runtime/terminal_inventory_internal.h"

typedef struct at_piece {
    const void *identity;
    size_t bytes;
    unsigned char *before;
} at_piece;

typedef struct at_snapshot {
    at_piece pieces[384];
    size_t count;
    size_t provider_holds[WORLD_COUNT];
    size_t store_chunks[WORLD_COUNT], store_reservations[WORLD_COUNT];
} at_snapshot;

static bool at_piece_save(at_snapshot *s, const void *identity, size_t bytes) {
    if (identity == NULL || bytes == 0)
        return identity == NULL ? bytes == 0 : true;
    for (size_t i = 0; i < s->count; ++i)
        if (s->pieces[i].identity == identity)
            return s->pieces[i].bytes == bytes;
    if (s->count == COUNT(s->pieces) || bytes > 4u * 1024u * 1024u)
        return false;
    unsigned char *copy = malloc(bytes);
    if (copy == NULL)
        return false;
    memcpy(copy, identity, bytes);
    s->pieces[s->count++] = (at_piece){identity, bytes, copy};
    return true;
}

static void at_snapshot_drop(at_snapshot *s) {
    for (size_t i = 0; i < s->count; ++i)
        free(s->pieces[i].before);
    memset(s, 0, sizeof(*s));
}

static bool at_snapshot_take(fixture *f, mc_entity *const *entities, size_t count, at_snapshot *s) {
    bool ok = false;
    memset(s, 0, sizeof(*s));
    CHECK(at_piece_save(s, f->classes[0], sizeof(*f->classes[0])) &&
          at_piece_save(s, f->entity_classes[0], sizeof(*f->entity_classes[0])) &&
          at_piece_save(s, f->registry[0], sizeof(*f->registry[0])) &&
          at_piece_save(s, f->profile, sizeof(*f->profile)) &&
          (f->profile->math == NULL ||
           at_piece_save(s, f->profile->math, sizeof(*f->profile->math))));
    for (size_t world = 0; world < WORLD_COUNT; ++world) {
        mc_world_runtime *w = f->worlds[world];
        if (w == NULL)
            continue;
        CHECK(at_piece_save(s, w, sizeof(*w)) &&
              at_piece_save(s, w->active_chunks, sizeof(*w->active_chunks)) &&
              at_piece_save(s, w->active_chunks->entries,
                            w->active_chunks->capacity * sizeof(*w->active_chunks->entries)));
        const mc_world_reference_list *lists[] = {
            w->fields.C, w->fields.b, w->fields.D, w->fields.c, w->fields.G,
            w->fields.d, w->fields.e, w->fields.u, w->fields.K, w->fields.R};
        for (size_t i = 0; i < COUNT(lists); ++i)
            CHECK(lists[i] != NULL && lists[i]->capacity <= 1024 &&
                  at_piece_save(s, lists[i], sizeof(*lists[i])) &&
                  at_piece_save(s, lists[i]->items, lists[i]->capacity * sizeof(void *)));
        const mc_world_entity_list_ledger *ledgers[] = {
            &w->entity_membership, &w->entity_deferral, &w->player_membership,
            &w->weather_membership, &w->nearby_entities};
        for (size_t i = 0; i < COUNT(ledgers); ++i)
            CHECK(ledgers[i]->capacity <= 1024 &&
                  at_piece_save(s, ledgers[i]->entries,
                                ledgers[i]->capacity * sizeof(*ledgers[i]->entries)));
        CHECK(at_piece_save(s, f->providers[world].cache, sizeof(*f->providers[world].cache)) &&
              at_piece_save(s, f->providers[world].cache->entries,
                            f->providers[world].cache->capacity *
                                sizeof(*f->providers[world].cache->entries)) &&
              at_piece_save(s, f->providers[world].cache->insertions,
                            f->providers[world].cache->insertion_capacity *
                                sizeof(*f->providers[world].cache->insertions)) &&
              at_piece_save(s, f->providers[world].pending, sizeof(*f->providers[world].pending)) &&
              at_piece_save(s, f->providers[world].pending->keys,
                            f->providers[world].pending->capacity * sizeof(int32_t)));
        s->provider_holds[world] = mc_world_provider_hold_count(w->fields.t);
        s->store_chunks[world] = mc_world_store_chunk_count(f->stores[world]);
        s->store_reservations[world] = mc_world_store_reservation_count(f->stores[world]);
        mc_world_chunk *chunk = f->chunks[world];
        CHECK(chunk != NULL && at_piece_save(s, chunk, sizeof(*chunk)) &&
              at_piece_save(s, f->states[world], sizeof(*f->states[world])) &&
              at_piece_save(s, chunk->data, sizeof(*chunk->data)) &&
              at_piece_save(s, chunk->data->blocks, chunk->data->blocks_capacity));
        for (size_t slice = 0; slice < CHUNK_ENTITY_SLICES; ++slice)
            CHECK(at_piece_save(s, chunk->entities[slice].items,
                                chunk->entities[slice].capacity * sizeof(mc_entity *)));
    }
    for (size_t i = 0; i < count; ++i) {
        const mc_entity *p = entities[i];
        if (p == NULL)
            continue;
        CHECK(at_piece_save(s, p, sizeof(*p)) &&
              at_piece_save(s, p->bounds, p->bounds == NULL ? 0 : sizeof(*p->bounds)) &&
              at_piece_save(s, p->random, p->random == NULL ? 0 : sizeof(*p->random)) &&
              (p->arrow == NULL || at_piece_save(s, p->arrow, sizeof(*p->arrow))) &&
              (p->mob == NULL || at_piece_save(s, p->mob, sizeof(*p->mob))));
        if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ARROW)) {
            const mc_arrow_entity_object *actual = (const mc_arrow_entity_object *)p;
            CHECK(at_piece_save(s, &actual->state, sizeof(actual->state)));
        }
        if (p->watcher != NULL) {
            CHECK(at_piece_save(s, p->watcher, sizeof(*p->watcher)) && p->watcher->map != NULL &&
                  at_piece_save(s, p->watcher->map, sizeof(*p->watcher->map)));
            size_t records = 0;
            for (const mc_entity_watch_node *node = p->watcher->map->head; node != NULL;
                 node = node->next) {
                CHECK(records++ < p->watcher->map->size && at_piece_save(s, node, sizeof(*node)) &&
                      node->record != NULL &&
                      at_piece_save(s, node->record, sizeof(*node->record)));
            }
            CHECK(records == p->watcher->map->size);
        }
    }
    ok = true;
cleanup:
    if (!ok)
        at_snapshot_drop(s);
    return ok;
}

static bool at_snapshot_same(fixture *f, const at_snapshot *s) {
    for (size_t i = 0; i < s->count; ++i)
        if (memcmp(s->pieces[i].identity, s->pieces[i].before, s->pieces[i].bytes) != 0)
            return false;
    for (size_t world = 0; world < WORLD_COUNT; ++world)
        if (f->worlds[world] != NULL &&
            (mc_world_provider_hold_count(f->worlds[world]->fields.t) != s->provider_holds[world] ||
             mc_world_provider_world(f->worlds[world]->fields.t) != f->worlds[world] ||
             mc_world_store_chunk_count(f->stores[world]) != s->store_chunks[world] ||
             mc_world_store_reservation_count(f->stores[world]) != s->store_reservations[world]))
            return false;
    return true;
}

static bool at_attempt(fixture *f, mc_entity *const *observed, size_t count,
                       const mc_world_terminal_entity_anchor *anchors, size_t anchor_count,
                       bool containers, mc_world_storage_status expected, const char *operation,
                       mc_world_terminal_inventory *success) {
    bool ok = false;
    at_snapshot snapshot = {0};
    mc_world_terminal_inventory out;
    memset(&out, 0xa5, sizeof(out));
    unsigned char sentinel[sizeof(out)];
    memcpy(sentinel, &out, sizeof(out));
    mc_world_storage_error error = {0};
    const mc_world_terminal_anchors roots = {anchors, anchor_count, NULL, 0};
    CHECK(!containers || expected != MC_WORLD_STORAGE_OK);
    CHECK(at_snapshot_take(f, observed, count, &snapshot));
    const mc_world_storage_status status =
        containers
            ? mc_world_runtime_retire_entity_containers(f->worlds[0], &error)
            : mc_world_runtime_terminal_admit_owned(f->worlds[0], &roots, NULL, &out, &error);
    CHECK(status == expected && error.status == expected &&
          (operation == NULL || (error.operation != NULL && !strcmp(error.operation, operation))) &&
          at_snapshot_same(f, &snapshot));
    if (expected != MC_WORLD_STORAGE_OK)
        CHECK(memcmp(sentinel, &out, sizeof(out)) == 0);
    else if (success != NULL)
        *success = out;
    ok = true;
cleanup:
    at_snapshot_drop(&snapshot);
    return ok;
}

static bool at_open(fixture *f, const char *root) {
    bool ok = false;
    const uint16_t title[] = {'A', 'r', 'r', 'o', 'w'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK &&
          mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          open_store(f, 0, root) && create_world(f, 0));
    /* End only the caller's provider occurrence; the actual World owns t. */
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    CHECK(mc_world_provider_hold_count(f->worlds[0]->fields.t) == 1);
    ok = true;
cleanup:
    return ok;
}

static bool at_close(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t world = 0; world < WORLD_COUNT; ++world) {
        mc_world_runtime *w = f->worlds[world];
        if (w == NULL)
            continue;
        if (!f->terminal[world] &&
            mc_world_runtime_retire_entity_containers(w, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->terminal[world] = true;
        if (!retire_descriptors(f, world) ||
            mc_world_runtime_retire_world(w, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        if (f->states[world] != NULL &&
            mc_world_store_retire_chunk(f->stores[world], f->states[world], NULL) !=
                MC_WORLD_STORAGE_OK)
            return false;
        if (mc_world_runtime_free(w, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->worlds[world] = NULL;
        mc_world_provider_release(f->dimensions[world]);
        if (mc_world_store_free_checked(&f->stores[world], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    if ((f->classes[0] != NULL &&
         mc_world_runtime_classes_free(f->classes[0], NULL) != MC_WORLD_STORAGE_OK) ||
        (f->registry[0] != NULL &&
         mc_registry_classes_free(f->registry[0], NULL) != MC_REGISTRY_OK) ||
        (f->entity_classes[0] != NULL &&
         mc_entity_classes_free(f->entity_classes[0]) != MC_ENTITY_OK))
        return false;
    mc_item_stack_release(f->stack);
    mc_world_info_name_release(f->name);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool at_pig(fixture *f, mc_world_runtime *world, mc_entity **out) {
    bool ok = false;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->entity_classes[0], MC_ENTITY_CLASS_KIND_PIG, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->entity_classes[0], f->registry[0], literal, out, &error) ==
              MC_MOB_OK &&
          mc_mob_construct(*out, world, &error) == MC_MOB_OK &&
          mc_entity_set_position(*out, 8.5, 8.5, 8.5) == MC_ENTITY_OK);
    ok = true;
cleanup:
    return ok;
}

typedef struct at_publication {
    fixture *f;
    mc_entity *arrow;
    size_t calls;
    bool actual_prefix;
} at_publication;

static mc_world_storage_status at_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *error) {
    at_publication *p = context;
    ++p->calls;
    p->actual_prefix = world == p->f->worlds[0] && entity == p->arrow &&
                       world->fields.b->count == 1 && world->fields.b->items[0] == entity &&
                       p->f->chunks[0]->entities[0].count == 1 &&
                       p->f->chunks[0]->entities[0].items[0] == entity;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Authored Arrow publication observer", 77};
    return error->status;
}

static bool at_arrow_fields_same(const mc_arrow_fields *a, const mc_arrow_fields *b) {
    return a->a == b->a && a->b == b->b && a->c == b->c && a->d == b->d && a->e == b->e &&
           a->f == b->f && a->g == b->g && a->h == b->h && a->i == b->i && a->j == b->j &&
           a->k == b->k;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_entity *a_owner = NULL, *p_owner = NULL, *b_owner = NULL, *failed_owner = NULL;
    mc_entity *arrow = NULL, *pig = NULL;
    mc_world_access *receiver = NULL;
    at_snapshot refused_store = {0};
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "arrow-terminal") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && at_open(f, root));
    mc_world_runtime *w = f->worlds[0];
    mc_entity_error child;
    mc_arrow_error construction;
    CHECK(mc_arrow_allocate(f->entity_classes[0], &a_owner, &child) == MC_ENTITY_OK &&
          mc_arrow_construct_position(a_owner, NULL, 8.5, 8.5, 8.5, &construction) ==
              MC_ENTITY_OK &&
          a_owner->phase == MC_ENTITY_READY && mc_entity_has_arrow(a_owner) &&
          a_owner->base.world == NULL && a_owner->mob == NULL && a_owner->holds == 1 &&
          a_owner->arrow->fields.c == NULL && a_owner->arrow->shooter_hold == NULL);
    arrow = a_owner;
    const mc_java_random random = *arrow->random;
    const mc_arrow_fields source_fields = arrow->arrow->fields;
    at_publication observed = {f, arrow, 0, false};
    const mc_world_access_callbacks callbacks = {.entity_added = at_added};
    CHECK(mc_world_access_create(&callbacks, &observed, &receiver, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(w, receiver, NULL) == MC_WORLD_STORAGE_OK);
    bool inserted = false;
    mc_world_entity_error publication;
    CHECK(mc_world_runtime_insert_entity(w, arrow, &inserted, &publication) ==
              MC_WORLD_STORAGE_IO_ERROR &&
          !inserted && observed.calls == 1 && observed.actual_prefix &&
          publication.cause.system_error == 77 && arrow->holds == 3 && w->fields.b->count == 1 &&
          f->chunks[0]->entities[0].count == 1);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(w, receiver, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(receiver, NULL) == MC_WORLD_STORAGE_OK);
    receiver = NULL;
    CHECK(mc_world_runtime_insert_entity(w, arrow, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted && arrow->holds == 5 && w->fields.b->count == 2 &&
          f->chunks[0]->entities[0].count == 2 &&
          at_arrow_fields_same(&source_fields, &arrow->arrow->fields) &&
          arrow->random->state == random.state &&
          arrow->random->have_next_next_gaussian == random.have_next_next_gaussian &&
          arrow->random->next_next_gaussian == random.next_next_gaussian);
    mc_entity *nodes[4] = {arrow, NULL, NULL, NULL};
    /* Explicit native entered-operation controls with a null shooter prove
     * that the old Mob-only early-out cannot bypass Arrow validation. */
    arrow->arrow->heading_active = true;
    CHECK(at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_BUSY,
                     "World terminal Arrow heading still entered", NULL));
    arrow->arrow->heading_active = false;
    arrow->arrow->constructor_active = true;
    CHECK(at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_BUSY,
                     "World terminal Arrow constructor still entered", NULL));
    arrow->arrow->constructor_active = false;
    arrow->phase = MC_ENTITY_CONSTRUCTING;
    CHECK(at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_BUSY,
                     "World terminal Entity constructor still entered", NULL));
    arrow->phase = MC_ENTITY_READY;
    mc_arrow_entity_state *storage = arrow->arrow;
    arrow->arrow = NULL;
    CHECK(at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                     "World terminal canonical Arrow child storage", NULL));
    arrow->arrow = storage;
    CHECK(at_pig(f, w, &p_owner) && at_pig(f, w, &b_owner));
    pig = p_owner;
    nodes[1] = pig;
    nodes[2] = b_owner;
    CHECK(mc_arrow_shooter_store(arrow, pig, &child) == MC_ENTITY_OK && pig->holds == 2 &&
          mc_arrow_shooter_store(arrow, pig, &child) == MC_ENTITY_OK && pig->holds == 2 &&
          mc_arrow_shooter_store(arrow, b_owner, &child) == MC_ENTITY_OK && pig->holds == 1 &&
          b_owner->holds == 2 && mc_arrow_shooter_store(arrow, NULL, &child) == MC_ENTITY_OK &&
          b_owner->holds == 1 && mc_arrow_shooter_store(arrow, pig, &child) == MC_ENTITY_OK);
    const size_t replacement_holds = b_owner->holds;
    b_owner->holds = SIZE_MAX;
    CHECK(at_snapshot_take(f, nodes, COUNT(nodes), &refused_store));
    const mc_entity_status hold_refused = mc_arrow_shooter_store(arrow, b_owner, &child);
    CHECK(hold_refused == MC_ENTITY_LIMIT && child.status == hold_refused &&
          at_snapshot_same(f, &refused_store) && arrow->arrow->fields.c == pig &&
          arrow->arrow->shooter_hold == pig && pig->holds == 2);
    at_snapshot_drop(&refused_store);
    b_owner->holds = replacement_holds;
    mc_entity_release(b_owner);
    b_owner = nodes[2] = NULL;
    const mc_world_terminal_entity_anchor owners[] = {{&a_owner}, {&p_owner}};
    mc_world_terminal_inventory inventory;
    CHECK(at_attempt(f, nodes, COUNT(nodes), owners, COUNT(owners), false, MC_WORLD_STORAGE_OK,
                     NULL, &inventory) &&
          inventory.entities == 2 && inventory.entity_occurrences == 7 &&
          inventory.expected_world_bindings == 2);
    const mc_world_terminal_entity_anchor repeated[] = {{&a_owner}, {&a_owner}, {&p_owner}};
    CHECK(at_attempt(f, nodes, COUNT(nodes), repeated, COUNT(repeated), false,
                     MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World terminal repeated owning slot",
                     NULL));
    arrow->arrow->fields.c = NULL; /* Declared source/hold mismatch, no release. */
    mc_entity *untouched = arrow;
    CHECK(mc_arrow_shooter_get(arrow, &untouched, &child) == MC_ENTITY_STATE_ERROR &&
          untouched == arrow &&
          at_attempt(f, nodes, COUNT(nodes), owners, COUNT(owners), false,
                     MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Arrow shooter occurrence provenance",
                     NULL) &&
          at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                     "Arrow shooter occurrence provenance", NULL));
    arrow->arrow->fields.c = pig;
    arrow->arrow->shooter_hold = NULL; /* Opposite mismatch also reaches scan. */
    CHECK(at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                     "Arrow shooter occurrence provenance", NULL));
    arrow->arrow->shooter_hold = pig;
    const size_t holds_before_failure = pig->holds,
                 bindings_before_failure = w->persistent_bindings;
    CHECK(mc_arrow_allocate(f->entity_classes[0], &failed_owner, NULL) == MC_ENTITY_OK &&
          mc_arrow_construct_living(failed_owner, w, pig, NULL, NULL) != MC_ENTITY_OK &&
          failed_owner->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
          failed_owner->arrow->fields.c == pig && failed_owner->arrow->shooter_hold == pig &&
          pig->holds == holds_before_failure + 1 &&
          w->persistent_bindings == bindings_before_failure + 1);
    nodes[2] = failed_owner;
    const mc_world_terminal_entity_anchor failed_roots[] = {
        {&a_owner}, {&p_owner}, {&failed_owner}};
    CHECK(at_attempt(f, nodes, COUNT(nodes), failed_roots, COUNT(failed_roots), false,
                     MC_WORLD_STORAGE_OK, NULL, &inventory) &&
          inventory.entities == 3 && inventory.entity_occurrences == 9 &&
          inventory.expected_world_bindings == 3);
    mc_entity_release(failed_owner); /* Actual failed-object destructor drops c. */
    failed_owner = nodes[2] = NULL;
    CHECK(pig->holds == holds_before_failure && w->persistent_bindings == bindings_before_failure);
    CHECK(open_store(f, 2, root) && create_world(f, 2) && at_pig(f, f->worlds[2], &b_owner));
    mc_world_provider_release(f->dimensions[2]);
    f->dimensions[2] = NULL;
    nodes[2] = b_owner;
    CHECK(mc_arrow_shooter_store(arrow, b_owner, &child) == MC_ENTITY_OK &&
          arrow->arrow->fields.c == b_owner && b_owner->base.world == f->worlds[2] &&
          at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                     "World terminal Entity bound outside admitted World", NULL) &&
          at_attempt(f, nodes, COUNT(nodes), owners, COUNT(owners), false,
                     MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                     "World terminal Entity bound outside admitted World", NULL));
    CHECK(mc_arrow_shooter_store(arrow, pig, NULL) == MC_ENTITY_OK);
    mc_entity_release(b_owner);
    b_owner = nodes[2] = NULL;
    CHECK(at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_BUSY,
                     "World terminal external owner of Arrow shooter field", NULL));
    mc_entity_release(a_owner);
    a_owner = NULL; /* Remaining Arrow roots are the actual duplicate publications. */
    const mc_world_terminal_entity_anchor pig_root[] = {{&p_owner}};
    CHECK(at_attempt(f, nodes, COUNT(nodes), pig_root, COUNT(pig_root), false, MC_WORLD_STORAGE_OK,
                     NULL, &inventory) &&
          inventory.entities == 2 && inventory.entity_occurrences == 6 &&
          inventory.expected_world_bindings == 2);
    const size_t held = pig->holds;
    pig->holds = SIZE_MAX; /* Native trusted-commit pin admission only. */
    const bool pin_refused =
        at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_LIMIT,
                   "World terminal Mob graph pin ceiling", NULL);
    pig->holds = held;
    CHECK(pin_refused);
    CHECK(mc_mob_creature_target_store(pig, arrow, NULL) == MC_ENTITY_OK &&
          pig->mob->creature_target == arrow && pig->mob->creature_target_hold == arrow &&
          at_attempt(f, nodes, COUNT(nodes), pig_root, COUNT(pig_root), false, MC_WORLD_STORAGE_OK,
                     NULL, &inventory) &&
          inventory.entities == 2 && inventory.entity_occurrences == 7 &&
          at_attempt(f, nodes, COUNT(nodes), NULL, 0, true, MC_WORLD_STORAGE_BUSY,
                     "World terminal external owner of Mob reference fields", NULL));
    mc_entity_release(p_owner);
    p_owner = NULL; /* Closed Arrow -> Pig -> Arrow cycle has no external owner. */
    CHECK(
        at_attempt(f, nodes, COUNT(nodes), NULL, 0, false, MC_WORLD_STORAGE_OK, NULL, &inventory) &&
        inventory.entities == 2 && inventory.entity_occurrences == 6 &&
        inventory.expected_world_bindings == 2);
    CHECK(mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_OK);
    f->terminal[0] = true;
    arrow = pig = NULL; /* The joint commit ended both borrowed identities. */
    memset(nodes, 0, sizeof(nodes));
    CHECK(w->entity_containers_retired && w->construction.phase == MC_WORLD_RUNTIME_RETIRED &&
          w->fields.b->count == 0 && f->chunks[0]->entities[0].count == 0 &&
          f->entity_classes[0]->live_entities == 0 && w->persistent_bindings == 1);
    result = 0;
cleanup:
    at_snapshot_drop(&refused_store);
    if (receiver != NULL && f != NULL && f->worlds[0] != NULL) {
        bool was_removed = false;
        (void)mc_world_runtime_remove_world_access(f->worlds[0], receiver, &was_removed, NULL);
        (void)mc_world_access_free(receiver, NULL);
    }
    mc_entity_release(failed_owner);
    mc_entity_release(b_owner);
    mc_entity_release(a_owner);
    mc_entity_release(p_owner);
    if (!at_close(f))
        result = 1;
    if (result == 0)
        printf("PASS: %u Arrow World ownership checks (arrow-terminal)\n", checks);
    return result;
}
