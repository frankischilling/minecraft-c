/* Included only by the canonical ld owner. No alternate region cache or
 * legacy Chunk composer participates in this actual source save path. */
static void save_site(mc_world_source_error *e, const char *cl, int pc) {
    const size_t count = e->frame_count < 6 ? e->frame_count : 5;
    memmove(e->frames + 1, e->frames, count * sizeof(*e->frames));
    e->frames[0] = (mc_world_source_site){
        cl, "a", strcmp(cl, "ld") == 0 ? "(Lfd;Llm;)V" : "(Llm;Lfd;Lnu;)V", pc};
    e->frame_count = count + 1;
}

static mc_world_storage_status save_admission(mc_world_source_chunk_error *error,
                                              mc_world_storage_status status,
                                              const char *operation) {
    if (error != NULL) {
        *error = (mc_world_source_chunk_error){0};
        storage_result(&error->storage, status, operation);
        result(&error->source,
               status == MC_WORLD_STORAGE_BUSY               ? MC_WORLD_SOURCE_BUSY
               : status == MC_WORLD_STORAGE_INVALID_ARGUMENT ? MC_WORLD_SOURCE_ARGUMENT
                                                             : MC_WORLD_SOURCE_STATE,
               operation, NULL, -1);
    }
    return status;
}

static mc_world_storage_status save_done(mc_world_source_chunk_loader *l,
                                         mc_world_storage_status status,
                                         mc_world_source_chunk_error *error) {
    l->work.serializer_pc = l->save.serializer_pc;
    l->work.save_pc = l->save.last_pc;
    if (status != MC_WORLD_STORAGE_OK) {
        l->save.failed = true;
        mc_world_source_error *source = &l->work.source;
        if (source->status == MC_WORLD_SOURCE_OK)
            result(source,
                   status == MC_WORLD_STORAGE_LIMIT               ? MC_WORLD_SOURCE_LIMIT
                   : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_WORLD_SOURCE_ALLOCATION
                   : status == MC_WORLD_STORAGE_BUSY              ? MC_WORLD_SOURCE_BUSY
                                                                  : MC_WORLD_SOURCE_STATE,
                   l->work.storage.operation, NULL, -1);
        /* LD catches Exception only after its first World.r has returned.
         * A blocked child printStackTrace has not returned/thrown to LD. */
        if (source->status == MC_WORLD_SOURCE_SOURCE_EXCEPTION && source->source_exception &&
            !source->diagnostic_frontier) {
            source->cause_site =
                source->frame_count == 0 ? (mc_world_source_site){0} : source->frames[0];
            source->prior_site = (mc_world_source_site){"ld", "a", "(Lfd;Llm;)V", l->save.last_pc};
            source->caught_exception = true;
            source->diagnostic_frontier = true;
            source->status = MC_WORLD_SOURCE_UNIMPLEMENTED;
            source->operation = "java/lang/Exception.printStackTrace()V";
            save_site(source, "ld", 102);
            status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        } else {
            save_site(source, "ld", l->save.last_pc);
        }
        l->work.serializer_pc = l->save.serializer_pc;
        storage_result(&l->work.storage, status, source->operation);
        l->failure = l->work;
    }
    l->state.active = false;
    storage_result(&l->work.storage, status,
                   status == MC_WORLD_STORAGE_OK ? NULL : l->work.storage.operation);
    if (error != NULL)
        *error = l->work;
    return status;
}

static mc_world_storage_status save_native(mc_world_source_chunk_loader *l,
                                           mc_world_storage_status status, const char *operation,
                                           mc_world_source_chunk_error *error) {
    storage_result(&l->work.storage, status, operation);
    return save_done(l, status, error);
}

