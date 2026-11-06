#ifndef MC_TEST_NAMED_DATA_PROBE_H
#define MC_TEST_NAMED_DATA_PROBE_H

/* Shares the finite constructor, origin, numerical and complete disk observer
 * with natural_probe.c. Every observed owner comes from actual native calls;
 * this producer reads no expected observation or fixture. */
static const char *named_map_class(const natural_probe *probe) {
    return probe->client ? "iu" : "fv";
}

static bool named_ref(natural_probe *probe, const void *owner, const char *type) {
    if (owner == NULL)
        return emit(&probe->json, "null");
    natural_named *named = probe->named;
    size_t i = 0;
    while (i < named->reference_count && named->references[i] != owner)
        ++i;
    if (i == named->reference_count) {
        if (i >= sizeof(named->references) / sizeof(named->references[0]))
            return problem(probe, "Named-data reference capacity", "probe", 1);
        named->references[i] = owner;
        named->reference_classes[i] = type;
        ++named->reference_count;
    } else if (strcmp(named->reference_classes[i], type) != 0) {
        return problem(probe, "Named-data reference class identity", "probe", 1);
    }
    return emit(&probe->json, "{\"ref\":\"n%zu\",\"class\":", i) && string(&probe->json, type) &&
           emit(&probe->json, "}");
}

static bool named_string(natural_probe *probe, mc_world_info_name *name) {
    if (!emit(&probe->json, "{\"object\":") || !named_ref(probe, name, "java.lang.String") ||
        !emit(&probe->json, ",\"utf16\":"))
        return false;
    if (name == NULL)
        return emit(&probe->json, "null}");
    mc_nbt_string text = mc_world_info_name_text(name);
    if (text.length > 256 || !emit(&probe->json, "["))
        return false;
    for (size_t i = 0; i < text.length; ++i)
        if (!emit(&probe->json, "%s%u", i == 0 ? "" : ",", (unsigned int)text.units[i]))
            return false;
    return emit(&probe->json, "]}");
}

static bool named_empty(natural_probe *probe, const void *owner, bool map, size_t count) {
    if (owner == NULL || count != 0)
        return problem(probe, "Named-data actual empty collection", "probe", 1);
    return emit(&probe->json, "{\"object\":") &&
           named_ref(probe, owner, map ? "java.util.HashMap" : "java.util.ArrayList") &&
           emit(&probe->json, ",\"size\":0,\"entries\":[]}");
}

static bool named_manager(natural_probe *probe, mc_world_saved_data *owner) {
    mc_world_saved_data_view view;
    if (mc_world_saved_data_inspect(owner, &view) != MC_WORLD_STORAGE_OK || view.storage == NULL ||
        !view.class_bound ||
        view.side != (probe->client ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER) ||
        view.named_count > 256 || view.ordered_count > 256 || view.id_count != 0)
        return problem(probe, "Named-data actual manager projection", "probe", 1);
    natural_json *json = &probe->json;
    if (!emit(json, "{\"object\":") || !named_ref(probe, owner, probe->client ? "hc" : "es") ||
        !emit(json, ",\"handler_store\":") ||
        !named_ref(probe, probe->store, probe->client ? "mx" : "ie") ||
        !emit(json, ",\"native_storage_binding\":") ||
        !named_ref(probe, view.storage, "native.path_binding") ||
        !emit(json, ",\"named_cache\":{\"object\":") ||
        !named_ref(probe, view.named_cache, "java.util.HashMap") ||
        !emit(json, ",\"size\":%zu,\"entries\":[", view.named_count))
        return false;
    for (size_t i = 0; i < view.named_count; ++i) {
        mc_world_info_name *key;
        mc_saved_data_value *value;
        if (!mc_world_saved_data_named_at(owner, i, &key, &value) ||
            !emit(json, "%s{\"ordinal\":%zu,\"key\":", i == 0 ? "" : ",", i) ||
            !named_ref(probe, key, "java.lang.String") || !emit(json, ",\"key_string\":") ||
            !named_string(probe, key) || !emit(json, ",\"value\":") ||
            !named_ref(probe, value, named_map_class(probe)) || !emit(json, "}"))
            return false;
    }
    if (!emit(json, "]},\"ordered_list\":{\"object\":") ||
        !named_ref(probe, view.ordered_list, "java.util.ArrayList") ||
        !emit(json, ",\"size\":%zu,\"entries\":[", view.ordered_count))
        return false;
    for (size_t i = 0; i < view.ordered_count; ++i)
        if (!emit(json, "%s{\"index\":%zu,\"value\":", i == 0 ? "" : ",", i) ||
            !named_ref(probe, mc_world_saved_data_ordered_at(owner, i), named_map_class(probe)) ||
            !emit(json, "}"))
            return false;
    return emit(json, "]},\"id_map\":") && named_empty(probe, view.id_map, true, view.id_count) &&
           emit(json, "}");
}

