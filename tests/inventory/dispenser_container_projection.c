/* Standalone authored SH component projection, not an enclosing GS/GUI probe.
 * Reuse only the genuine fixture setup/retirement from the CPU controls. Its
 * renamed main and scripted hosts are compiled but never entered here. */
#define main mc_dispenser_container_cpu_controls_not_executed
#include "dispenser_container.c"
#undef main

#include <inttypes.h>

typedef struct projection_case {
    const char *name, *operation;
    bool player, dispenser;
    int32_t slot, button, ordinal;
} projection_case;

static const projection_case projection_cases[] = {
    {"layout-00", "layout", false, false, 0, 0, 0},
    {"layout-01", "layout", false, true, 0, 0, 1},
    {"layout-10", "layout", true, false, 0, 0, 2},
    {"layout-11", "layout", true, true, 0, 0, 3},
    {"get-az", "getter", true, true, 0, 0, 4},
    {"get-az-empty", "getter", true, true, 8, 0, 5},
    {"get-ix-main", "getter", true, true, 9, 0, 6},
    {"get-ix-hotbar", "getter", true, true, 36, 0, 7},
    {"get-negative", "getter", true, true, -1, 0, 8},
    {"get-end", "getter", true, true, 45, 0, 9},
    {"get-null-az", "getter", true, false, 0, 0, 10},
    {"get-null-ix", "getter", false, true, 9, 0, 11},
    {"usable-null-az", "usable", true, false, 0, 0, 12},
    {"usable-null-world", "usable", true, true, 0, 0, 13},
    {"invalid-button-two", "click", true, true, 0, 2, 16},
    {"invalid-button-negative", "click", false, false, -1, -1, 17},
};

typedef struct projection_snapshot {
    bool tile_present, player_present;
    mc_tile_entity_snapshot tile;
    mc_inventory_player_state player;
    uintptr_t adapter_diagnostic_owner_id;
    const char *tile_slots[9], *main_slots[36], *armor_slots[4], *cursor;
    bool stack_present[3];
    mc_item_stack_fields stacks[3];
} projection_snapshot;

static bool projection_need(bool value, const char *operation) {
    if (!value)
        fprintf(stderr, "SH_NATIVE_REFUSAL %s\n", operation);
    return value;
}

static const char *projection_bool(bool value) {
    return value ? "true" : "false";
}

static void projection_text(FILE *out, const char *value) {
    if (value == NULL) {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)value; *p != 0; ++p) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', out);
            fputc(*p, out);
        } else if (*p < 32)
            fprintf(out, "\\u%04x", (unsigned)*p);
        else
            fputc(*p, out);
    }
    fputc('"', out);
}

static const char *projection_alias(const fixture *f, const mc_item_stack *value) {
    if (value == NULL)
        return NULL;
    if (value == f->a)
        return "A";
    if (value == f->b)
        return "B";
    if (value == f->cursor)
        return "H";
    /* A foreign identity is a counterexample, never the expected input alias. */
    return "OTHER";
}

static mc_item_stack *projection_stack(int32_t id, int32_t count, int32_t damage) {
    mc_item_stack *value = NULL;
    if (mc_item_stack_allocate(&value) != MC_ITEM_STACK_OK)
        return NULL;
    if (mc_item_stack_construct(value, id, count, damage) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(value);
        return NULL;
    }
    /* Actual constructor animation0: no extra Source field setter. */
    return value;
}

