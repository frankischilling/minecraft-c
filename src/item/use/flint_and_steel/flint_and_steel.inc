static bool flint_floating(mc_item_use_call *c, unsigned int pc) {
    at(c, "ye", use_block_descriptor, pc);
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_ITEM_USE_FLOATING, "Flint and Steel reached floating environment", true);
}

static double flint_center(int32_t coordinate) {
    return _mm_cvtsd_f64(_mm_add_sd(_mm_cvtsi32_sd(_mm_setzero_pd(), coordinate), _mm_set_sd(0.5)));
}

/* Actual YE.a: the occupied branch still wears the Stack and returns true.
 * Sound precedes the live Fire static read; the ordinary setter Boolean is
 * independent of native completion. No remote, face-validity or count gate. */
static bool flint_and_steel(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    if (s->face == 0)
        s->target_y = mc_java_i32_sub(s->target_y, 1);
    if (s->face == 1)
        s->target_y = mc_java_i32_add(s->target_y, 1);
    if (s->face == 2)
        s->target_z = mc_java_i32_sub(s->target_z, 1);
    if (s->face == 3)
        s->target_z = mc_java_i32_add(s->target_z, 1);
    if (s->face == 4)
        s->target_x = mc_java_i32_sub(s->target_x, 1);
    if (s->face == 5)
        s->target_x = mc_java_i32_add(s->target_x, 1);
    int32_t id;
    if (!query_xyz_at(c, "ye", 60, s->target_x, s->target_y, s->target_z, &id))
        return false;
    if (id == 0) {
        if (!flint_floating(c, 77))
            return false;
        s->sound_x = flint_center(s->target_x);
        if (!flint_floating(c, 84))
            return false;
        s->sound_y = flint_center(s->target_y);
        if (!flint_floating(c, 91))
            return false;
        s->sound_z = flint_center(s->target_z);
        const mc_registry_string *name = NULL;
        at(c, "ye", use_block_descriptor, 92);
        if (mc_registry_runtime_literal_ascii(s->registry, "fire.ignite", &name,
                                              &c->error.registry) != MC_REGISTRY_OK)
            return registry_failed(c);
        s->volume = 1.0f;
        mc_java_random *random = NULL;
        at(c, "ye", use_block_descriptor, 95);
        if (mc_registry_item_b_get(s->registry, &random, &c->error.registry) != MC_REGISTRY_OK)
            return registry_failed(c);
        at(c, "ye", use_block_descriptor, 98);
        if (random == NULL)
            return source_null(c, "Flint and Steel actual GM.b Random invocation");
        const float draw = mc_java_random_next_float(random);
        if (!flint_floating(c, 103))
            return false;
        s->pitch = multiply(draw, 0.4f);
        if (!flint_floating(c, 106))
            return false;
        s->pitch = add(s->pitch, 0.8f);
        at(c, "ye", use_block_descriptor, 107);
        c->sound = calloc(1, sizeof(*c->sound));
        if (c->sound == NULL)
            return fail(c, MC_ITEM_USE_ALLOCATION, "Flint and Steel reached sound diagnostic",
                        true);
        c->error.sound = c->sound;
        const mc_world_storage_status status = mc_world_runtime_sound(
            s->world, s->sound_x, s->sound_y, s->sound_z, name, s->volume, s->pitch, c->sound);
        if (status != MC_WORLD_STORAGE_OK)
            world_failed(c, c->sound, "a", "(DDDLjava/lang/String;FF)V");
        else
            s->sound_returned = true;
        if (!captured(c) || status != MC_WORLD_STORAGE_OK)
            return false;
        at(c, "ye", use_block_descriptor, 117);
        if (mc_registry_block_static_get(s->registry, "as", &s->captured_block,
                                         &c->error.registry) != MC_REGISTRY_OK)
            return registry_failed(c);
        at(c, "ye", use_block_descriptor, 120);
        if (s->captured_block == NULL)
            return source_null(c, "Flint and Steel live Fire bn receiver");
        s->block_id = s->captured_block->fields.bn;
        if (!farming_set(c, "ye", 123, s->block_id))
            return false;
    }
    if (!durability(c, "ye", 130))
        return false;
    at(c, "ye", use_block_descriptor, 134);
    s->result = true;
    return true;
}