static bool named_value(natural_probe *probe, mc_saved_data_value *value) {
    mc_saved_data_map_fields fields;
    if (mc_saved_data_map_fields_get(value, &fields) != MC_WORLD_STORAGE_OK ||
        fields.type !=
            mc_saved_data_map_class(probe->client ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER) ||
        fields.colors == NULL || mc_nbt_byte_array_length(fields.colors) > NATURAL_DISK_FILE_BYTES)
        return problem(probe, "Named-data actual MapData projection", "probe", 1);
    natural_json *json = &probe->json;
    if (!emit(json, "{\"object\":") || !named_ref(probe, value, named_map_class(probe)) ||
        !emit(json, ",\"name\":") || !named_string(probe, fields.name) ||
        !emit(json,
              ",\"dirty\":%s,\"x_center\":%" PRId32 ",\"z_center\":%" PRId32
              ",\"dimension\":%d,\"scale\":%d,\"update_counter\":%" PRId32
              ",\"colors\":{\"object\":",
              boolean(fields.dirty), fields.x_center, fields.z_center, (int)fields.dimension,
              (int)fields.scale, fields.updates) ||
        !named_ref(probe, fields.colors, "[B") || !emit(json, ",\"data\":") ||
        !byte_array(json, mc_nbt_byte_array_data(fields.colors),
                    mc_nbt_byte_array_length(fields.colors)) ||
        !emit(json, "},\"trackers\":") ||
        !named_empty(probe, fields.trackers, false, fields.tracker_count) ||
        !emit(json, ",\"decorations\":") ||
        !named_empty(probe, fields.decorations, false, fields.decoration_count) ||
        !emit(json, ",\"tracker_cache\":") ||
        !named_empty(probe, fields.player_trackers, true, fields.player_tracker_count))
        return false;
    return emit(json, "}");
}

static bool named_boundary(natural_probe *probe) {
    mc_world_runtime_fields fields;
    if (mc_world_runtime_fields_get(probe->world, &fields) != MC_WORLD_STORAGE_OK ||
        fields.w != probe->store || fields.x != probe->persistence->info || fields.d == NULL ||
        fields.d->count != 0 || fields.z == NULL)
        return problem(probe, "Named-data held World graph", "probe", 1);
    natural_json *json = &probe->json;
    if (!emit(json, "{\"state\":") || !persistence_boundary(probe) ||
        !emit(json, ",\"collections\":{\"world\":") ||
        !named_ref(probe, probe->world, probe->client ? "fd" : "dj") ||
        !emit(json, ",\"players\":") || !named_empty(probe, fields.d, false, fields.d->count) ||
        !emit(json, ",\"saved_data\":") || !named_manager(probe, fields.z) ||
        !emit(json, ",\"reader_manager\":") ||
        !(probe->named->reader == NULL ? emit(json, "null")
                                       : named_manager(probe, probe->named->reader)) ||
        !emit(json, "},\"map_data\":["))
        return false;
    for (size_t i = 0; i < probe->named->value_count; ++i)
        if (!emit(json, "%s", i == 0 ? "" : ",") || !named_value(probe, probe->named->values[i]))
            return false;
    return emit(json, "]}");
}

static bool named_status(natural_probe *probe, mc_world_storage_status status,
                         const char *operation) {
    return status == MC_WORLD_STORAGE_OK ||
           problem(probe, operation, "storage", (unsigned int)status);
}