static bool projection_snapshot_read(fixture *f, projection_snapshot *out) {
    *out = (projection_snapshot){0};
    out->tile_present = f->tile != NULL;
    out->player_present = f->inventory != NULL;
    if (out->tile_present) {
        mc_inventory_dispenser_state adapter;
        if (!projection_need(mc_inventory_dispenser_read_state(f->dispenser, &adapter) &&
                                 adapter.tile == f->tile,
                             "actual passive adapter diagnostic owner snapshot"))
            return false;
        out->adapter_diagnostic_owner_id = adapter.diagnostic_owner_id;
        if (!projection_need(
                mc_tile_entity_get_snapshot(f->tile, &out->tile) == MC_TILE_ENTITY_OK &&
                    out->tile.kind == MC_TILE_ENTITY_DISPENSER &&
                    out->tile.phase == MC_TILE_ENTITY_READY && out->tile.dispenser_capacity == 9,
                "actual AZ snapshot and nine-cell extent"))
            return false;
        for (int32_t index = 0; index < 9; ++index) {
            const mc_item_stack *value = NULL;
            if (!projection_need(mc_inventory_dispenser_borrow(f->dispenser, index, &value, NULL) ==
                                     MC_INVENTORY_OK,
                                 "actual AZ cell borrow"))
                return false;
            out->tile_slots[index] = projection_alias(f, value);
        }
    }
    if (out->player_present) {
        if (!projection_need(mc_inventory_player_read_state(f->inventory, &out->player) &&
                                 out->player.main_slots != NULL && out->player.armor_slots != NULL,
                             "actual IX snapshot"))
            return false;
        for (size_t index = 0; index < 36; ++index)
            out->main_slots[index] = projection_alias(f, out->player.main_slots[index]);
        for (size_t index = 0; index < 4; ++index)
            out->armor_slots[index] = projection_alias(f, out->player.armor_slots[index]);
        out->cursor = projection_alias(f, out->player.cursor);
    }
    const mc_item_stack *values[] = {f->a, f->b, f->cursor};
    for (size_t index = 0; index < 3; ++index) {
        out->stack_present[index] = values[index] != NULL;
        if (values[index] != NULL &&
            !projection_need(mc_item_stack_fields_get(values[index], &out->stacks[index]) ==
                                 MC_ITEM_STACK_OK,
                             "actual held input Stack fields"))
            return false;
    }
    return true;
}

static void projection_array(FILE *out, const char *const *values, size_t count) {
    fputc('[', out);
    for (size_t index = 0; index < count; ++index) {
        if (index != 0)
            fputc(',', out);
        projection_text(out, values[index]);
    }
    fputc(']', out);
}

static void projection_snapshot_json(FILE *out, const projection_snapshot *s) {
    fputs("{\"tile\":", out);
    if (!s->tile_present)
        fputs("null", out);
    else {
        fputs("{\"slots\":", out);
        projection_array(out, s->tile_slots, 9);
        fprintf(out, ",\"world_null\":%s,\"random_constructed\":%s}",
                projection_bool(s->tile.world == NULL),
                projection_bool(s->tile.dispenser_random_constructed));
    }
    fputs(",\"player\":", out);
    if (!s->player_present)
        fputs("null", out);
    else {
        fputs("{\"main\":", out);
        projection_array(out, s->main_slots, 36);
        fputs(",\"armor\":", out);
        projection_array(out, s->armor_slots, 4);
        fputs(",\"cursor\":", out);
        projection_text(out, s->cursor);
        fprintf(out, ",\"selected\":%" PRId32 ",\"changed\":%s,\"player_null\":%s}",
                s->player.selected, projection_bool(s->player.changed),
                projection_bool(s->player.player == NULL));
    }
    fputs(",\"stacks\":{", out);
    const char *names[] = {"A", "B", "H"};
    bool comma = false;
    for (size_t index = 0; index < 3; ++index) {
        if (!s->stack_present[index])
            continue;
        if (comma)
            fputc(',', out);
        comma = true;
        projection_text(out, names[index]);
        const mc_item_stack_fields *v = &s->stacks[index];
        fprintf(out, ":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]", v->id, v->count,
                v->damage, v->animation);
    }
    fputs("}}", out);
}

static void projection_site(FILE *out, const mc_inventory_site *site) {
    fputs("{\"class\":", out);
    projection_text(out, site->class_name);
    fputs(",\"method\":", out);
    projection_text(out, site->method);
    fputs(",\"descriptor\":", out);
    projection_text(out, site->descriptor);
    fprintf(out, ",\"pc\":%" PRId32 "}", site->pc);
}

static void projection_registry(FILE *out, const mc_registry_error *e) {
    fprintf(out, "{\"status\":%d,\"message\":", (int)e->status);
    projection_text(out, e->message);
    fprintf(out, ",\"initializer_pc\":%" PRId32 ",\"block_id\":%" PRId32 ",\"constructor_class\":",
            e->initializer_pc, e->block_id);
    projection_text(out, e->constructor_class);
    fprintf(out,
            ",\"constructor_pc\":%" PRId32 ",\"profile_status\":%d,\"active_initializer_class\":",
            e->constructor_pc, (int)e->profile_status);
    projection_text(out, e->active_initializer_class);
    fprintf(out,
            ",\"active_initializer_pc\":%" PRId32 ",\"item_argument\":%" PRId32
            ",\"format_error\":{\"status\":%d,\"offset\":%zu,\"length\":%zu,\"value\":%" PRId32
            ",\"flags\":%" PRIu32 ",\"conversion\":%u,\"date_time\":%s}}",
            e->active_initializer_pc, e->item_argument, (int)e->format_error.status,
            e->format_error.offset, e->format_error.length, e->format_error.value,
            e->format_error.flags, (unsigned)e->format_error.conversion,
            projection_bool(e->format_error.date_time));
}