static mc_world_storage_status save_null(mc_world_source_chunk_loader *l, int pc,
                                         const char *operation,
                                         mc_world_source_chunk_error *error) {
    result(&l->work.source, MC_WORLD_SOURCE_SOURCE_EXCEPTION, operation, NULL, -1);
    l->work.source.source_exception = true;
    l->work.source.native_policy = false;
    l->work.source.exception_class = "java/lang/NullPointerException";
    save_site(&l->work.source, "to", pc);
    return save_done(l, MC_WORLD_STORAGE_INVALID_DATA, error);
}

static bool save_enter(mc_world_source_chunk_loader *l) {
    if (l == NULL || l->state.active || l->state.retiring ||
        l->state.phase != MC_WORLD_SOURCE_READY || l->output == NULL || l->save.source_returned ||
        l->save.failed)
        return false;
    l->state.active = true;
    l->work = (mc_world_source_chunk_error){0};
    return true;
}

mc_world_storage_status
mc_world_source_chunk_loader_save_begin(mc_world_source_chunk_loader *l, mc_world_chunk *chunk,
                                        const mc_java_output_options *options,
                                        mc_world_source_chunk_error *error) {
    if (l == NULL || chunk == NULL || l->state.active || l->state.retiring || l->save.failed ||
        l->state.phase != MC_WORLD_SOURCE_READY)
        return save_admission(error, MC_WORLD_STORAGE_BUSY, "ld source save owner admission");
    if (l->output != NULL && !l->save.source_returned)
        return save_admission(error, MC_WORLD_STORAGE_BUSY, "ld retained incomplete save prefix");
    l->state.active = true;
    l->work = (mc_world_source_chunk_error){0};
    const mc_world_source_status cleared = clear_output(l, &l->work);
    l->save.last_pc = 16;
    if (cleared != MC_WORLD_SOURCE_OK)
        return save_done(l, source_status(cleared), error);
    l->save.last_pc = 16;
    mc_world_chunk_fields fields;
    if (mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK)
        return save_native(l, MC_WORLD_STORAGE_INVALID_ARGUMENT, "ld source Chunk projection",
                           error);
    const mc_world_source_status opened =
        mc_world_source_regions_output(l->state.regions, l->state.source_a, fields.j, fields.k,
                                       options, &l->output, &l->work.output);
    if (opened != MC_WORLD_SOURCE_OK) {
        l->work.source = l->work.output.region.source;
        return save_done(l, source_status(opened), error);
    }
    /* Concrete fresh logical compounds. Their bounded independent native
     * representation is embedded in this stable owner, not a Java heap model. */
    l->save.last_pc = 24;
    l->save_root = (mc_nbt_tag){.type = MC_NBT_COMPOUND};
    l->save.root_constructed = true;
    l->save.last_pc = 33;
    static const uint16_t level_name[] = {'L', 'e', 'v', 'e', 'l'};
    l->save_level = (mc_nbt_tag){.type = MC_NBT_COMPOUND, .name = {level_name, 5}};
    l->save.level_constructed = true;
    l->save.last_pc = 44;
    l->save_root_child = &l->save_level;
    l->save_root.value.compound.items = &l->save_root_child;
    l->save_root.value.compound.length = 1;
    l->save_level.value.compound.items = l->save_children;
    l->save.level_attached = true;
    return save_done(l, MC_WORLD_STORAGE_OK, error);
}