static bool named_after(natural_probe *probe, mc_world_storage_status status,
                        const natural_epoch *epoch, bool *ready) {
    bool after = false;
    if (!emit(&probe->json, ",\"status\":") ||
        !string(&probe->json, mc_world_storage_status_string(status)) ||
        !emit(&probe->json, ",\"native_status\":%u,\"epoch_ms\":", (unsigned int)status) ||
        !epoch_projection(&probe->json, epoch) || !emit(&probe->json, ",\"after\":") ||
        !boundary_value(probe, &after) || !emit(&probe->json, "}"))
        return false;
    *ready = *ready && after && status == MC_WORLD_STORAGE_OK && probe->failed_stage == NULL;
    return true;
}

static bool named_operation(natural_probe *probe, unsigned int operation, bool *ready) {
    natural_named *named = probe->named;
    natural_json *json = &probe->json;
    bool before = false;
    natural_epoch epoch = {0};
    if (!emit(json, "{\"before\":") || !boundary_value(probe, &before))
        return false;
    *ready = *ready && before;
    mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    mc_saved_data_error detail = {0};
    if (*ready && epoch_sample(probe, &epoch.low, &epoch.low_valid)) {
        switch (operation) {
            case 0: {
                static const uint16_t units[] = {'n', 'a', 'm', 'e', 'd', '_', 'm', 'a',
                                                 'p', '_', 'v', 'a', 'l', 'u', 'e'};
                mc_world_info_status copied = mc_world_info_name_copy(
                    (mc_nbt_string){units, sizeof(units) / sizeof(units[0])}, 4096, &named->name);
                status = copied == MC_WORLD_INFO_OK
                             ? mc_saved_data_map_create(
                                   mc_saved_data_map_class(probe->client ? MC_WORLD_INFO_CLIENT
                                                                         : MC_WORLD_INFO_SERVER),
                                   named->name, 1024u * 1024u, &named->values[0], &detail)
                             : MC_WORLD_STORAGE_ALLOCATION_FAILED;
                if (status == MC_WORLD_STORAGE_OK)
                    named->value_count = 1;
                break;
            }
            case 1:
                status = mc_saved_data_value_mark_dirty(named->values[0]);
                break;
            case 2: {
                static const uint16_t units[] = {'n', 'a', 'm', 'e', 'd', '_', 'm', 'a',
                                                 'p', '_', 'l', 'o', 'o', 'k', 'u', 'p'};
                mc_world_info_status copied = mc_world_info_name_copy(
                    (mc_nbt_string){units, sizeof(units) / sizeof(units[0])}, 4096, &named->lookup);
                status =
                    copied == MC_WORLD_INFO_OK
                        ? mc_world_runtime_saved_data_put(probe->world, named->lookup,
                                                          named->values[0], &probe->storage_error)
                        : MC_WORLD_STORAGE_ALLOCATION_FAILED;
                break;
            }
            case 3:
                status = mc_world_saved_data_create_for_side(
                    probe->store, probe->client ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, NULL,
                    &named->reader, &probe->storage_error);
                break;
            case 4:
            case 5: {
                mc_saved_data_value *value = NULL;
                status = mc_world_saved_data_get(
                    named->reader,
                    mc_saved_data_map_class(probe->client ? MC_WORLD_INFO_CLIENT
                                                          : MC_WORLD_INFO_SERVER),
                    named->name, &value, &probe->storage_error);
                if (status == MC_WORLD_STORAGE_OK && value == NULL)
                    status = MC_WORLD_STORAGE_INVALID_DATA;
                if (status == MC_WORLD_STORAGE_OK && operation == 4) {
                    named->values[1] = value;
                    named->value_count = 2;
                } else {
                    if (status == MC_WORLD_STORAGE_OK && value != named->values[1])
                        status = MC_WORLD_STORAGE_INVALID_DATA;
                    mc_saved_data_value_release(value);
                }
                break;
            }
            default:
                break;
        }
        (void)named_status(probe, status, "Actual named-data operation");
        (void)epoch_sample(probe, &epoch.high, &epoch.high_valid);
    }
    if (operation == 0) {
        if (!emit(json, ",\"name\":") || !named_string(probe, named->name) ||
            !emit(json, ",\"result\":") ||
            !named_ref(probe, named->values[0], named_map_class(probe)))
            return false;
    } else if (operation == 2) {
        if (!emit(json, ",\"key\":") || !named_string(probe, named->lookup) ||
            !emit(json, ",\"value\":") ||
            !named_ref(probe, named->values[0], named_map_class(probe)))
            return false;
    } else if (operation == 3) {
        if (!emit(json, ",\"storage\":") ||
            !named_ref(probe, probe->store, probe->client ? "mx" : "ie") ||
            !emit(json, ",\"result\":") ||
            !named_ref(probe, named->reader, probe->client ? "hc" : "es"))
            return false;
    } else if (operation == 4 || operation == 5) {
        if (!emit(json, ",\"name\":") || !named_string(probe, named->name) ||
            !emit(json, ",\"requested_class\":") || !string(json, named_map_class(probe)) ||
            !emit(json, ",\"requested_class_object\":") ||
            !named_ref(probe,
                       mc_saved_data_map_class(probe->client ? MC_WORLD_INFO_CLIENT
                                                             : MC_WORLD_INFO_SERVER),
                       "java.lang.Class") ||
            !emit(json, ",\"result\":") ||
            !named_ref(probe, named->values[1], named_map_class(probe)))
            return false;
    }
    if (operation == 1 || operation == 2)
        if (!emit(json, ",\"returned_void\":%s", boolean(status == MC_WORLD_STORAGE_OK)))
            return false;
    return named_after(probe, status, &epoch, ready);
}

