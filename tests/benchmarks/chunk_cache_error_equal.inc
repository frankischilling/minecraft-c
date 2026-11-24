static bool ew_error_equal(const mc_client_chunk_cache_error *a,
                           const mc_client_chunk_cache_error *b) {
    /* Generated from actual public declarations. Every scalar, string,
     * retained identity and nested record is compared, without padding.
     * NBT payload unions are inactive: both statuses must be OK, since a
     * block getter reaches no Entity/NBT descendant. */
    if ((*a).status != (*b).status)
        return false;
    if (!ew_text_equal((*a).operation, (*b).operation))
        return false;
    if (!ew_text_equal((*a).method, (*b).method))
        return false;
    if (!ew_text_equal((*a).descriptor, (*b).descriptor))
        return false;
    if ((*a).bytecode != (*b).bytecode)
        return false;
    if ((*a).child_bytecode != (*b).child_bytecode)
        return false;
    if ((*a).native_policy != (*b).native_policy)
        return false;
    for (size_t i4 = 0; i4 < 6u; ++i4) {
        if (!ew_text_equal((*a).nested[i4].method, (*b).nested[i4].method))
            return false;
        if (!ew_text_equal((*a).nested[i4].descriptor, (*b).nested[i4].descriptor))
            return false;
        if ((*a).nested[i4].bytecode != (*b).nested[i4].bytecode)
            return false;
    }
    if ((*a).nested_count != (*b).nested_count)
        return false;
    if ((*a).storage.status != (*b).storage.status)
        return false;
    if (!ew_text_equal((*a).storage.operation, (*b).storage.operation))
        return false;
    if ((*a).storage.system_error != (*b).storage.system_error)
        return false;
    if ((*a).acquisition.status != (*b).acquisition.status)
        return false;
    if ((*a).acquisition.cause.status != (*b).acquisition.cause.status)
        return false;
    if (!ew_text_equal((*a).acquisition.cause.operation, (*b).acquisition.cause.operation))
        return false;
    if ((*a).acquisition.cause.system_error != (*b).acquisition.cause.system_error)
        return false;
    if ((*a).acquisition.registry.status != (*b).acquisition.registry.status)
        return false;
    if (!ew_text_equal((*a).acquisition.registry.message, (*b).acquisition.registry.message))
        return false;
    if ((*a).acquisition.registry.initializer_pc != (*b).acquisition.registry.initializer_pc)
        return false;
    if ((*a).acquisition.registry.block_id != (*b).acquisition.registry.block_id)
        return false;
    if (!ew_text_equal((*a).acquisition.registry.constructor_class,
                       (*b).acquisition.registry.constructor_class))
        return false;
    if ((*a).acquisition.registry.constructor_pc != (*b).acquisition.registry.constructor_pc)
        return false;
    if ((*a).acquisition.registry.profile_status != (*b).acquisition.registry.profile_status)
        return false;
    if (!ew_text_equal((*a).acquisition.registry.active_initializer_class,
                       (*b).acquisition.registry.active_initializer_class))
        return false;
    if ((*a).acquisition.registry.active_initializer_pc !=
        (*b).acquisition.registry.active_initializer_pc)
        return false;
    if ((*a).acquisition.registry.item_argument != (*b).acquisition.registry.item_argument)
        return false;
    if ((*a).acquisition.registry.format_error.status !=
        (*b).acquisition.registry.format_error.status)
        return false;
    if ((*a).acquisition.registry.format_error.offset !=
        (*b).acquisition.registry.format_error.offset)
        return false;
    if ((*a).acquisition.registry.format_error.length !=
        (*b).acquisition.registry.format_error.length)
        return false;
    if ((*a).acquisition.registry.format_error.value !=
        (*b).acquisition.registry.format_error.value)
        return false;
    if ((*a).acquisition.registry.format_error.flags !=
        (*b).acquisition.registry.format_error.flags)
        return false;
    if ((*a).acquisition.registry.format_error.conversion !=
        (*b).acquisition.registry.format_error.conversion)
        return false;
    if ((*a).acquisition.registry.format_error.date_time !=
        (*b).acquisition.registry.format_error.date_time)
        return false;
    if ((*a).acquisition.tile_entity.status != (*b).acquisition.tile_entity.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.operation,
                       (*b).acquisition.tile_entity.operation))
        return false;
    if ((*a).acquisition.tile_entity.bytecode != (*b).acquisition.tile_entity.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.chunk_bytecode != (*b).acquisition.tile_entity.chunk_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.world_access_bytecode !=
        (*b).acquisition.tile_entity.world_access_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.initializer_bytecode !=
        (*b).acquisition.tile_entity.initializer_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.cause.status != (*b).acquisition.tile_entity.cause.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.cause.operation,
                       (*b).acquisition.tile_entity.cause.operation))
        return false;
    if ((*a).acquisition.tile_entity.cause.system_error !=
        (*b).acquisition.tile_entity.cause.system_error)
        return false;
    if ((*a).acquisition.tile_entity.registry.status !=
        (*b).acquisition.tile_entity.registry.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.registry.message,
                       (*b).acquisition.tile_entity.registry.message))
        return false;
    if ((*a).acquisition.tile_entity.registry.initializer_pc !=
        (*b).acquisition.tile_entity.registry.initializer_pc)
        return false;
    if ((*a).acquisition.tile_entity.registry.block_id !=
        (*b).acquisition.tile_entity.registry.block_id)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.registry.constructor_class,
                       (*b).acquisition.tile_entity.registry.constructor_class))
        return false;
    if ((*a).acquisition.tile_entity.registry.constructor_pc !=
        (*b).acquisition.tile_entity.registry.constructor_pc)
        return false;
    if ((*a).acquisition.tile_entity.registry.profile_status !=
        (*b).acquisition.tile_entity.registry.profile_status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.registry.active_initializer_class,
                       (*b).acquisition.tile_entity.registry.active_initializer_class))
        return false;
    if ((*a).acquisition.tile_entity.registry.active_initializer_pc !=
        (*b).acquisition.tile_entity.registry.active_initializer_pc)
        return false;
    if ((*a).acquisition.tile_entity.registry.item_argument !=
        (*b).acquisition.tile_entity.registry.item_argument)
        return false;
    if ((*a).acquisition.tile_entity.registry.format_error.status !=
        (*b).acquisition.tile_entity.registry.format_error.status)
        return false;
    if ((*a).acquisition.tile_entity.registry.format_error.offset !=
        (*b).acquisition.tile_entity.registry.format_error.offset)
        return false;
    if ((*a).acquisition.tile_entity.registry.format_error.length !=
        (*b).acquisition.tile_entity.registry.format_error.length)
        return false;
    if ((*a).acquisition.tile_entity.registry.format_error.value !=
        (*b).acquisition.tile_entity.registry.format_error.value)
        return false;
    if ((*a).acquisition.tile_entity.registry.format_error.flags !=
        (*b).acquisition.tile_entity.registry.format_error.flags)
        return false;
    if ((*a).acquisition.tile_entity.registry.format_error.conversion !=
        (*b).acquisition.tile_entity.registry.format_error.conversion)
        return false;
    if ((*a).acquisition.tile_entity.registry.format_error.date_time !=
        (*b).acquisition.tile_entity.registry.format_error.date_time)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.status !=
        (*b).acquisition.tile_entity.tile_entity.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.tile_entity.field,
                       (*b).acquisition.tile_entity.tile_entity.field))
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.item_index !=
        (*b).acquisition.tile_entity.tile_entity.item_index)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.item.status !=
        (*b).acquisition.tile_entity.tile_entity.item.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.tile_entity.item.field,
                       (*b).acquisition.tile_entity.tile_entity.item.field))
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.item.nbt.status !=
        (*b).acquisition.tile_entity.tile_entity.item.nbt.status)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.item.nbt.offset !=
        (*b).acquisition.tile_entity.tile_entity.item.nbt.offset)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.nbt.status !=
        (*b).acquisition.tile_entity.tile_entity.nbt.status)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.nbt.offset !=
        (*b).acquisition.tile_entity.tile_entity.nbt.offset)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.source_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.source_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.base_read_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.base_read_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.read_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.read_bytecode)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.tile_entity.operation,
                       (*b).acquisition.tile_entity.tile_entity.operation))
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.getter_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.getter_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.base_save_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.base_save_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.save_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.save_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.item_save_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.item_save_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.status !=
        (*b).acquisition.tile_entity.tile_entity.registry.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.tile_entity.registry.message,
                       (*b).acquisition.tile_entity.tile_entity.registry.message))
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.initializer_pc !=
        (*b).acquisition.tile_entity.tile_entity.registry.initializer_pc)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.block_id !=
        (*b).acquisition.tile_entity.tile_entity.registry.block_id)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.tile_entity.registry.constructor_class,
                       (*b).acquisition.tile_entity.tile_entity.registry.constructor_class))
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.constructor_pc !=
        (*b).acquisition.tile_entity.tile_entity.registry.constructor_pc)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.profile_status !=
        (*b).acquisition.tile_entity.tile_entity.registry.profile_status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.tile_entity.registry.active_initializer_class,
                       (*b).acquisition.tile_entity.tile_entity.registry.active_initializer_class))
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.active_initializer_pc !=
        (*b).acquisition.tile_entity.tile_entity.registry.active_initializer_pc)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.item_argument !=
        (*b).acquisition.tile_entity.tile_entity.registry.item_argument)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.format_error.status !=
        (*b).acquisition.tile_entity.tile_entity.registry.format_error.status)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.format_error.offset !=
        (*b).acquisition.tile_entity.tile_entity.registry.format_error.offset)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.format_error.length !=
        (*b).acquisition.tile_entity.tile_entity.registry.format_error.length)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.format_error.value !=
        (*b).acquisition.tile_entity.tile_entity.registry.format_error.value)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.format_error.flags !=
        (*b).acquisition.tile_entity.tile_entity.registry.format_error.flags)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.format_error.conversion !=
        (*b).acquisition.tile_entity.tile_entity.registry.format_error.conversion)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.registry.format_error.date_time !=
        (*b).acquisition.tile_entity.tile_entity.registry.format_error.date_time)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.constructor_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.constructor_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.initializer_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.initializer_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity.piston_initializer_bytecode !=
        (*b).acquisition.tile_entity.tile_entity.piston_initializer_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.entity.status != (*b).acquisition.tile_entity.entity.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.entity.operation,
                       (*b).acquisition.tile_entity.entity.operation))
        return false;
    if ((*a).acquisition.tile_entity.entity.bytecode !=
        (*b).acquisition.tile_entity.entity.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.entity.profile != (*b).acquisition.tile_entity.entity.profile)
        return false;
    if ((*a).acquisition.tile_entity.entity.item != (*b).acquisition.tile_entity.entity.item)
        return false;
    if ((*a).acquisition.tile_entity.entity.world != (*b).acquisition.tile_entity.entity.world)
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_world_constructor.selected !=
        (*b).acquisition.tile_entity.entity.minecart_world_constructor.selected)
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_world_constructor.entered !=
        (*b).acquisition.tile_entity.entity.minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.entity.minecart_world_constructor.declaring_class,
            (*b).acquisition.tile_entity.entity.minecart_world_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.entity.minecart_world_constructor.method,
                       (*b).acquisition.tile_entity.entity.minecart_world_constructor.method))
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_world_constructor.bytecode !=
        (*b).acquisition.tile_entity.entity.minecart_world_constructor.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_constructor.selected !=
        (*b).acquisition.tile_entity.entity.minecart_constructor.selected)
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_constructor.entered !=
        (*b).acquisition.tile_entity.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.entity.minecart_constructor.declaring_class,
                       (*b).acquisition.tile_entity.entity.minecart_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.entity.minecart_constructor.method,
                       (*b).acquisition.tile_entity.entity.minecart_constructor.method))
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_constructor.bytecode !=
        (*b).acquisition.tile_entity.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_initializer.selected !=
        (*b).acquisition.tile_entity.entity.minecart_initializer.selected)
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_initializer.entered !=
        (*b).acquisition.tile_entity.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.entity.minecart_initializer.declaring_class,
                       (*b).acquisition.tile_entity.entity.minecart_initializer.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.entity.minecart_initializer.method,
                       (*b).acquisition.tile_entity.entity.minecart_initializer.method))
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_initializer.bytecode !=
        (*b).acquisition.tile_entity.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_initializer_cause.selected !=
        (*b).acquisition.tile_entity.entity.minecart_initializer_cause.selected)
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_initializer_cause.entered !=
        (*b).acquisition.tile_entity.entity.minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.entity.minecart_initializer_cause.declaring_class,
            (*b).acquisition.tile_entity.entity.minecart_initializer_cause.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.entity.minecart_initializer_cause.method,
                       (*b).acquisition.tile_entity.entity.minecart_initializer_cause.method))
        return false;
    if ((*a).acquisition.tile_entity.entity.minecart_initializer_cause.bytecode !=
        (*b).acquisition.tile_entity.entity.minecart_initializer_cause.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.status !=
        (*b).acquisition.tile_entity.publication.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.publication.operation,
                       (*b).acquisition.tile_entity.publication.operation))
        return false;
    if ((*a).acquisition.tile_entity.publication.bytecode !=
        (*b).acquisition.tile_entity.publication.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.presence_bytecode !=
        (*b).acquisition.tile_entity.publication.presence_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.provider_bytecode !=
        (*b).acquisition.tile_entity.publication.provider_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.chunk_bytecode !=
        (*b).acquisition.tile_entity.publication.chunk_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.world_access_bytecode !=
        (*b).acquisition.tile_entity.publication.world_access_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.dedicated_bytecode !=
        (*b).acquisition.tile_entity.publication.dedicated_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.index_bytecode !=
        (*b).acquisition.tile_entity.publication.index_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.acquisition !=
        (*b).acquisition.tile_entity.publication.acquisition)
        return false;
    if ((*a).acquisition.tile_entity.publication.cause.status !=
        (*b).acquisition.tile_entity.publication.cause.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.publication.cause.operation,
                       (*b).acquisition.tile_entity.publication.cause.operation))
        return false;
    if ((*a).acquisition.tile_entity.publication.cause.system_error !=
        (*b).acquisition.tile_entity.publication.cause.system_error)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.status !=
        (*b).acquisition.tile_entity.publication.entity.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.publication.entity.operation,
                       (*b).acquisition.tile_entity.publication.entity.operation))
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.bytecode !=
        (*b).acquisition.tile_entity.publication.entity.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.profile !=
        (*b).acquisition.tile_entity.publication.entity.profile)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.item !=
        (*b).acquisition.tile_entity.publication.entity.item)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.world !=
        (*b).acquisition.tile_entity.publication.entity.world)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_world_constructor.selected !=
        (*b).acquisition.tile_entity.publication.entity.minecart_world_constructor.selected)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_world_constructor.entered !=
        (*b).acquisition.tile_entity.publication.entity.minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.publication.entity.minecart_world_constructor
                           .declaring_class,
                       (*b).acquisition.tile_entity.publication.entity.minecart_world_constructor
                           .declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.publication.entity.minecart_world_constructor.method,
            (*b).acquisition.tile_entity.publication.entity.minecart_world_constructor.method))
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_world_constructor.bytecode !=
        (*b).acquisition.tile_entity.publication.entity.minecart_world_constructor.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_constructor.selected !=
        (*b).acquisition.tile_entity.publication.entity.minecart_constructor.selected)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_constructor.entered !=
        (*b).acquisition.tile_entity.publication.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.publication.entity.minecart_constructor.declaring_class,
            (*b).acquisition.tile_entity.publication.entity.minecart_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.publication.entity.minecart_constructor.method,
                       (*b).acquisition.tile_entity.publication.entity.minecart_constructor.method))
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_constructor.bytecode !=
        (*b).acquisition.tile_entity.publication.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_initializer.selected !=
        (*b).acquisition.tile_entity.publication.entity.minecart_initializer.selected)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_initializer.entered !=
        (*b).acquisition.tile_entity.publication.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.publication.entity.minecart_initializer.declaring_class,
            (*b).acquisition.tile_entity.publication.entity.minecart_initializer.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.publication.entity.minecart_initializer.method,
                       (*b).acquisition.tile_entity.publication.entity.minecart_initializer.method))
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_initializer.bytecode !=
        (*b).acquisition.tile_entity.publication.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_initializer_cause.selected !=
        (*b).acquisition.tile_entity.publication.entity.minecart_initializer_cause.selected)
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_initializer_cause.entered !=
        (*b).acquisition.tile_entity.publication.entity.minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.publication.entity.minecart_initializer_cause
                           .declaring_class,
                       (*b).acquisition.tile_entity.publication.entity.minecart_initializer_cause
                           .declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.publication.entity.minecart_initializer_cause.method,
            (*b).acquisition.tile_entity.publication.entity.minecart_initializer_cause.method))
        return false;
    if ((*a).acquisition.tile_entity.publication.entity.minecart_initializer_cause.bytecode !=
        (*b).acquisition.tile_entity.publication.entity.minecart_initializer_cause.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.sleep.bytecode !=
        (*b).acquisition.tile_entity.publication.sleep.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.sleep.player_bytecode !=
        (*b).acquisition.tile_entity.publication.sleep.player_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.publication.sleep.index !=
        (*b).acquisition.tile_entity.publication.sleep.index)
        return false;
    if ((*a).acquisition.tile_entity.publication.world_access_failure.receiver !=
        (*b).acquisition.tile_entity.publication.world_access_failure.receiver)
        return false;
    if ((*a).acquisition.tile_entity.publication.world_access_failure.render_global !=
        (*b).acquisition.tile_entity.publication.world_access_failure.render_global)
        return false;
    if ((*a).acquisition.tile_entity.publication.world_access_failure.receipt !=
        (*b).acquisition.tile_entity.publication.world_access_failure.receipt)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.status !=
        (*b).acquisition.tile_entity.batch.publication.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.batch.publication.operation,
                       (*b).acquisition.tile_entity.batch.publication.operation))
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.bytecode !=
        (*b).acquisition.tile_entity.batch.publication.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.presence_bytecode !=
        (*b).acquisition.tile_entity.batch.publication.presence_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.provider_bytecode !=
        (*b).acquisition.tile_entity.batch.publication.provider_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.chunk_bytecode !=
        (*b).acquisition.tile_entity.batch.publication.chunk_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.world_access_bytecode !=
        (*b).acquisition.tile_entity.batch.publication.world_access_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.dedicated_bytecode !=
        (*b).acquisition.tile_entity.batch.publication.dedicated_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.index_bytecode !=
        (*b).acquisition.tile_entity.batch.publication.index_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.acquisition !=
        (*b).acquisition.tile_entity.batch.publication.acquisition)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.cause.status !=
        (*b).acquisition.tile_entity.batch.publication.cause.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.batch.publication.cause.operation,
                       (*b).acquisition.tile_entity.batch.publication.cause.operation))
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.cause.system_error !=
        (*b).acquisition.tile_entity.batch.publication.cause.system_error)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.status !=
        (*b).acquisition.tile_entity.batch.publication.entity.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.batch.publication.entity.operation,
                       (*b).acquisition.tile_entity.batch.publication.entity.operation))
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.bytecode !=
        (*b).acquisition.tile_entity.batch.publication.entity.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.profile !=
        (*b).acquisition.tile_entity.batch.publication.entity.profile)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.item !=
        (*b).acquisition.tile_entity.batch.publication.entity.item)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.world !=
        (*b).acquisition.tile_entity.batch.publication.entity.world)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_world_constructor.selected !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_world_constructor.selected)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_world_constructor.entered !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.batch.publication.entity
                           .minecart_world_constructor.declaring_class,
                       (*b).acquisition.tile_entity.batch.publication.entity
                           .minecart_world_constructor.declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.batch.publication.entity.minecart_world_constructor.method,
            (*b).acquisition.tile_entity.batch.publication.entity.minecart_world_constructor
                .method))
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_world_constructor.bytecode !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_world_constructor.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_constructor.selected !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_constructor.selected)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_constructor.entered !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.batch.publication.entity.minecart_constructor
                           .declaring_class,
                       (*b).acquisition.tile_entity.batch.publication.entity.minecart_constructor
                           .declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.batch.publication.entity.minecart_constructor.method,
            (*b).acquisition.tile_entity.batch.publication.entity.minecart_constructor.method))
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_constructor.bytecode !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_initializer.selected !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_initializer.selected)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_initializer.entered !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.batch.publication.entity.minecart_initializer
                           .declaring_class,
                       (*b).acquisition.tile_entity.batch.publication.entity.minecart_initializer
                           .declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.batch.publication.entity.minecart_initializer.method,
            (*b).acquisition.tile_entity.batch.publication.entity.minecart_initializer.method))
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_initializer.bytecode !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_initializer_cause.selected !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_initializer_cause.selected)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_initializer_cause.entered !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.batch.publication.entity
                           .minecart_initializer_cause.declaring_class,
                       (*b).acquisition.tile_entity.batch.publication.entity
                           .minecart_initializer_cause.declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.batch.publication.entity.minecart_initializer_cause.method,
            (*b).acquisition.tile_entity.batch.publication.entity.minecart_initializer_cause
                .method))
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.entity.minecart_initializer_cause.bytecode !=
        (*b).acquisition.tile_entity.batch.publication.entity.minecart_initializer_cause.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.sleep.bytecode !=
        (*b).acquisition.tile_entity.batch.publication.sleep.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.sleep.player_bytecode !=
        (*b).acquisition.tile_entity.batch.publication.sleep.player_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.sleep.index !=
        (*b).acquisition.tile_entity.batch.publication.sleep.index)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.world_access_failure.receiver !=
        (*b).acquisition.tile_entity.batch.publication.world_access_failure.receiver)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.world_access_failure.render_global !=
        (*b).acquisition.tile_entity.batch.publication.world_access_failure.render_global)
        return false;
    if ((*a).acquisition.tile_entity.batch.publication.world_access_failure.receipt !=
        (*b).acquisition.tile_entity.batch.publication.world_access_failure.receipt)
        return false;
    if ((*a).acquisition.tile_entity.batch.index != (*b).acquisition.tile_entity.batch.index)
        return false;
    if ((*a).acquisition.tile_entity.caller_bytecode !=
        (*b).acquisition.tile_entity.caller_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.hook_bytecode != (*b).acquisition.tile_entity.hook_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.remove_bytecode !=
        (*b).acquisition.tile_entity.remove_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.tile_entity_bytecode !=
        (*b).acquisition.tile_entity.tile_entity_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.inventory_bytecode !=
        (*b).acquisition.tile_entity.inventory_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_caller_bytecode !=
        (*b).acquisition.tile_entity.acquisition_caller_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.activation_bytecode !=
        (*b).acquisition.tile_entity.activation_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.activation_world_bytecode !=
        (*b).acquisition.tile_entity.activation_world_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_bytecode != (*b).acquisition.tile_entity.load_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_builder_bytecode !=
        (*b).acquisition.tile_entity.load_builder_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.index !=
        (*b).acquisition.tile_entity.load_entity.index)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.status !=
        (*b).acquisition.tile_entity.load_entity.factory.status)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.late_hook !=
        (*b).acquisition.tile_entity.load_entity.factory.late_hook)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.factory.operation,
                       (*b).acquisition.tile_entity.load_entity.factory.operation))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.factory.field,
                       (*b).acquisition.tile_entity.load_entity.factory.field))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.index !=
        (*b).acquisition.tile_entity.load_entity.factory.index)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.wrapper_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.wrapper_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.name_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.name_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.factory_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.factory_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.base_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.base_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.item_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.item_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.falling_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.falling_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.stack_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.stack_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.local_player_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.local_player_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.player_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.player_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.initializer_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.initializer_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.registration_bytecode !=
        (*b).acquisition.tile_entity.load_entity.factory.registration_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.factory.status != MC_ENTITY_NBT_OK ||
        (*b).acquisition.tile_entity.load_entity.factory.status != MC_ENTITY_NBT_OK)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.status !=
        (*b).acquisition.tile_entity.load_entity.provenance.status)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.late_hook !=
        (*b).acquisition.tile_entity.load_entity.provenance.late_hook)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.provenance.operation,
                       (*b).acquisition.tile_entity.load_entity.provenance.operation))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.provenance.field,
                       (*b).acquisition.tile_entity.load_entity.provenance.field))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.index !=
        (*b).acquisition.tile_entity.load_entity.provenance.index)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.wrapper_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.wrapper_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.name_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.name_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.factory_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.factory_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.base_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.base_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.item_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.item_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.falling_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.falling_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.stack_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.stack_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.local_player_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.local_player_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.player_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.player_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.initializer_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.initializer_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.registration_bytecode !=
        (*b).acquisition.tile_entity.load_entity.provenance.registration_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.provenance.status != MC_ENTITY_NBT_OK ||
        (*b).acquisition.tile_entity.load_entity.provenance.status != MC_ENTITY_NBT_OK)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.status !=
        (*b).acquisition.tile_entity.load_entity.insertion.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.insertion.operation,
                       (*b).acquisition.tile_entity.load_entity.insertion.operation))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.bytecode !=
        (*b).acquisition.tile_entity.load_entity.insertion.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.cause.status !=
        (*b).acquisition.tile_entity.load_entity.insertion.cause.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.insertion.cause.operation,
                       (*b).acquisition.tile_entity.load_entity.insertion.cause.operation))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.cause.system_error !=
        (*b).acquisition.tile_entity.load_entity.insertion.cause.system_error)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.status !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.insertion.entity.operation,
                       (*b).acquisition.tile_entity.load_entity.insertion.entity.operation))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.bytecode !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.profile !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.profile)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.item !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.item)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.world !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.world)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_world_constructor
            .selected != (*b).acquisition.tile_entity.load_entity.insertion.entity
                             .minecart_world_constructor.selected)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_world_constructor
            .entered != (*b).acquisition.tile_entity.load_entity.insertion.entity
                            .minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_world_constructor.declaring_class,
                       (*b).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_world_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_world_constructor.method,
                       (*b).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_world_constructor.method))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_world_constructor
            .bytecode != (*b).acquisition.tile_entity.load_entity.insertion.entity
                             .minecart_world_constructor.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_constructor.selected !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.minecart_constructor.selected)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_constructor.entered !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_constructor.declaring_class,
                       (*b).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_constructor.declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_constructor.method,
            (*b).acquisition.tile_entity.load_entity.insertion.entity.minecart_constructor.method))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_constructor.bytecode !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer.selected !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer.selected)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer.entered !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_initializer.declaring_class,
                       (*b).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_initializer.declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer.method,
            (*b).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer.method))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer.bytecode !=
        (*b).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer_cause
            .selected != (*b).acquisition.tile_entity.load_entity.insertion.entity
                             .minecart_initializer_cause.selected)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer_cause
            .entered != (*b).acquisition.tile_entity.load_entity.insertion.entity
                            .minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_initializer_cause.declaring_class,
                       (*b).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_initializer_cause.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_initializer_cause.method,
                       (*b).acquisition.tile_entity.load_entity.insertion.entity
                           .minecart_initializer_cause.method))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.insertion.entity.minecart_initializer_cause
            .bytecode != (*b).acquisition.tile_entity.load_entity.insertion.entity
                             .minecart_initializer_cause.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.load_entity.native.status !=
        (*b).acquisition.tile_entity.load_entity.native.status)
        return false;
    if (!ew_text_equal((*a).acquisition.tile_entity.load_entity.native.operation,
                       (*b).acquisition.tile_entity.load_entity.native.operation))
        return false;
    if ((*a).acquisition.tile_entity.load_entity.native.system_error !=
        (*b).acquisition.tile_entity.load_entity.native.system_error)
        return false;
    if ((*a).acquisition.tile_entity.provider_bytecode !=
        (*b).acquisition.tile_entity.provider_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.chunk_bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.chunk_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.world_access_bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.world_access_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.initializer_bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.initializer_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.caller_bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.caller_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.hook_bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.hook_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.remove_bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.remove_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.provider_bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.provider_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.tile_entity_bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.tile_entity_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.acquisition_parent.inventory_bytecode !=
        (*b).acquisition.tile_entity.acquisition_parent.inventory_bytecode)
        return false;
    if ((*a).acquisition.tile_entity.world_access_failure.receiver !=
        (*b).acquisition.tile_entity.world_access_failure.receiver)
        return false;
    if ((*a).acquisition.tile_entity.world_access_failure.render_global !=
        (*b).acquisition.tile_entity.world_access_failure.render_global)
        return false;
    if ((*a).acquisition.tile_entity.world_access_failure.receipt !=
        (*b).acquisition.tile_entity.world_access_failure.receipt)
        return false;
    if ((*a).acquisition.batch.publication.status != (*b).acquisition.batch.publication.status)
        return false;
    if (!ew_text_equal((*a).acquisition.batch.publication.operation,
                       (*b).acquisition.batch.publication.operation))
        return false;
    if ((*a).acquisition.batch.publication.bytecode != (*b).acquisition.batch.publication.bytecode)
        return false;
    if ((*a).acquisition.batch.publication.presence_bytecode !=
        (*b).acquisition.batch.publication.presence_bytecode)
        return false;
    if ((*a).acquisition.batch.publication.provider_bytecode !=
        (*b).acquisition.batch.publication.provider_bytecode)
        return false;
    if ((*a).acquisition.batch.publication.chunk_bytecode !=
        (*b).acquisition.batch.publication.chunk_bytecode)
        return false;
    if ((*a).acquisition.batch.publication.world_access_bytecode !=
        (*b).acquisition.batch.publication.world_access_bytecode)
        return false;
    if ((*a).acquisition.batch.publication.dedicated_bytecode !=
        (*b).acquisition.batch.publication.dedicated_bytecode)
        return false;
    if ((*a).acquisition.batch.publication.index_bytecode !=
        (*b).acquisition.batch.publication.index_bytecode)
        return false;
    if ((*a).acquisition.batch.publication.acquisition !=
        (*b).acquisition.batch.publication.acquisition)
        return false;
    if ((*a).acquisition.batch.publication.cause.status !=
        (*b).acquisition.batch.publication.cause.status)
        return false;
    if (!ew_text_equal((*a).acquisition.batch.publication.cause.operation,
                       (*b).acquisition.batch.publication.cause.operation))
        return false;
    if ((*a).acquisition.batch.publication.cause.system_error !=
        (*b).acquisition.batch.publication.cause.system_error)
        return false;
    if ((*a).acquisition.batch.publication.entity.status !=
        (*b).acquisition.batch.publication.entity.status)
        return false;
    if (!ew_text_equal((*a).acquisition.batch.publication.entity.operation,
                       (*b).acquisition.batch.publication.entity.operation))
        return false;
    if ((*a).acquisition.batch.publication.entity.bytecode !=
        (*b).acquisition.batch.publication.entity.bytecode)
        return false;
    if ((*a).acquisition.batch.publication.entity.profile !=
        (*b).acquisition.batch.publication.entity.profile)
        return false;
    if ((*a).acquisition.batch.publication.entity.item !=
        (*b).acquisition.batch.publication.entity.item)
        return false;
    if ((*a).acquisition.batch.publication.entity.world !=
        (*b).acquisition.batch.publication.entity.world)
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_world_constructor.selected !=
        (*b).acquisition.batch.publication.entity.minecart_world_constructor.selected)
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_world_constructor.entered !=
        (*b).acquisition.batch.publication.entity.minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.batch.publication.entity.minecart_world_constructor.declaring_class,
            (*b).acquisition.batch.publication.entity.minecart_world_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.batch.publication.entity.minecart_world_constructor.method,
                       (*b).acquisition.batch.publication.entity.minecart_world_constructor.method))
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_world_constructor.bytecode !=
        (*b).acquisition.batch.publication.entity.minecart_world_constructor.bytecode)
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_constructor.selected !=
        (*b).acquisition.batch.publication.entity.minecart_constructor.selected)
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_constructor.entered !=
        (*b).acquisition.batch.publication.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.batch.publication.entity.minecart_constructor.declaring_class,
            (*b).acquisition.batch.publication.entity.minecart_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.batch.publication.entity.minecart_constructor.method,
                       (*b).acquisition.batch.publication.entity.minecart_constructor.method))
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_constructor.bytecode !=
        (*b).acquisition.batch.publication.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_initializer.selected !=
        (*b).acquisition.batch.publication.entity.minecart_initializer.selected)
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_initializer.entered !=
        (*b).acquisition.batch.publication.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.batch.publication.entity.minecart_initializer.declaring_class,
            (*b).acquisition.batch.publication.entity.minecart_initializer.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.batch.publication.entity.minecart_initializer.method,
                       (*b).acquisition.batch.publication.entity.minecart_initializer.method))
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_initializer.bytecode !=
        (*b).acquisition.batch.publication.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_initializer_cause.selected !=
        (*b).acquisition.batch.publication.entity.minecart_initializer_cause.selected)
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_initializer_cause.entered !=
        (*b).acquisition.batch.publication.entity.minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.batch.publication.entity.minecart_initializer_cause.declaring_class,
            (*b).acquisition.batch.publication.entity.minecart_initializer_cause.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.batch.publication.entity.minecart_initializer_cause.method,
                       (*b).acquisition.batch.publication.entity.minecart_initializer_cause.method))
        return false;
    if ((*a).acquisition.batch.publication.entity.minecart_initializer_cause.bytecode !=
        (*b).acquisition.batch.publication.entity.minecart_initializer_cause.bytecode)
        return false;
    if ((*a).acquisition.batch.publication.sleep.bytecode !=
        (*b).acquisition.batch.publication.sleep.bytecode)
        return false;
    if ((*a).acquisition.batch.publication.sleep.player_bytecode !=
        (*b).acquisition.batch.publication.sleep.player_bytecode)
        return false;
    if ((*a).acquisition.batch.publication.sleep.index !=
        (*b).acquisition.batch.publication.sleep.index)
        return false;
    if ((*a).acquisition.batch.publication.world_access_failure.receiver !=
        (*b).acquisition.batch.publication.world_access_failure.receiver)
        return false;
    if ((*a).acquisition.batch.publication.world_access_failure.render_global !=
        (*b).acquisition.batch.publication.world_access_failure.render_global)
        return false;
    if ((*a).acquisition.batch.publication.world_access_failure.receipt !=
        (*b).acquisition.batch.publication.world_access_failure.receipt)
        return false;
    if ((*a).acquisition.batch.index != (*b).acquisition.batch.index)
        return false;
    if ((*a).acquisition.activation_bytecode != (*b).acquisition.activation_bytecode)
        return false;
    if ((*a).acquisition.activation_world_bytecode != (*b).acquisition.activation_world_bytecode)
        return false;
    if ((*a).acquisition.load_bytecode != (*b).acquisition.load_bytecode)
        return false;
    if ((*a).acquisition.load_builder_bytecode != (*b).acquisition.load_builder_bytecode)
        return false;
    if ((*a).acquisition.load_entity.index != (*b).acquisition.load_entity.index)
        return false;
    if ((*a).acquisition.load_entity.factory.status != (*b).acquisition.load_entity.factory.status)
        return false;
    if ((*a).acquisition.load_entity.factory.late_hook !=
        (*b).acquisition.load_entity.factory.late_hook)
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.factory.operation,
                       (*b).acquisition.load_entity.factory.operation))
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.factory.field,
                       (*b).acquisition.load_entity.factory.field))
        return false;
    if ((*a).acquisition.load_entity.factory.index != (*b).acquisition.load_entity.factory.index)
        return false;
    if ((*a).acquisition.load_entity.factory.wrapper_bytecode !=
        (*b).acquisition.load_entity.factory.wrapper_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.name_bytecode !=
        (*b).acquisition.load_entity.factory.name_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.factory_bytecode !=
        (*b).acquisition.load_entity.factory.factory_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.base_bytecode !=
        (*b).acquisition.load_entity.factory.base_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.item_bytecode !=
        (*b).acquisition.load_entity.factory.item_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.falling_bytecode !=
        (*b).acquisition.load_entity.factory.falling_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.stack_bytecode !=
        (*b).acquisition.load_entity.factory.stack_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.local_player_bytecode !=
        (*b).acquisition.load_entity.factory.local_player_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.player_bytecode !=
        (*b).acquisition.load_entity.factory.player_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.initializer_bytecode !=
        (*b).acquisition.load_entity.factory.initializer_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.registration_bytecode !=
        (*b).acquisition.load_entity.factory.registration_bytecode)
        return false;
    if ((*a).acquisition.load_entity.factory.status != MC_ENTITY_NBT_OK ||
        (*b).acquisition.load_entity.factory.status != MC_ENTITY_NBT_OK)
        return false;
    if ((*a).acquisition.load_entity.provenance.status !=
        (*b).acquisition.load_entity.provenance.status)
        return false;
    if ((*a).acquisition.load_entity.provenance.late_hook !=
        (*b).acquisition.load_entity.provenance.late_hook)
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.provenance.operation,
                       (*b).acquisition.load_entity.provenance.operation))
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.provenance.field,
                       (*b).acquisition.load_entity.provenance.field))
        return false;
    if ((*a).acquisition.load_entity.provenance.index !=
        (*b).acquisition.load_entity.provenance.index)
        return false;
    if ((*a).acquisition.load_entity.provenance.wrapper_bytecode !=
        (*b).acquisition.load_entity.provenance.wrapper_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.name_bytecode !=
        (*b).acquisition.load_entity.provenance.name_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.factory_bytecode !=
        (*b).acquisition.load_entity.provenance.factory_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.base_bytecode !=
        (*b).acquisition.load_entity.provenance.base_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.item_bytecode !=
        (*b).acquisition.load_entity.provenance.item_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.falling_bytecode !=
        (*b).acquisition.load_entity.provenance.falling_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.stack_bytecode !=
        (*b).acquisition.load_entity.provenance.stack_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.local_player_bytecode !=
        (*b).acquisition.load_entity.provenance.local_player_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.player_bytecode !=
        (*b).acquisition.load_entity.provenance.player_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.initializer_bytecode !=
        (*b).acquisition.load_entity.provenance.initializer_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.registration_bytecode !=
        (*b).acquisition.load_entity.provenance.registration_bytecode)
        return false;
    if ((*a).acquisition.load_entity.provenance.status != MC_ENTITY_NBT_OK ||
        (*b).acquisition.load_entity.provenance.status != MC_ENTITY_NBT_OK)
        return false;
    if ((*a).acquisition.load_entity.insertion.status !=
        (*b).acquisition.load_entity.insertion.status)
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.insertion.operation,
                       (*b).acquisition.load_entity.insertion.operation))
        return false;
    if ((*a).acquisition.load_entity.insertion.bytecode !=
        (*b).acquisition.load_entity.insertion.bytecode)
        return false;
    if ((*a).acquisition.load_entity.insertion.cause.status !=
        (*b).acquisition.load_entity.insertion.cause.status)
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.insertion.cause.operation,
                       (*b).acquisition.load_entity.insertion.cause.operation))
        return false;
    if ((*a).acquisition.load_entity.insertion.cause.system_error !=
        (*b).acquisition.load_entity.insertion.cause.system_error)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.status !=
        (*b).acquisition.load_entity.insertion.entity.status)
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.insertion.entity.operation,
                       (*b).acquisition.load_entity.insertion.entity.operation))
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.bytecode !=
        (*b).acquisition.load_entity.insertion.entity.bytecode)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.profile !=
        (*b).acquisition.load_entity.insertion.entity.profile)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.item !=
        (*b).acquisition.load_entity.insertion.entity.item)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.world !=
        (*b).acquisition.load_entity.insertion.entity.world)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_world_constructor.selected !=
        (*b).acquisition.load_entity.insertion.entity.minecart_world_constructor.selected)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_world_constructor.entered !=
        (*b).acquisition.load_entity.insertion.entity.minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.insertion.entity.minecart_world_constructor
                           .declaring_class,
                       (*b).acquisition.load_entity.insertion.entity.minecart_world_constructor
                           .declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.load_entity.insertion.entity.minecart_world_constructor.method,
            (*b).acquisition.load_entity.insertion.entity.minecart_world_constructor.method))
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_world_constructor.bytecode !=
        (*b).acquisition.load_entity.insertion.entity.minecart_world_constructor.bytecode)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_constructor.selected !=
        (*b).acquisition.load_entity.insertion.entity.minecart_constructor.selected)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_constructor.entered !=
        (*b).acquisition.load_entity.insertion.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.load_entity.insertion.entity.minecart_constructor.declaring_class,
            (*b).acquisition.load_entity.insertion.entity.minecart_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.insertion.entity.minecart_constructor.method,
                       (*b).acquisition.load_entity.insertion.entity.minecart_constructor.method))
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_constructor.bytecode !=
        (*b).acquisition.load_entity.insertion.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_initializer.selected !=
        (*b).acquisition.load_entity.insertion.entity.minecart_initializer.selected)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_initializer.entered !=
        (*b).acquisition.load_entity.insertion.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal(
            (*a).acquisition.load_entity.insertion.entity.minecart_initializer.declaring_class,
            (*b).acquisition.load_entity.insertion.entity.minecart_initializer.declaring_class))
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.insertion.entity.minecart_initializer.method,
                       (*b).acquisition.load_entity.insertion.entity.minecart_initializer.method))
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_initializer.bytecode !=
        (*b).acquisition.load_entity.insertion.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_initializer_cause.selected !=
        (*b).acquisition.load_entity.insertion.entity.minecart_initializer_cause.selected)
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_initializer_cause.entered !=
        (*b).acquisition.load_entity.insertion.entity.minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.insertion.entity.minecart_initializer_cause
                           .declaring_class,
                       (*b).acquisition.load_entity.insertion.entity.minecart_initializer_cause
                           .declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).acquisition.load_entity.insertion.entity.minecart_initializer_cause.method,
            (*b).acquisition.load_entity.insertion.entity.minecart_initializer_cause.method))
        return false;
    if ((*a).acquisition.load_entity.insertion.entity.minecart_initializer_cause.bytecode !=
        (*b).acquisition.load_entity.insertion.entity.minecart_initializer_cause.bytecode)
        return false;
    if ((*a).acquisition.load_entity.native.status != (*b).acquisition.load_entity.native.status)
        return false;
    if (!ew_text_equal((*a).acquisition.load_entity.native.operation,
                       (*b).acquisition.load_entity.native.operation))
        return false;
    if ((*a).acquisition.load_entity.native.system_error !=
        (*b).acquisition.load_entity.native.system_error)
        return false;
    if ((*a).acquisition.population.reached != (*b).acquisition.population.reached)
        return false;
    if (!ew_text_equal((*a).acquisition.population.current.provider_class,
                       (*b).acquisition.population.current.provider_class))
        return false;
    if (!ew_text_equal((*a).acquisition.population.current.method,
                       (*b).acquisition.population.current.method))
        return false;
    if ((*a).acquisition.population.current.bytecode !=
        (*b).acquisition.population.current.bytecode)
        return false;
    if ((*a).acquisition.population.current.world_bytecode !=
        (*b).acquisition.population.current.world_bytecode)
        return false;
    if (!ew_text_equal((*a).acquisition.population.current.selector_class,
                       (*b).acquisition.population.current.selector_class))
        return false;
    if (!ew_text_equal((*a).acquisition.population.current.selector_method,
                       (*b).acquisition.population.current.selector_method))
        return false;
    if ((*a).acquisition.population.current.selector_bytecode !=
        (*b).acquisition.population.current.selector_bytecode)
        return false;
    if ((*a).acquisition.population.current.iteration !=
        (*b).acquisition.population.current.iteration)
        return false;
    if ((*a).acquisition.population.current.native_boundary !=
        (*b).acquisition.population.current.native_boundary)
        return false;
    if (!ew_text_equal((*a).acquisition.population.origin.provider_class,
                       (*b).acquisition.population.origin.provider_class))
        return false;
    if (!ew_text_equal((*a).acquisition.population.origin.method,
                       (*b).acquisition.population.origin.method))
        return false;
    if ((*a).acquisition.population.origin.bytecode != (*b).acquisition.population.origin.bytecode)
        return false;
    if ((*a).acquisition.population.origin.world_bytecode !=
        (*b).acquisition.population.origin.world_bytecode)
        return false;
    if (!ew_text_equal((*a).acquisition.population.origin.selector_class,
                       (*b).acquisition.population.origin.selector_class))
        return false;
    if (!ew_text_equal((*a).acquisition.population.origin.selector_method,
                       (*b).acquisition.population.origin.selector_method))
        return false;
    if ((*a).acquisition.population.origin.selector_bytecode !=
        (*b).acquisition.population.origin.selector_bytecode)
        return false;
    if ((*a).acquisition.population.origin.iteration !=
        (*b).acquisition.population.origin.iteration)
        return false;
    if ((*a).acquisition.population.origin.native_boundary !=
        (*b).acquisition.population.origin.native_boundary)
        return false;
    if ((*a).acquisition.population.cause.status != (*b).acquisition.population.cause.status)
        return false;
    if (!ew_text_equal((*a).acquisition.population.cause.operation,
                       (*b).acquisition.population.cause.operation))
        return false;
    if ((*a).acquisition.population.cause.system_error !=
        (*b).acquisition.population.cause.system_error)
        return false;
    if ((*a).acquisition.population.registry.status != (*b).acquisition.population.registry.status)
        return false;
    if (!ew_text_equal((*a).acquisition.population.registry.message,
                       (*b).acquisition.population.registry.message))
        return false;
    if ((*a).acquisition.population.registry.initializer_pc !=
        (*b).acquisition.population.registry.initializer_pc)
        return false;
    if ((*a).acquisition.population.registry.block_id !=
        (*b).acquisition.population.registry.block_id)
        return false;
    if (!ew_text_equal((*a).acquisition.population.registry.constructor_class,
                       (*b).acquisition.population.registry.constructor_class))
        return false;
    if ((*a).acquisition.population.registry.constructor_pc !=
        (*b).acquisition.population.registry.constructor_pc)
        return false;
    if ((*a).acquisition.population.registry.profile_status !=
        (*b).acquisition.population.registry.profile_status)
        return false;
    if (!ew_text_equal((*a).acquisition.population.registry.active_initializer_class,
                       (*b).acquisition.population.registry.active_initializer_class))
        return false;
    if ((*a).acquisition.population.registry.active_initializer_pc !=
        (*b).acquisition.population.registry.active_initializer_pc)
        return false;
    if ((*a).acquisition.population.registry.item_argument !=
        (*b).acquisition.population.registry.item_argument)
        return false;
    if ((*a).acquisition.population.registry.format_error.status !=
        (*b).acquisition.population.registry.format_error.status)
        return false;
    if ((*a).acquisition.population.registry.format_error.offset !=
        (*b).acquisition.population.registry.format_error.offset)
        return false;
    if ((*a).acquisition.population.registry.format_error.length !=
        (*b).acquisition.population.registry.format_error.length)
        return false;
    if ((*a).acquisition.population.registry.format_error.value !=
        (*b).acquisition.population.registry.format_error.value)
        return false;
    if ((*a).acquisition.population.registry.format_error.flags !=
        (*b).acquisition.population.registry.format_error.flags)
        return false;
    if ((*a).acquisition.population.registry.format_error.conversion !=
        (*b).acquisition.population.registry.format_error.conversion)
        return false;
    if ((*a).acquisition.population.registry.format_error.date_time !=
        (*b).acquisition.population.registry.format_error.date_time)
        return false;
    if ((*a).acquisition.source_chunk != (*b).acquisition.source_chunk)
        return false;
    if ((*a).registry.status != (*b).registry.status)
        return false;
    if (!ew_text_equal((*a).registry.message, (*b).registry.message))
        return false;
    if ((*a).registry.initializer_pc != (*b).registry.initializer_pc)
        return false;
    if ((*a).registry.block_id != (*b).registry.block_id)
        return false;
    if (!ew_text_equal((*a).registry.constructor_class, (*b).registry.constructor_class))
        return false;
    if ((*a).registry.constructor_pc != (*b).registry.constructor_pc)
        return false;
    if ((*a).registry.profile_status != (*b).registry.profile_status)
        return false;
    if (!ew_text_equal((*a).registry.active_initializer_class,
                       (*b).registry.active_initializer_class))
        return false;
    if ((*a).registry.active_initializer_pc != (*b).registry.active_initializer_pc)
        return false;
    if ((*a).registry.item_argument != (*b).registry.item_argument)
        return false;
    if ((*a).registry.format_error.status != (*b).registry.format_error.status)
        return false;
    if ((*a).registry.format_error.offset != (*b).registry.format_error.offset)
        return false;
    if ((*a).registry.format_error.length != (*b).registry.format_error.length)
        return false;
    if ((*a).registry.format_error.value != (*b).registry.format_error.value)
        return false;
    if ((*a).registry.format_error.flags != (*b).registry.format_error.flags)
        return false;
    if ((*a).registry.format_error.conversion != (*b).registry.format_error.conversion)
        return false;
    if ((*a).registry.format_error.date_time != (*b).registry.format_error.date_time)
        return false;
    if ((*a).tile_entity.status != (*b).tile_entity.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.operation, (*b).tile_entity.operation))
        return false;
    if ((*a).tile_entity.bytecode != (*b).tile_entity.bytecode)
        return false;
    if ((*a).tile_entity.chunk_bytecode != (*b).tile_entity.chunk_bytecode)
        return false;
    if ((*a).tile_entity.world_access_bytecode != (*b).tile_entity.world_access_bytecode)
        return false;
    if ((*a).tile_entity.initializer_bytecode != (*b).tile_entity.initializer_bytecode)
        return false;
    if ((*a).tile_entity.cause.status != (*b).tile_entity.cause.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.cause.operation, (*b).tile_entity.cause.operation))
        return false;
    if ((*a).tile_entity.cause.system_error != (*b).tile_entity.cause.system_error)
        return false;
    if ((*a).tile_entity.registry.status != (*b).tile_entity.registry.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.registry.message, (*b).tile_entity.registry.message))
        return false;
    if ((*a).tile_entity.registry.initializer_pc != (*b).tile_entity.registry.initializer_pc)
        return false;
    if ((*a).tile_entity.registry.block_id != (*b).tile_entity.registry.block_id)
        return false;
    if (!ew_text_equal((*a).tile_entity.registry.constructor_class,
                       (*b).tile_entity.registry.constructor_class))
        return false;
    if ((*a).tile_entity.registry.constructor_pc != (*b).tile_entity.registry.constructor_pc)
        return false;
    if ((*a).tile_entity.registry.profile_status != (*b).tile_entity.registry.profile_status)
        return false;
    if (!ew_text_equal((*a).tile_entity.registry.active_initializer_class,
                       (*b).tile_entity.registry.active_initializer_class))
        return false;
    if ((*a).tile_entity.registry.active_initializer_pc !=
        (*b).tile_entity.registry.active_initializer_pc)
        return false;
    if ((*a).tile_entity.registry.item_argument != (*b).tile_entity.registry.item_argument)
        return false;
    if ((*a).tile_entity.registry.format_error.status !=
        (*b).tile_entity.registry.format_error.status)
        return false;
    if ((*a).tile_entity.registry.format_error.offset !=
        (*b).tile_entity.registry.format_error.offset)
        return false;
    if ((*a).tile_entity.registry.format_error.length !=
        (*b).tile_entity.registry.format_error.length)
        return false;
    if ((*a).tile_entity.registry.format_error.value !=
        (*b).tile_entity.registry.format_error.value)
        return false;
    if ((*a).tile_entity.registry.format_error.flags !=
        (*b).tile_entity.registry.format_error.flags)
        return false;
    if ((*a).tile_entity.registry.format_error.conversion !=
        (*b).tile_entity.registry.format_error.conversion)
        return false;
    if ((*a).tile_entity.registry.format_error.date_time !=
        (*b).tile_entity.registry.format_error.date_time)
        return false;
    if ((*a).tile_entity.tile_entity.status != (*b).tile_entity.tile_entity.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.tile_entity.field, (*b).tile_entity.tile_entity.field))
        return false;
    if ((*a).tile_entity.tile_entity.item_index != (*b).tile_entity.tile_entity.item_index)
        return false;
    if ((*a).tile_entity.tile_entity.item.status != (*b).tile_entity.tile_entity.item.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.tile_entity.item.field,
                       (*b).tile_entity.tile_entity.item.field))
        return false;
    if ((*a).tile_entity.tile_entity.item.nbt.status !=
        (*b).tile_entity.tile_entity.item.nbt.status)
        return false;
    if ((*a).tile_entity.tile_entity.item.nbt.offset !=
        (*b).tile_entity.tile_entity.item.nbt.offset)
        return false;
    if ((*a).tile_entity.tile_entity.nbt.status != (*b).tile_entity.tile_entity.nbt.status)
        return false;
    if ((*a).tile_entity.tile_entity.nbt.offset != (*b).tile_entity.tile_entity.nbt.offset)
        return false;
    if ((*a).tile_entity.tile_entity.source_bytecode !=
        (*b).tile_entity.tile_entity.source_bytecode)
        return false;
    if ((*a).tile_entity.tile_entity.base_read_bytecode !=
        (*b).tile_entity.tile_entity.base_read_bytecode)
        return false;
    if ((*a).tile_entity.tile_entity.read_bytecode != (*b).tile_entity.tile_entity.read_bytecode)
        return false;
    if (!ew_text_equal((*a).tile_entity.tile_entity.operation,
                       (*b).tile_entity.tile_entity.operation))
        return false;
    if ((*a).tile_entity.tile_entity.getter_bytecode !=
        (*b).tile_entity.tile_entity.getter_bytecode)
        return false;
    if ((*a).tile_entity.tile_entity.base_save_bytecode !=
        (*b).tile_entity.tile_entity.base_save_bytecode)
        return false;
    if ((*a).tile_entity.tile_entity.save_bytecode != (*b).tile_entity.tile_entity.save_bytecode)
        return false;
    if ((*a).tile_entity.tile_entity.item_save_bytecode !=
        (*b).tile_entity.tile_entity.item_save_bytecode)
        return false;
    if ((*a).tile_entity.tile_entity.registry.status !=
        (*b).tile_entity.tile_entity.registry.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.tile_entity.registry.message,
                       (*b).tile_entity.tile_entity.registry.message))
        return false;
    if ((*a).tile_entity.tile_entity.registry.initializer_pc !=
        (*b).tile_entity.tile_entity.registry.initializer_pc)
        return false;
    if ((*a).tile_entity.tile_entity.registry.block_id !=
        (*b).tile_entity.tile_entity.registry.block_id)
        return false;
    if (!ew_text_equal((*a).tile_entity.tile_entity.registry.constructor_class,
                       (*b).tile_entity.tile_entity.registry.constructor_class))
        return false;
    if ((*a).tile_entity.tile_entity.registry.constructor_pc !=
        (*b).tile_entity.tile_entity.registry.constructor_pc)
        return false;
    if ((*a).tile_entity.tile_entity.registry.profile_status !=
        (*b).tile_entity.tile_entity.registry.profile_status)
        return false;
    if (!ew_text_equal((*a).tile_entity.tile_entity.registry.active_initializer_class,
                       (*b).tile_entity.tile_entity.registry.active_initializer_class))
        return false;
    if ((*a).tile_entity.tile_entity.registry.active_initializer_pc !=
        (*b).tile_entity.tile_entity.registry.active_initializer_pc)
        return false;
    if ((*a).tile_entity.tile_entity.registry.item_argument !=
        (*b).tile_entity.tile_entity.registry.item_argument)
        return false;
    if ((*a).tile_entity.tile_entity.registry.format_error.status !=
        (*b).tile_entity.tile_entity.registry.format_error.status)
        return false;
    if ((*a).tile_entity.tile_entity.registry.format_error.offset !=
        (*b).tile_entity.tile_entity.registry.format_error.offset)
        return false;
    if ((*a).tile_entity.tile_entity.registry.format_error.length !=
        (*b).tile_entity.tile_entity.registry.format_error.length)
        return false;
    if ((*a).tile_entity.tile_entity.registry.format_error.value !=
        (*b).tile_entity.tile_entity.registry.format_error.value)
        return false;
    if ((*a).tile_entity.tile_entity.registry.format_error.flags !=
        (*b).tile_entity.tile_entity.registry.format_error.flags)
        return false;
    if ((*a).tile_entity.tile_entity.registry.format_error.conversion !=
        (*b).tile_entity.tile_entity.registry.format_error.conversion)
        return false;
    if ((*a).tile_entity.tile_entity.registry.format_error.date_time !=
        (*b).tile_entity.tile_entity.registry.format_error.date_time)
        return false;
    if ((*a).tile_entity.tile_entity.constructor_bytecode !=
        (*b).tile_entity.tile_entity.constructor_bytecode)
        return false;
    if ((*a).tile_entity.tile_entity.initializer_bytecode !=
        (*b).tile_entity.tile_entity.initializer_bytecode)
        return false;
    if ((*a).tile_entity.tile_entity.piston_initializer_bytecode !=
        (*b).tile_entity.tile_entity.piston_initializer_bytecode)
        return false;
    if ((*a).tile_entity.entity.status != (*b).tile_entity.entity.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.entity.operation, (*b).tile_entity.entity.operation))
        return false;
    if ((*a).tile_entity.entity.bytecode != (*b).tile_entity.entity.bytecode)
        return false;
    if ((*a).tile_entity.entity.profile != (*b).tile_entity.entity.profile)
        return false;
    if ((*a).tile_entity.entity.item != (*b).tile_entity.entity.item)
        return false;
    if ((*a).tile_entity.entity.world != (*b).tile_entity.entity.world)
        return false;
    if ((*a).tile_entity.entity.minecart_world_constructor.selected !=
        (*b).tile_entity.entity.minecart_world_constructor.selected)
        return false;
    if ((*a).tile_entity.entity.minecart_world_constructor.entered !=
        (*b).tile_entity.entity.minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal((*a).tile_entity.entity.minecart_world_constructor.declaring_class,
                       (*b).tile_entity.entity.minecart_world_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.entity.minecart_world_constructor.method,
                       (*b).tile_entity.entity.minecart_world_constructor.method))
        return false;
    if ((*a).tile_entity.entity.minecart_world_constructor.bytecode !=
        (*b).tile_entity.entity.minecart_world_constructor.bytecode)
        return false;
    if ((*a).tile_entity.entity.minecart_constructor.selected !=
        (*b).tile_entity.entity.minecart_constructor.selected)
        return false;
    if ((*a).tile_entity.entity.minecart_constructor.entered !=
        (*b).tile_entity.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal((*a).tile_entity.entity.minecart_constructor.declaring_class,
                       (*b).tile_entity.entity.minecart_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.entity.minecart_constructor.method,
                       (*b).tile_entity.entity.minecart_constructor.method))
        return false;
    if ((*a).tile_entity.entity.minecart_constructor.bytecode !=
        (*b).tile_entity.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).tile_entity.entity.minecart_initializer.selected !=
        (*b).tile_entity.entity.minecart_initializer.selected)
        return false;
    if ((*a).tile_entity.entity.minecart_initializer.entered !=
        (*b).tile_entity.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal((*a).tile_entity.entity.minecart_initializer.declaring_class,
                       (*b).tile_entity.entity.minecart_initializer.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.entity.minecart_initializer.method,
                       (*b).tile_entity.entity.minecart_initializer.method))
        return false;
    if ((*a).tile_entity.entity.minecart_initializer.bytecode !=
        (*b).tile_entity.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).tile_entity.entity.minecart_initializer_cause.selected !=
        (*b).tile_entity.entity.minecart_initializer_cause.selected)
        return false;
    if ((*a).tile_entity.entity.minecart_initializer_cause.entered !=
        (*b).tile_entity.entity.minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal((*a).tile_entity.entity.minecart_initializer_cause.declaring_class,
                       (*b).tile_entity.entity.minecart_initializer_cause.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.entity.minecart_initializer_cause.method,
                       (*b).tile_entity.entity.minecart_initializer_cause.method))
        return false;
    if ((*a).tile_entity.entity.minecart_initializer_cause.bytecode !=
        (*b).tile_entity.entity.minecart_initializer_cause.bytecode)
        return false;
    if ((*a).tile_entity.publication.status != (*b).tile_entity.publication.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.publication.operation,
                       (*b).tile_entity.publication.operation))
        return false;
    if ((*a).tile_entity.publication.bytecode != (*b).tile_entity.publication.bytecode)
        return false;
    if ((*a).tile_entity.publication.presence_bytecode !=
        (*b).tile_entity.publication.presence_bytecode)
        return false;
    if ((*a).tile_entity.publication.provider_bytecode !=
        (*b).tile_entity.publication.provider_bytecode)
        return false;
    if ((*a).tile_entity.publication.chunk_bytecode != (*b).tile_entity.publication.chunk_bytecode)
        return false;
    if ((*a).tile_entity.publication.world_access_bytecode !=
        (*b).tile_entity.publication.world_access_bytecode)
        return false;
    if ((*a).tile_entity.publication.dedicated_bytecode !=
        (*b).tile_entity.publication.dedicated_bytecode)
        return false;
    if ((*a).tile_entity.publication.index_bytecode != (*b).tile_entity.publication.index_bytecode)
        return false;
    if ((*a).tile_entity.publication.acquisition != (*b).tile_entity.publication.acquisition)
        return false;
    if ((*a).tile_entity.publication.cause.status != (*b).tile_entity.publication.cause.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.publication.cause.operation,
                       (*b).tile_entity.publication.cause.operation))
        return false;
    if ((*a).tile_entity.publication.cause.system_error !=
        (*b).tile_entity.publication.cause.system_error)
        return false;
    if ((*a).tile_entity.publication.entity.status != (*b).tile_entity.publication.entity.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.publication.entity.operation,
                       (*b).tile_entity.publication.entity.operation))
        return false;
    if ((*a).tile_entity.publication.entity.bytecode !=
        (*b).tile_entity.publication.entity.bytecode)
        return false;
    if ((*a).tile_entity.publication.entity.profile != (*b).tile_entity.publication.entity.profile)
        return false;
    if ((*a).tile_entity.publication.entity.item != (*b).tile_entity.publication.entity.item)
        return false;
    if ((*a).tile_entity.publication.entity.world != (*b).tile_entity.publication.entity.world)
        return false;
    if ((*a).tile_entity.publication.entity.minecart_world_constructor.selected !=
        (*b).tile_entity.publication.entity.minecart_world_constructor.selected)
        return false;
    if ((*a).tile_entity.publication.entity.minecart_world_constructor.entered !=
        (*b).tile_entity.publication.entity.minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.publication.entity.minecart_world_constructor.declaring_class,
            (*b).tile_entity.publication.entity.minecart_world_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.publication.entity.minecart_world_constructor.method,
                       (*b).tile_entity.publication.entity.minecart_world_constructor.method))
        return false;
    if ((*a).tile_entity.publication.entity.minecart_world_constructor.bytecode !=
        (*b).tile_entity.publication.entity.minecart_world_constructor.bytecode)
        return false;
    if ((*a).tile_entity.publication.entity.minecart_constructor.selected !=
        (*b).tile_entity.publication.entity.minecart_constructor.selected)
        return false;
    if ((*a).tile_entity.publication.entity.minecart_constructor.entered !=
        (*b).tile_entity.publication.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal((*a).tile_entity.publication.entity.minecart_constructor.declaring_class,
                       (*b).tile_entity.publication.entity.minecart_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.publication.entity.minecart_constructor.method,
                       (*b).tile_entity.publication.entity.minecart_constructor.method))
        return false;
    if ((*a).tile_entity.publication.entity.minecart_constructor.bytecode !=
        (*b).tile_entity.publication.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).tile_entity.publication.entity.minecart_initializer.selected !=
        (*b).tile_entity.publication.entity.minecart_initializer.selected)
        return false;
    if ((*a).tile_entity.publication.entity.minecart_initializer.entered !=
        (*b).tile_entity.publication.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal((*a).tile_entity.publication.entity.minecart_initializer.declaring_class,
                       (*b).tile_entity.publication.entity.minecart_initializer.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.publication.entity.minecart_initializer.method,
                       (*b).tile_entity.publication.entity.minecart_initializer.method))
        return false;
    if ((*a).tile_entity.publication.entity.minecart_initializer.bytecode !=
        (*b).tile_entity.publication.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).tile_entity.publication.entity.minecart_initializer_cause.selected !=
        (*b).tile_entity.publication.entity.minecart_initializer_cause.selected)
        return false;
    if ((*a).tile_entity.publication.entity.minecart_initializer_cause.entered !=
        (*b).tile_entity.publication.entity.minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.publication.entity.minecart_initializer_cause.declaring_class,
            (*b).tile_entity.publication.entity.minecart_initializer_cause.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.publication.entity.minecart_initializer_cause.method,
                       (*b).tile_entity.publication.entity.minecart_initializer_cause.method))
        return false;
    if ((*a).tile_entity.publication.entity.minecart_initializer_cause.bytecode !=
        (*b).tile_entity.publication.entity.minecart_initializer_cause.bytecode)
        return false;
    if ((*a).tile_entity.publication.sleep.bytecode != (*b).tile_entity.publication.sleep.bytecode)
        return false;
    if ((*a).tile_entity.publication.sleep.player_bytecode !=
        (*b).tile_entity.publication.sleep.player_bytecode)
        return false;
    if ((*a).tile_entity.publication.sleep.index != (*b).tile_entity.publication.sleep.index)
        return false;
    if ((*a).tile_entity.publication.world_access_failure.receiver !=
        (*b).tile_entity.publication.world_access_failure.receiver)
        return false;
    if ((*a).tile_entity.publication.world_access_failure.render_global !=
        (*b).tile_entity.publication.world_access_failure.render_global)
        return false;
    if ((*a).tile_entity.publication.world_access_failure.receipt !=
        (*b).tile_entity.publication.world_access_failure.receipt)
        return false;
    if ((*a).tile_entity.batch.publication.status != (*b).tile_entity.batch.publication.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.batch.publication.operation,
                       (*b).tile_entity.batch.publication.operation))
        return false;
    if ((*a).tile_entity.batch.publication.bytecode != (*b).tile_entity.batch.publication.bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.presence_bytecode !=
        (*b).tile_entity.batch.publication.presence_bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.provider_bytecode !=
        (*b).tile_entity.batch.publication.provider_bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.chunk_bytecode !=
        (*b).tile_entity.batch.publication.chunk_bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.world_access_bytecode !=
        (*b).tile_entity.batch.publication.world_access_bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.dedicated_bytecode !=
        (*b).tile_entity.batch.publication.dedicated_bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.index_bytecode !=
        (*b).tile_entity.batch.publication.index_bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.acquisition !=
        (*b).tile_entity.batch.publication.acquisition)
        return false;
    if ((*a).tile_entity.batch.publication.cause.status !=
        (*b).tile_entity.batch.publication.cause.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.batch.publication.cause.operation,
                       (*b).tile_entity.batch.publication.cause.operation))
        return false;
    if ((*a).tile_entity.batch.publication.cause.system_error !=
        (*b).tile_entity.batch.publication.cause.system_error)
        return false;
    if ((*a).tile_entity.batch.publication.entity.status !=
        (*b).tile_entity.batch.publication.entity.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.batch.publication.entity.operation,
                       (*b).tile_entity.batch.publication.entity.operation))
        return false;
    if ((*a).tile_entity.batch.publication.entity.bytecode !=
        (*b).tile_entity.batch.publication.entity.bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.entity.profile !=
        (*b).tile_entity.batch.publication.entity.profile)
        return false;
    if ((*a).tile_entity.batch.publication.entity.item !=
        (*b).tile_entity.batch.publication.entity.item)
        return false;
    if ((*a).tile_entity.batch.publication.entity.world !=
        (*b).tile_entity.batch.publication.entity.world)
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_world_constructor.selected !=
        (*b).tile_entity.batch.publication.entity.minecart_world_constructor.selected)
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_world_constructor.entered !=
        (*b).tile_entity.batch.publication.entity.minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.batch.publication.entity.minecart_world_constructor.declaring_class,
            (*b).tile_entity.batch.publication.entity.minecart_world_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.batch.publication.entity.minecart_world_constructor.method,
                       (*b).tile_entity.batch.publication.entity.minecart_world_constructor.method))
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_world_constructor.bytecode !=
        (*b).tile_entity.batch.publication.entity.minecart_world_constructor.bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_constructor.selected !=
        (*b).tile_entity.batch.publication.entity.minecart_constructor.selected)
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_constructor.entered !=
        (*b).tile_entity.batch.publication.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.batch.publication.entity.minecart_constructor.declaring_class,
            (*b).tile_entity.batch.publication.entity.minecart_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.batch.publication.entity.minecart_constructor.method,
                       (*b).tile_entity.batch.publication.entity.minecart_constructor.method))
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_constructor.bytecode !=
        (*b).tile_entity.batch.publication.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_initializer.selected !=
        (*b).tile_entity.batch.publication.entity.minecart_initializer.selected)
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_initializer.entered !=
        (*b).tile_entity.batch.publication.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.batch.publication.entity.minecart_initializer.declaring_class,
            (*b).tile_entity.batch.publication.entity.minecart_initializer.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.batch.publication.entity.minecart_initializer.method,
                       (*b).tile_entity.batch.publication.entity.minecart_initializer.method))
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_initializer.bytecode !=
        (*b).tile_entity.batch.publication.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_initializer_cause.selected !=
        (*b).tile_entity.batch.publication.entity.minecart_initializer_cause.selected)
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_initializer_cause.entered !=
        (*b).tile_entity.batch.publication.entity.minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.batch.publication.entity.minecart_initializer_cause.declaring_class,
            (*b).tile_entity.batch.publication.entity.minecart_initializer_cause.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.batch.publication.entity.minecart_initializer_cause.method,
                       (*b).tile_entity.batch.publication.entity.minecart_initializer_cause.method))
        return false;
    if ((*a).tile_entity.batch.publication.entity.minecart_initializer_cause.bytecode !=
        (*b).tile_entity.batch.publication.entity.minecart_initializer_cause.bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.sleep.bytecode !=
        (*b).tile_entity.batch.publication.sleep.bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.sleep.player_bytecode !=
        (*b).tile_entity.batch.publication.sleep.player_bytecode)
        return false;
    if ((*a).tile_entity.batch.publication.sleep.index !=
        (*b).tile_entity.batch.publication.sleep.index)
        return false;
    if ((*a).tile_entity.batch.publication.world_access_failure.receiver !=
        (*b).tile_entity.batch.publication.world_access_failure.receiver)
        return false;
    if ((*a).tile_entity.batch.publication.world_access_failure.render_global !=
        (*b).tile_entity.batch.publication.world_access_failure.render_global)
        return false;
    if ((*a).tile_entity.batch.publication.world_access_failure.receipt !=
        (*b).tile_entity.batch.publication.world_access_failure.receipt)
        return false;
    if ((*a).tile_entity.batch.index != (*b).tile_entity.batch.index)
        return false;
    if ((*a).tile_entity.caller_bytecode != (*b).tile_entity.caller_bytecode)
        return false;
    if ((*a).tile_entity.hook_bytecode != (*b).tile_entity.hook_bytecode)
        return false;
    if ((*a).tile_entity.remove_bytecode != (*b).tile_entity.remove_bytecode)
        return false;
    if ((*a).tile_entity.tile_entity_bytecode != (*b).tile_entity.tile_entity_bytecode)
        return false;
    if ((*a).tile_entity.inventory_bytecode != (*b).tile_entity.inventory_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_caller_bytecode !=
        (*b).tile_entity.acquisition_caller_bytecode)
        return false;
    if ((*a).tile_entity.activation_bytecode != (*b).tile_entity.activation_bytecode)
        return false;
    if ((*a).tile_entity.activation_world_bytecode != (*b).tile_entity.activation_world_bytecode)
        return false;
    if ((*a).tile_entity.load_bytecode != (*b).tile_entity.load_bytecode)
        return false;
    if ((*a).tile_entity.load_builder_bytecode != (*b).tile_entity.load_builder_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.index != (*b).tile_entity.load_entity.index)
        return false;
    if ((*a).tile_entity.load_entity.factory.status != (*b).tile_entity.load_entity.factory.status)
        return false;
    if ((*a).tile_entity.load_entity.factory.late_hook !=
        (*b).tile_entity.load_entity.factory.late_hook)
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.factory.operation,
                       (*b).tile_entity.load_entity.factory.operation))
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.factory.field,
                       (*b).tile_entity.load_entity.factory.field))
        return false;
    if ((*a).tile_entity.load_entity.factory.index != (*b).tile_entity.load_entity.factory.index)
        return false;
    if ((*a).tile_entity.load_entity.factory.wrapper_bytecode !=
        (*b).tile_entity.load_entity.factory.wrapper_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.name_bytecode !=
        (*b).tile_entity.load_entity.factory.name_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.factory_bytecode !=
        (*b).tile_entity.load_entity.factory.factory_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.base_bytecode !=
        (*b).tile_entity.load_entity.factory.base_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.item_bytecode !=
        (*b).tile_entity.load_entity.factory.item_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.falling_bytecode !=
        (*b).tile_entity.load_entity.factory.falling_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.stack_bytecode !=
        (*b).tile_entity.load_entity.factory.stack_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.local_player_bytecode !=
        (*b).tile_entity.load_entity.factory.local_player_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.player_bytecode !=
        (*b).tile_entity.load_entity.factory.player_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.initializer_bytecode !=
        (*b).tile_entity.load_entity.factory.initializer_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.registration_bytecode !=
        (*b).tile_entity.load_entity.factory.registration_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.factory.status != MC_ENTITY_NBT_OK ||
        (*b).tile_entity.load_entity.factory.status != MC_ENTITY_NBT_OK)
        return false;
    if ((*a).tile_entity.load_entity.provenance.status !=
        (*b).tile_entity.load_entity.provenance.status)
        return false;
    if ((*a).tile_entity.load_entity.provenance.late_hook !=
        (*b).tile_entity.load_entity.provenance.late_hook)
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.provenance.operation,
                       (*b).tile_entity.load_entity.provenance.operation))
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.provenance.field,
                       (*b).tile_entity.load_entity.provenance.field))
        return false;
    if ((*a).tile_entity.load_entity.provenance.index !=
        (*b).tile_entity.load_entity.provenance.index)
        return false;
    if ((*a).tile_entity.load_entity.provenance.wrapper_bytecode !=
        (*b).tile_entity.load_entity.provenance.wrapper_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.name_bytecode !=
        (*b).tile_entity.load_entity.provenance.name_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.factory_bytecode !=
        (*b).tile_entity.load_entity.provenance.factory_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.base_bytecode !=
        (*b).tile_entity.load_entity.provenance.base_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.item_bytecode !=
        (*b).tile_entity.load_entity.provenance.item_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.falling_bytecode !=
        (*b).tile_entity.load_entity.provenance.falling_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.stack_bytecode !=
        (*b).tile_entity.load_entity.provenance.stack_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.local_player_bytecode !=
        (*b).tile_entity.load_entity.provenance.local_player_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.player_bytecode !=
        (*b).tile_entity.load_entity.provenance.player_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.initializer_bytecode !=
        (*b).tile_entity.load_entity.provenance.initializer_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.registration_bytecode !=
        (*b).tile_entity.load_entity.provenance.registration_bytecode)
        return false;
    if ((*a).tile_entity.load_entity.provenance.status != MC_ENTITY_NBT_OK ||
        (*b).tile_entity.load_entity.provenance.status != MC_ENTITY_NBT_OK)
        return false;
    if ((*a).tile_entity.load_entity.insertion.status !=
        (*b).tile_entity.load_entity.insertion.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.insertion.operation,
                       (*b).tile_entity.load_entity.insertion.operation))
        return false;
    if ((*a).tile_entity.load_entity.insertion.bytecode !=
        (*b).tile_entity.load_entity.insertion.bytecode)
        return false;
    if ((*a).tile_entity.load_entity.insertion.cause.status !=
        (*b).tile_entity.load_entity.insertion.cause.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.insertion.cause.operation,
                       (*b).tile_entity.load_entity.insertion.cause.operation))
        return false;
    if ((*a).tile_entity.load_entity.insertion.cause.system_error !=
        (*b).tile_entity.load_entity.insertion.cause.system_error)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.status !=
        (*b).tile_entity.load_entity.insertion.entity.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.insertion.entity.operation,
                       (*b).tile_entity.load_entity.insertion.entity.operation))
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.bytecode !=
        (*b).tile_entity.load_entity.insertion.entity.bytecode)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.profile !=
        (*b).tile_entity.load_entity.insertion.entity.profile)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.item !=
        (*b).tile_entity.load_entity.insertion.entity.item)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.world !=
        (*b).tile_entity.load_entity.insertion.entity.world)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_world_constructor.selected !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_world_constructor.selected)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_world_constructor.entered !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_world_constructor.entered)
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.insertion.entity.minecart_world_constructor
                           .declaring_class,
                       (*b).tile_entity.load_entity.insertion.entity.minecart_world_constructor
                           .declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.load_entity.insertion.entity.minecart_world_constructor.method,
            (*b).tile_entity.load_entity.insertion.entity.minecart_world_constructor.method))
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_world_constructor.bytecode !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_world_constructor.bytecode)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_constructor.selected !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_constructor.selected)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_constructor.entered !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_constructor.entered)
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.load_entity.insertion.entity.minecart_constructor.declaring_class,
            (*b).tile_entity.load_entity.insertion.entity.minecart_constructor.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.insertion.entity.minecart_constructor.method,
                       (*b).tile_entity.load_entity.insertion.entity.minecart_constructor.method))
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_constructor.bytecode !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_constructor.bytecode)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_initializer.selected !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_initializer.selected)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_initializer.entered !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_initializer.entered)
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.load_entity.insertion.entity.minecart_initializer.declaring_class,
            (*b).tile_entity.load_entity.insertion.entity.minecart_initializer.declaring_class))
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.insertion.entity.minecart_initializer.method,
                       (*b).tile_entity.load_entity.insertion.entity.minecart_initializer.method))
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_initializer.bytecode !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_initializer.bytecode)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_initializer_cause.selected !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_initializer_cause.selected)
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_initializer_cause.entered !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_initializer_cause.entered)
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.insertion.entity.minecart_initializer_cause
                           .declaring_class,
                       (*b).tile_entity.load_entity.insertion.entity.minecart_initializer_cause
                           .declaring_class))
        return false;
    if (!ew_text_equal(
            (*a).tile_entity.load_entity.insertion.entity.minecart_initializer_cause.method,
            (*b).tile_entity.load_entity.insertion.entity.minecart_initializer_cause.method))
        return false;
    if ((*a).tile_entity.load_entity.insertion.entity.minecart_initializer_cause.bytecode !=
        (*b).tile_entity.load_entity.insertion.entity.minecart_initializer_cause.bytecode)
        return false;
    if ((*a).tile_entity.load_entity.native.status != (*b).tile_entity.load_entity.native.status)
        return false;
    if (!ew_text_equal((*a).tile_entity.load_entity.native.operation,
                       (*b).tile_entity.load_entity.native.operation))
        return false;
    if ((*a).tile_entity.load_entity.native.system_error !=
        (*b).tile_entity.load_entity.native.system_error)
        return false;
    if ((*a).tile_entity.provider_bytecode != (*b).tile_entity.provider_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.bytecode !=
        (*b).tile_entity.acquisition_parent.bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.chunk_bytecode !=
        (*b).tile_entity.acquisition_parent.chunk_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.world_access_bytecode !=
        (*b).tile_entity.acquisition_parent.world_access_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.initializer_bytecode !=
        (*b).tile_entity.acquisition_parent.initializer_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.caller_bytecode !=
        (*b).tile_entity.acquisition_parent.caller_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.hook_bytecode !=
        (*b).tile_entity.acquisition_parent.hook_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.remove_bytecode !=
        (*b).tile_entity.acquisition_parent.remove_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.provider_bytecode !=
        (*b).tile_entity.acquisition_parent.provider_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.tile_entity_bytecode !=
        (*b).tile_entity.acquisition_parent.tile_entity_bytecode)
        return false;
    if ((*a).tile_entity.acquisition_parent.inventory_bytecode !=
        (*b).tile_entity.acquisition_parent.inventory_bytecode)
        return false;
    if ((*a).tile_entity.world_access_failure.receiver !=
        (*b).tile_entity.world_access_failure.receiver)
        return false;
    if ((*a).tile_entity.world_access_failure.render_global !=
        (*b).tile_entity.world_access_failure.render_global)
        return false;
    if ((*a).tile_entity.world_access_failure.receipt !=
        (*b).tile_entity.world_access_failure.receipt)
        return false;
    return true;
}