mc_world_storage_status
mc_world_source_chunk_loader_save_prefix(mc_world_source_chunk_loader *l, mc_world_runtime *world,
                                         mc_world_chunk *chunk,
                                         mc_world_source_chunk_error *error) {
    if (world == NULL || chunk == NULL || !save_enter(l))
        return save_admission(error, MC_WORLD_STORAGE_BUSY, "ld source serializer admission");
    l->save.last_pc = 51;
    if (l->save.prefix_returned || !l->save.level_attached)
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "ld source prefix one-shot order",
                           error);
    l->save.serializer_pc = 1;
    mc_world_runtime_fields wf;
    if (mc_world_runtime_fields_get(world, &wf) != MC_WORLD_STORAGE_OK || wf.w != l->state.store)
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "to.a actual World/Store binding",
                           error);
    const mc_world_source_status checked =
        mc_world_store_source_check_session(wf.w, &l->work.source);
    if (checked != MC_WORLD_SOURCE_OK)
        return save_done(l, source_status(checked), error);
    mc_world_chunk_fields fields;
    if (mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK || fields.d != world)
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "to.a actual Chunk/World binding",
                           error);
    static const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    memset(l->save_fields, 0, sizeof(l->save_fields));
    for (size_t i = 0; i < 11; ++i) {
        const size_t n = strlen(names[i]);
        for (size_t j = 0; j < n; ++j)
            l->save_names[i][j] = (uint8_t)names[i][j];
        l->save_fields[i].name = (mc_nbt_string){l->save_names[i], n};
        l->save_children[i] = &l->save_fields[i];
    }
    l->save.serializer_pc = 11;
    l->save_fields[0].type = MC_NBT_INT;
    l->save_fields[0].value.int_value = fields.j;
    l->save_level.value.compound.length = 1;
    l->save.serializer_pc = 21;
    l->save_fields[1].type = MC_NBT_INT;
    l->save_fields[1].value.int_value = fields.k;
    l->save_level.value.compound.length = 2;
    l->save.serializer_pc = 28;
    if (wf.x == NULL)
        return save_null(l, 28, "to.a World time Info receiver", error);
    int64_t time;
    if (mc_world_info_time_get(wf.x, &time) != MC_WORLD_INFO_OK)
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "to.a native Info time owner",
                           error);
    l->save.serializer_pc = 31;
    l->save_fields[2].type = MC_NBT_LONG;
    l->save_fields[2].value.long_value = time;
    l->save_level.value.compound.length = 3;
    l->save.serializer_pc = 41;
    l->save_fields[3].type = MC_NBT_BYTE_ARRAY;
    l->save_fields[3].value.byte_array.data = fields.b;
    l->save_fields[3].value.byte_array.length = MC_CHUNK_BLOCKS;
    l->save_level.value.compound.length = 4;
    const mc_world_chunk_nibble *nibbles[] = {fields.e, fields.f, fields.g};
    const int dereference_pc[] = {51, 64, 77}, put_pc[] = {54, 67, 80};
    for (size_t i = 0; i < 3; ++i) {
        l->save.serializer_pc = dereference_pc[i];
        if (nibbles[i] == NULL)
            return save_null(l, dereference_pc[i], "to.a nibble receiver", error);
        mc_world_chunk_nibble_fields nibble;
        if (mc_world_chunk_nibble_fields_get(nibbles[i], &nibble) != MC_WORLD_STORAGE_OK)
            return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "to.a native nibble projection", error);
        /* Native extents, not an extra source array-length dereference. */
        if (nibble.length != MC_CHUNK_BLOCKS / 2)
            return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "to.a native packed nibble extent", error);
        l->save.serializer_pc = put_pc[i];
        l->save_fields[4 + i].type = MC_NBT_BYTE_ARRAY;
        l->save_fields[4 + i].value.byte_array.data = nibble.a;
        l->save_fields[4 + i].value.byte_array.length = nibble.length;
        l->save_level.value.compound.length = 5 + i;
    }
    l->save.serializer_pc = 90;
    l->save_fields[7].type = MC_NBT_BYTE_ARRAY;
    l->save_fields[7].value.byte_array.data = fields.h;
    l->save_fields[7].value.byte_array.length = MC_CHUNK_HEIGHTMAP_BYTES;
    l->save_level.value.compound.length = 8;
    l->save.serializer_pc = 100;
    l->save_fields[8].type = MC_NBT_BYTE;
    l->save_fields[8].value.byte_value = fields.n ? 1 : 0;
    l->save_level.value.compound.length = 9;
    mc_world_chunk_state(chunk)->has_entities = false;
    l->save.serializer_pc = 105;
    l->save.prefix_returned = true;
    return save_done(l, MC_WORLD_STORAGE_OK, error);
}