static bool observe_named(natural_probe *probe, bool initialized) {
    natural_json *json = &probe->json;
    natural_persistence *p = probe->persistence;
    natural_epoch construction = {p->construction_low, p->construction_high,
                                  p->construction_low_valid, p->construction_high_valid};
    if (!emit(json,
              "{\"schema\":1,\"scope\":\"native-game-population-named-map-data\","
              "\"profile\":\"named-map-data\",\"side\":\"%s\",\"seed_hex\":\"0000000000000017\","
              "\"construction_epoch_ms\":",
              probe->client ? "client" : "server") ||
        !epoch_projection(json, &construction) || !emit(json, ",\"population\":") ||
        !observe(probe, initialized, false) ||
        !emit(json, ",\"named_data\":{\"session_token\":\"%016" PRIx64 "\",\"initial\":",
              (uint64_t)p->session_token))
        return false;
    bool ready = initialized && probe->failed_stage == NULL;
    mc_world_runtime_fields fields;
    if (ready && mc_world_runtime_fields_get(probe->world, &fields) == MC_WORLD_STORAGE_OK &&
        mc_world_info_retain(fields.x) == MC_WORLD_INFO_OK)
        p->info = fields.x;
    else
        ready = problem(probe, "Named-data actual Info hold", "probe", 1);
    bool observed = false;
    if (!(ready ? boundary_value(probe, &observed) : emit(json, "null")))
        return false;
    ready = ready && observed;
    if (!emit(json, ",\"time_api\":") ||
        !(ready ? persistence_time(probe, &ready) : emit(json, "null")))
        return false;
    static const char *keys[] = {"map_constructor",    "dirty_mark",  "put",
                                 "reader_constructor", "reader_load", "reader_cache_hit"};
    for (unsigned int i = 0; i < 3; ++i)
        if (!emit(json, ",\"%s\":", keys[i]) ||
            !(ready ? named_operation(probe, i, &ready) : emit(json, "null")))
            return false;
    if (!emit(json, ",\"saves\":["))
        return false;
    for (size_t i = 0; ready && i < 2; ++i)
        if (!emit(json, "%s", i == 0 ? "" : ",") || !persistence_save(probe, i, &ready))
            return false;
    if (!emit(json, "]"))
        return false;
    for (unsigned int i = 3; i < 6; ++i)
        if (!emit(json, ",\"%s\":", keys[i]) ||
            !(ready ? named_operation(probe, i, &ready) : emit(json, "null")))
            return false;
    return emit(json, ",\"failure\":") && diagnostic(probe) && emit(json, "}}\n");
}

#endif