static void projection_inventory(FILE *out, const mc_inventory_error *e) {
    fprintf(out, "{\"status\":%d,\"operation\":", (int)e->status);
    projection_text(out, e->operation);
    fprintf(out,
            ",\"source_exception\":%s,\"exception_class\":", projection_bool(e->source_exception));
    projection_text(out, e->exception_class);
    fprintf(out,
            ",\"dispenser_diagnostic_owner_id_is_zero\":%s,\"exception_class_from_dispenser\":%s",
            projection_bool(e->dispenser_diagnostic_owner_id == 0),
            projection_bool(e->exception_class_from_dispenser));
    fputs(",\"frames\":[", out);
    for (size_t index = 0; index < e->frame_count; ++index) {
        if (index != 0)
            fputc(',', out);
        projection_site(out, &e->frames[index]);
    }
    fprintf(out,
            "],\"mutation_reached\":%s,\"notification_reached\":%s,\"stack_status\":%d,\"stack\":{"
            "\"status\":%d,\"field\":",
            projection_bool(e->mutation_reached), projection_bool(e->notification_reached),
            (int)e->stack_status, (int)e->stack.status);
    projection_text(out, e->stack.field);
    fprintf(out, ",\"nbt\":{\"status\":%d,\"offset\":%zu}},\"registry_status\":%d,\"registry\":",
            (int)e->stack.nbt.status, e->stack.nbt.offset, (int)e->registry_status);
    projection_registry(out, &e->registry);
    fprintf(out,
            ",\"matching_status\":%" PRId32
            ",\"chest_present\":%s,\"furnace_present\":%s,\"dispenser_present\":%s}",
            e->matching_status, projection_bool(e->chest != NULL),
            projection_bool(e->furnace != NULL), projection_bool(e->dispenser != NULL));
}