static mc_world_storage_status save_records(mc_world_source_chunk_loader *l, bool tile,
                                            mc_nbt_tree **slot,
                                            mc_world_source_chunk_error *error) {
    if (slot == NULL || *slot == NULL || !save_enter(l))
        return save_admission(error, MC_WORLD_STORAGE_BUSY, "ld fresh record transfer admission");
    l->save.last_pc = 51;
    if (!l->save.prefix_returned || (tile ? !l->save.entities_attached || l->save_tiles != NULL
                                          : l->save_entities != NULL || l->save.tiles_attached))
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "ld fresh record transfer order",
                           error);
    const size_t index = tile ? 10 : 9;
    const mc_nbt_tag *root = mc_nbt_tree_root(*slot);
    const mc_nbt_tag *list = mc_nbt_compound_get(root, l->save_fields[index].name.units,
                                                 l->save_fields[index].name.length);
    if (root == NULL || root->type != MC_NBT_COMPOUND || root->value.compound.length != 1 ||
        list == NULL || list->type != MC_NBT_LIST ||
        list->value.list.element_type != MC_NBT_COMPOUND)
        return save_native(l, MC_WORLD_STORAGE_INVALID_DATA, "ld fresh semantic record shape",
                           error);
    l->save.serializer_pc = tile ? 289 : 208;
    l->save_fields[index].type = MC_NBT_LIST;
    l->save_fields[index].value.list = list->value.list;
    l->save_level.value.compound.length = index + 1;
    if (tile) {
        l->save_tiles = *slot;
        l->save.tiles_attached = true;
    } else {
        l->save_entities = *slot;
        l->save.entities_attached = true;
    }
    *slot = NULL;
    return save_done(l, MC_WORLD_STORAGE_OK, error);
}

mc_world_storage_status
mc_world_source_chunk_loader_save_entities(mc_world_source_chunk_loader *l, mc_nbt_tree **slot,
                                           mc_world_source_chunk_error *error) {
    return save_records(l, false, slot, error);
}

mc_world_storage_status
mc_world_source_chunk_loader_save_tiles(mc_world_source_chunk_loader *l, mc_nbt_tree **slot,
                                        mc_world_source_chunk_error *error) {
    return save_records(l, true, slot, error);
}

mc_world_storage_status
mc_world_source_chunk_loader_save_write(mc_world_source_chunk_loader *l, mc_world_runtime *world,
                                        mc_world_chunk *chunk, const mc_nbt_limits *requested,
                                        uint32_t *growth, mc_world_source_chunk_error *error) {
    if (world == NULL || chunk == NULL || growth == NULL || !save_enter(l))
        return save_admission(error, MC_WORLD_STORAGE_BUSY, "ld source emit admission");
    l->save.last_pc = 51;
    if (!l->save.prefix_returned || !l->save.entities_attached || !l->save.tiles_attached ||
        l->save_document != NULL || l->save_entities == NULL || l->save_tiles == NULL)
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "ld source serializer phase",
                           error);
    mc_nbt_limits limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    const size_t entity_bytes = mc_nbt_tree_allocation_bytes(l->save_entities);
    const size_t tile_bytes = mc_nbt_tree_allocation_bytes(l->save_tiles);
    if (entity_bytes > limits.max_allocation_bytes ||
        tile_bytes > limits.max_allocation_bytes - entity_bytes)
        return save_native(l, MC_WORLD_STORAGE_LIMIT, "ld fresh source list overlap budget", error);
    limits.max_allocation_bytes -= entity_bytes + tile_bytes;
    const mc_nbt_status built =
        mc_nbt_tree_copy_compound(&l->save_root, &limits, &l->save_document, &l->work.output_tree);
    if (built != MC_NBT_OK)
        return save_native(l,
                           built == MC_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                           : built == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                               : MC_WORLD_STORAGE_INVALID_DATA,
                           "ld fresh source root tree safety policy", error);
    l->save.serializer_pc = 292;
    l->save.last_pc = 57;
    const size_t tree_bytes = mc_nbt_tree_allocation_bytes(l->save_document);
    if (tree_bytes > limits.max_allocation_bytes)
        return save_native(l, MC_WORLD_STORAGE_LIMIT, "ld fresh source tree/emitter overlap budget",
                           error);
    limits.max_allocation_bytes -= tree_bytes;
    mc_world_source_status status = mc_world_source_region_output_write_compound(
        l->output, mc_nbt_tree_root(l->save_document), &limits, &l->work.output);
    if (status != MC_WORLD_SOURCE_OK) {
        l->work.source = l->work.output.region.source;
        return save_done(l, source_status(status), error);
    }
    l->save.nbt_written = true;
    l->save.last_pc = 61;
    status = mc_world_source_region_output_source_close(l->output, &l->work.output);
    if (status != MC_WORLD_SOURCE_OK) {
        l->work.source = l->work.output.region.source;
        return save_done(l, source_status(status), error);
    }
    l->save.closed = true;
    l->save.last_pc = 65;
    mc_world_runtime_fields wf;
    if (mc_world_runtime_fields_get(world, &wf) != MC_WORLD_STORAGE_OK)
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "ld actual late World Info getter", error);
    mc_world_info *info = wf.x;
    l->save.last_pc = 74;
    if (info == NULL) {
        result(&l->work.source, MC_WORLD_SOURCE_SOURCE_EXCEPTION, "ld late Info size receiver",
               NULL, -1);
        l->work.source.source_exception = true;
        l->work.source.native_policy = false;
        l->work.source.exception_class = "java/lang/NullPointerException";
        save_site(&l->work.source, "ld", 74);
        return save_done(l, MC_WORLD_STORAGE_INVALID_DATA, error);
    }
    int64_t size;
    if (mc_world_info_size_get(info, &size) != MC_WORLD_INFO_OK)
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "ld native late Info size owner",
                           error);
    mc_world_chunk_fields fields;
    if (mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK)
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "ld late Chunk coordinate projection", error);
    l->save.last_pc = 89;
    status = mc_world_source_regions_consume_growth(l->state.regions, l->state.source_a, fields.j,
                                                    fields.k, &l->save.growth, &l->work.region);
    if (status != MC_WORLD_SOURCE_OK) {
        l->work.source = l->work.region.source;
        return save_done(l, source_status(status), error);
    }
    l->save.last_pc = 94;
    if (mc_world_info_size_set(info, mc_java_i64_add(size, (int64_t)l->save.growth)) !=
        MC_WORLD_INFO_OK)
        return save_native(l, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "ld native late Info size store",
                           error);
    l->save.last_pc = 105;
    l->save.source_returned = true;
    *growth = (uint32_t)l->save.growth;
    return save_done(l, MC_WORLD_STORAGE_OK, error);
}

mc_world_storage_status
mc_world_source_chunk_loader_save_fail(mc_world_source_chunk_loader *l,
                                       const mc_world_source_chunk_error *error) {
    if (l == NULL || error == NULL || error->storage.status == MC_WORLD_STORAGE_OK)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (l->state.active || l->state.retiring)
        return MC_WORLD_STORAGE_BUSY;
    if (l->save.failed)
        return MC_WORLD_STORAGE_OK;
    if (l->output == NULL || l->save.source_returned)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    l->save.failed = true;
    l->failure = *error;
    if (error->save_pc > 0)
        l->save.last_pc = error->save_pc;
    if (error->serializer_pc > 0)
        l->save.serializer_pc = error->serializer_pc;
    return MC_WORLD_STORAGE_OK;
}