static bool projection_case_run(const projection_case *p, mc_registry_classes *registry,
                                mc_world_runtime_classes *classes) {
    bool ok = false;
    fixture f = {0};
    mc_inventory_error e = {0};
    mc_container_interaction_error ie;
    mc_container_interaction_error_init(&ie);
    projection_snapshot before, after;
    mc_container_state base;
    mc_container_slot_state slots[45];
    mc_container_interaction_state call_state = {0};
    mc_inventory_status status = MC_INVENTORY_OK;
    bool returned = true, usable = true, has_call = false;
    const mc_item_stack *result = NULL;
    if (!projection_need(setup(&f, registry, classes, p->player, p->dispenser),
                         "genuine fresh fixture"))
        goto cleanup;
    if (strcmp(p->operation, "layout") != 0) {
        if (f.dispenser != NULL) {
            f.a = projection_stack(1, 7, 3);
            if (!projection_need(f.a != NULL && mc_inventory_dispenser_set(f.dispenser, 0, f.a,
                                                                           &e) == MC_INVENTORY_OK,
                                 "actual AZ public setup setter"))
                goto cleanup;
        }
        if (f.inventory != NULL) {
            f.b = projection_stack(4, 6, 2);
            f.cursor = projection_stack(5, 2, 1); /* Held H input, not IX cursor. */
            if (!projection_need(
                    f.b != NULL && f.cursor != NULL &&
                        mc_inventory_player_set(f.inventory, 9, f.b, &e) == MC_INVENTORY_OK &&
                        mc_inventory_player_set(f.inventory, 0, f.cursor, &e) == MC_INVENTORY_OK,
                    "actual IX public setup setters"))
                goto cleanup;
        }
    }
    if (!projection_snapshot_read(&f, &before))
        goto cleanup;
    e = (mc_inventory_error){0};
    if (strcmp(p->operation, "getter") == 0) {
        status = mc_container_dispenser_transfer_begin(f.container, p->slot, &f.call, &ie);
        has_call = true;
    } else if (strcmp(p->operation, "usable") == 0) {
        status = mc_container_dispenser_usable(f.container, NULL, &usable, &e);
        returned = status == MC_INVENTORY_OK;
    } else if (strcmp(p->operation, "click") == 0) {
        /* Both declared buttons return before GS.c. No host or fake Player. */
        status = mc_container_click_begin(f.container, NULL, p->slot, p->button, true, NULL,
                                          &f.call, &ie);
        has_call = true;
    }
    if (has_call) {
        if (!projection_need(mc_container_interaction_inspect(f.call, &call_state) &&
                                 !call_state.active,
                             "actual completed invocation inspection"))
            goto cleanup;
        returned = call_state.returned;
        result = call_state.result;
    }
    if (!projection_snapshot_read(&f, &after) ||
        !projection_need(mc_container_read_state(f.container, &base) && base.slots == 45 &&
                             base.snapshots == 45 && base.constructed && base.constructor_entered &&
                             !base.constructor_failed && base.constructor_pc == 173,
                         "actual successful SH constructor state"))
        goto cleanup;
    bool snapshots_null = true;
    for (int32_t index = 0; index < 45; ++index) {
        if (!projection_need(mc_container_slot_read(f.container, index, &slots[index], NULL) ==
                                 MC_INVENTORY_OK,
                             "actual GP slot state read"))
            goto cleanup;
        snapshots_null = snapshots_null && f.container->snapshots->values[index] == NULL;
    }
    if (!projection_need(e.frame_count <= MC_INVENTORY_ERROR_FRAMES &&
                             ie.inventory.frame_count <= MC_INVENTORY_ERROR_FRAMES,
                         "complete bounded actual Source frame extents"))
        goto cleanup;

    fputs("SH_NATIVE_ROW {\"ordinal\":", stdout);
    printf("%" PRId32 ",\"case\":", p->ordinal);
    projection_text(stdout, p->name);
    fputs(",\"layout\":[", stdout);
    for (size_t index = 0; index < 45; ++index) {
        const mc_container_slot_state *s = &slots[index];
        if (index != 0)
            fputc(',', stdout);
        printf("{\"index\":%" PRId32 ",\"owner\":", s->container_index);
        const char *owner = s->inventory == NULL                        ? NULL
                            : s->inventory == (const void *)f.dispenser ? "AZ"
                            : s->inventory == (const void *)f.inventory ? "IX"
                                                                        : "OTHER";
        projection_text(stdout, owner);
        printf(",\"inventory_index\":%" PRId32 ",\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"class\":",
               s->inventory_index, s->x, s->y);
        projection_text(stdout, s->kind == MC_SLOT_NORMAL ? "gp" : "NON_GP");
        fputc('}', stdout);
    }
    const bool distinct =
        base.slot_list != base.snapshot_list && base.slot_list != base.listener_list &&
        base.slot_list != base.blocked_player_set && base.snapshot_list != base.listener_list &&
        base.snapshot_list != base.blocked_player_set &&
        base.listener_list != base.blocked_player_set;
    printf(
        "],\"base\":{\"window\":%" PRId32
        ",\"transaction\":%d,\"slots\":%zu,\"snapshots\":%zu,\"snapshots_null\":%s,\"listeners\":%"
        "zu,\"blocked\":%zu,\"collections_distinct\":%s,\"captured_az_same\":%s},\"before\":",
        base.window_id, (int)base.transaction, base.slots, base.snapshots,
        projection_bool(snapshots_null), base.listeners, base.blocked_players,
        projection_bool(distinct), projection_bool(base.dispenser_inventory == f.dispenser));
    projection_snapshot_json(stdout, &before);
    fputs(",\"after\":", stdout);
    projection_snapshot_json(stdout, &after);
    fprintf(stdout, ",\"returned\":%s,\"result\":", projection_bool(returned));
    if (strcmp(p->operation, "usable") == 0 && returned)
        fputs(projection_bool(usable), stdout);
    else
        projection_text(stdout, projection_alias(&f, result));
    fputs(",\"exception_class\":", stdout);
    const char *exception = has_call ? (ie.source_exception ? ie.exception_class : NULL)
                                     : (e.source_exception ? e.exception_class : NULL);
    if (exception == NULL)
        fputs("null", stdout);
    else {
        /* Explicit Java internal-name to binary-name representation only. */
        fputc('"', stdout);
        for (const char *ch = exception; *ch != 0; ++ch)
            fputc(*ch == '/' ? '.' : *ch, stdout);
        fputc('"', stdout);
    }
    fputs("}\n", stdout);

    fprintf(stderr, "SH_NATIVE_DETAIL {\"ordinal\":%" PRId32 ",\"case\":", p->ordinal);
    projection_text(stderr, p->name);
    fprintf(stderr,
            ",\"status\":%d,\"has_call\":%s,\"usable_sentinel\":%s,\"inventory\":", (int)status,
            projection_bool(has_call), projection_bool(usable));
    projection_inventory(stderr, &e);
    fprintf(stderr, ",\"interaction\":{\"status\":%d,\"operation\":", (int)ie.status);
    projection_text(stderr, ie.operation);
    fputs(",\"exception_class\":", stderr);
    projection_text(stderr, ie.exception_class);
    fprintf(stderr, ",\"exception_class_dispenser_owner_id_is_zero\":%s",
            projection_bool(ie.exception_class_dispenser_owner_id == 0));
    fprintf(stderr, ",\"native_policy\":%s,\"source_exception\":%s,\"source_error\":%s,\"site\":",
            projection_bool(ie.native_policy), projection_bool(ie.source_exception),
            projection_bool(ie.source_error));
    projection_site(stderr, &ie.site);
    fputs(",\"child\":", stderr);
    projection_site(stderr, &ie.child);
    fputs(",\"dependency\":", stderr);
    projection_site(stderr, &ie.dependency);
    fputs(",\"transfer\":", stderr);
    projection_site(stderr, &ie.transfer);
    fputs(",\"inventory\":", stderr);
    projection_inventory(stderr, &ie.inventory);
    fputs(",\"registry\":", stderr);
    projection_registry(stderr, &ie.registry);
    fprintf(
        stderr,
        ",\"stack\":%d},\"call_state\":{\"returned\":%s,\"active\":%s,\"source_pc\":%" PRId32
        ",\"shift_calls\":%zu,\"drop_calls\":%zu,\"steps\":%zu,\"cursor_cleared\":%s},"
        "\"constructor_pc\":%" PRId32 ",\"inventory_calls\":%u,\"cursor_calls\":%u,"
        "\"adapter_diagnostic_owner_present\":%s,\"adapter_diagnostic_owner_stable\":%s}\n",
        (int)ie.stack, projection_bool(call_state.returned), projection_bool(call_state.active),
        call_state.source_pc, call_state.shift_calls, call_state.drop_calls, call_state.steps,
        projection_bool(call_state.cursor_cleared), base.constructor_pc, f.inventory_calls,
        f.cursor_calls, projection_bool(after.adapter_diagnostic_owner_id != 0),
        projection_bool(before.adapter_diagnostic_owner_id != 0 &&
                        before.adapter_diagnostic_owner_id == after.adapter_diagnostic_owner_id));
    if (!projection_need(e.chest == NULL && e.furnace == NULL && e.dispenser == NULL &&
                             ie.inventory.chest == NULL && ie.inventory.furnace == NULL &&
                             ie.inventory.dispenser == NULL,
                         "declared component cohort has no opaque retained World failure child"))
        goto cleanup;
    ok = true;
cleanup:;
    const bool retired = destroy(&f);
    fprintf(stderr, "SH_NATIVE_RETIRE {\"ordinal\":%" PRId32 ",\"case\":", p->ordinal);
    projection_text(stderr, p->name);
    fprintf(stderr, ",\"retired\":%s}\n", projection_bool(retired));
    return ok && retired && !ferror(stdout) && !ferror(stderr);
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *registry = NULL;
    mc_world_runtime_classes *classes = NULL;
    static mc_math_trig trig;
    if (argc != 2 || !mc_math_trig_init(&trig) ||
        mc_resource_files_create(argv[1], &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {MC_REGISTRY_CLIENT, profile, materials, 0,
                                         mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &registry, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_initialize_statistics(registry, NULL) != MC_REGISTRY_OK ||
        mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &classes, NULL) !=
            MC_WORLD_STORAGE_OK ||
        mc_world_runtime_classes_bind_registry(classes, registry, NULL) != MC_REGISTRY_OK)
        goto cleanup;
    for (size_t index = 0; index < sizeof projection_cases / sizeof *projection_cases; ++index)
        if (!projection_case_run(&projection_cases[index], registry, classes))
            goto cleanup;
    result = 0;
cleanup:
    if (classes != NULL && mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    if (registry != NULL && mc_registry_classes_free(registry, NULL) != MC_REGISTRY_OK)
        result = 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    if (result == 0 && !ferror(stdout) && !ferror(stderr)) {
        fputs("SH_NATIVE_DETAIL_COMPLETE 16\n", stderr);
        fputs("SH_NATIVE_COMPLETE 16\n", stdout);
        if (fflush(stdout) != 0 || fflush(stderr) != 0)
            result = 1;
    }
    return result;
}
