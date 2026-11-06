/* Authored native ownership/failure inputs, separate from original rows. */
static bool coordinate_methods_cases(fixture *f, client_application *app, mc_entity *p) {
    bool ok = false;
    mc_player_coordinate_pin *first = NULL, *second = NULL, *owned = NULL;
    mc_entity *raw = NULL;
    mc_player_error error = {0};
    mc_player_fields *fields = &p->local_player->fields.player;
    const mc_player_coordinates *saved = fields->private_b;
    const size_t holds = p->holds, bytes = mc_entity_native_bytes(p);
    CHECK(saved != NULL && mc_player_saved_spawn_get(p, &first, &error) == MC_PLAYER_OK &&
          mc_player_coordinate_pin_value(first) == saved && p->holds == holds + 1 &&
          mc_player_saved_spawn_get(p, &second, &error) == MC_PLAYER_OK &&
          mc_player_coordinate_pin_value(second) == saved && p->holds == holds + 2);
    CHECK(mc_player_saved_spawn_get(p, &first, &error) == MC_PLAYER_INVALID_ARGUMENT &&
          mc_player_coordinate_pin_value(first) == saved && p->holds == holds + 2);
    /* This actual NBT reader publishes a replacement and runs the collector.
     * Both source-local occurrences must still point at the old same object. */
    CHECK(nbt_spawn(p, app->registry, 10, 20, 30) && fields->private_b != saved);
    mc_player_nbt_coordinates_collect_held(p);
    CHECK(mc_player_coordinate_pin_value(first) == saved && saved->x == 7 && saved->y == 8 &&
          saved->z == 9 && mc_player_coordinate_pin_value(second) == saved);
    CHECK(mc_player_saved_spawn_set(p, saved, &error) == MC_PLAYER_OK &&
          fields->private_b != saved && fields->private_b->x == 7 && fields->private_b->y == 8 &&
          fields->private_b->z == 9);
    CHECK(mc_player_saved_spawn_get(p, &owned, &error) == MC_PLAYER_OK);
    const mc_player_coordinates *copy = mc_player_coordinate_pin_value(owned);
    CHECK(copy == fields->private_b &&
          mc_player_saved_spawn_set(p, fields->private_b, &error) == MC_PLAYER_OK &&
          fields->private_b != copy && copy->x == 7 && copy->y == 8 && copy->z == 9 &&
          fields->private_b->x == 7 && fields->private_b->y == 8 && fields->private_b->z == 9);
    mc_player_coordinate_pin_release(&owned);
    copy = fields->private_b;
    fields->private_bN = copy;
    mc_player_coordinates input = {INT32_MIN, 16777217, INT32_MAX};
    CHECK(mc_player_saved_spawn_set(p, &input, &error) == MC_PLAYER_OK &&
          fields->private_b != &input && fields->private_b != copy && fields->private_bN == copy &&
          copy->x == 7 && copy->y == 8 && copy->z == 9);
    input = (mc_player_coordinates){0};
    CHECK(fields->private_b->x == INT32_MIN && fields->private_b->y == 16777217 &&
          fields->private_b->z == INT32_MAX);
    fields->private_bN = NULL;
    mc_player_nbt_coordinates_collect_held(p);
    const size_t limit = p->classes->limits.max_allocation_bytes;
    p->classes->limits.max_allocation_bytes = p->classes->allocated_bytes;
    const mc_player_coordinates *before = fields->private_b;
    const size_t before_limit = mc_entity_native_bytes(p);
    const mc_player_status set_limit = mc_player_saved_spawn_set(p, saved, &error);
    p->classes->limits.max_allocation_bytes = limit;
    CHECK(set_limit == MC_PLAYER_LIMIT && error.site.pc == 5 && fields->private_b == before &&
          mc_entity_native_bytes(p) == before_limit && p->holds == holds + 2);
    p->classes->limits.max_allocation_bytes = p->classes->allocated_bytes;
    const mc_player_status get_limit = mc_player_saved_spawn_get(p, &owned, &error);
    p->classes->limits.max_allocation_bytes = limit;
    CHECK(get_limit == MC_PLAYER_LIMIT && error.site.pc == 1 && owned == NULL &&
          fields->private_b == before && mc_entity_native_bytes(p) == before_limit &&
          p->holds == holds + 2);
    struct mc_player_nbt_coordinate *node = p->local_player->nbt_coordinates;
    CHECK(node != NULL && &node->value == before && node->pins == 0);
    node->pins = SIZE_MAX;
    const mc_player_status pin_limit = mc_player_saved_spawn_get(p, &owned, &error);
    node->pins = 0;
    CHECK(pin_limit == MC_PLAYER_LIMIT && error.site.pc == 1 && owned == NULL &&
          p->holds == holds + 2 && mc_entity_native_bytes(p) == before_limit);
    const size_t before_holds = p->holds;
    p->holds = SIZE_MAX;
    const mc_player_status hold_limit = mc_player_saved_spawn_get(p, &owned, &error);
    p->holds = before_holds;
    CHECK(hold_limit == MC_PLAYER_LIMIT && owned == NULL && fields->private_b == before);
    fields->private_b = (const mc_player_coordinates *)(uintptr_t)1;
    const mc_player_status foreign = mc_player_saved_spawn_get(p, &owned, &error);
    fields->private_b = before;
    CHECK(foreign == MC_PLAYER_INVALID_ARGUMENT && owned == NULL && error.site.pc == 1 &&
          p->holds == holds + 2 && mc_entity_native_bytes(p) == before_limit);
    mc_player_coordinate_pin_release(&first);
    CHECK(first == NULL && p->holds == holds + 1 &&
          mc_player_coordinate_pin_value(second) == saved && saved->x == 7 && saved->y == 8 &&
          saved->z == 9);
    mc_player_coordinate_pin_release(&second);
    CHECK(second == NULL && p->holds == holds && mc_entity_native_bytes(p) == bytes);
    CHECK(mc_player_saved_spawn_set(p, NULL, &error) == MC_PLAYER_OK && fields->private_b == NULL &&
          mc_entity_native_bytes(p) == bytes - sizeof(struct mc_player_nbt_coordinate));
    /* Source NULL needs no native pin/allocation/hold, even at both ceilings. */
    p->classes->limits.max_allocation_bytes = 0;
    p->holds = SIZE_MAX;
    const mc_player_status null_result = mc_player_saved_spawn_get(p, &owned, &error);
    p->holds = holds;
    p->classes->limits.max_allocation_bytes = limit;
    CHECK(null_result == MC_PLAYER_OK && owned == NULL &&
          mc_player_coordinate_pin_value(NULL) == NULL);
    puts("COORDINATE_METHODS same=1 distinct=1 replaced-local=1 null=1");
    CHECK(mc_player_saved_spawn_get(NULL, &owned, &error) == MC_PLAYER_NULL_REFERENCE &&
          error.site.pc == 1 && owned == NULL &&
          mc_player_saved_spawn_set(NULL, NULL, &error) == MC_PLAYER_NULL_REFERENCE &&
          error.site.pc == 21 &&
          mc_player_saved_spawn_set(NULL, &input, &error) == MC_PLAYER_NULL_REFERENCE &&
          error.site.pc == 13);
    const size_t allocated = p->classes->allocated_bytes;
    const mc_player_coordinates raw_input = {4, 5, 6};
    CHECK(mc_local_player_allocate(f->entity_classes[0], &raw, NULL) == MC_ENTITY_OK &&
          mc_player_saved_spawn_set(raw, &raw_input, &error) == MC_PLAYER_OK &&
          raw->local_player->fields.player.private_b != &raw_input &&
          mc_player_saved_spawn_get(raw, &owned, &error) == MC_PLAYER_OK);
    mc_entity_release(raw);
    raw = NULL; /* The pin is now the only native Player owner. */
    CHECK(mc_player_coordinate_pin_value(owned)->x == 4 &&
          mc_player_coordinate_pin_value(owned)->y == 5 &&
          mc_player_coordinate_pin_value(owned)->z == 6);
    mc_player_coordinate_pin_release(&owned);
    CHECK(owned == NULL && p->classes->allocated_bytes == allocated && p->holds == holds);
    ok = true;
cleanup:
    mc_player_coordinate_pin_release(&first);
    mc_player_coordinate_pin_release(&second);
    mc_player_coordinate_pin_release(&owned);
    mc_entity_release(raw);
    return ok;
}

static bool coordinate_position_cases(mc_entity *p) {
    bool ok = false;
    const mc_entity_base_fields original = p->base;
    mc_entity_bounds *permanent = p->bounds;
    const mc_entity_bounds original_bounds = *permanent;
    const unsigned int mxcsr = _mm_getcsr();
    const int32_t vectors[][3] = {
        {4, 64, -7}, {16777217, 16777217, -16777217}, {INT32_MAX, INT32_MIN, INT32_MAX}};
    mc_entity_position_rotation_error error;
    p->base.width = .6f;
    p->base.height = 1.8f;
    p->base.offset = 1.62f;
    p->base.y_size = .125f;
    p->base.aU = -11;
    p->base.aV = -13;
    for (size_t i = 0; i < COUNT(vectors); ++i) {
        const double x = (double)((float)vectors[i][0] + .5f);
        const double y = (double)((float)vectors[i][1] + .1f);
        const double z = (double)((float)vectors[i][2] + .5f);
        CHECK(
            mc_entity_source_position_rotation(p, x, y, z, 37.0f, -17.0f, &error) == MC_ENTITY_OK &&
            error.bytecode == 80 && error.position_bytecode == 90 && p->bounds == permanent &&
            p->base.x == x && p->base.aJ == x && p->base.bl == x &&
            p->base.y == y + (double)p->base.offset && p->base.aK == p->base.y &&
            p->base.bm == p->base.y && p->base.z == z && p->base.aL == z && p->base.bn == z &&
            p->base.yaw == 37.0f && p->base.aT == -17.0f && p->base.aU == -11 && p->base.aV == -13);
        const double row[] = {p->base.x,        p->base.y,        p->base.z,
                              p->base.aJ,       p->base.aK,       p->base.aL,
                              p->base.bl,       p->base.bm,       p->base.bn,
                              permanent->min_x, permanent->min_y, permanent->min_z,
                              permanent->max_x, permanent->max_y, permanent->max_z};
        printf("COORDINATE_POSITION %zu", i);
        for (size_t n = 0; n < COUNT(row); ++n)
            printf(" %016" PRIx64, movement_double_bits(row[n]));
        printf("\n");
    }
    CHECK(p->base.x == 2147483648.0 && p->base.z == 2147483648.0);
    p->bounds = NULL;
    CHECK(mc_entity_source_position_rotation(p, 1, 2, 3, 4, 5, &error) == MC_ENTITY_STATE_ERROR &&
          error.bytecode == 77 && error.position_bytecode == 86 && error.source_null &&
          !error.native_policy && p->base.x == 1 && p->base.aJ == 1 && p->base.bl == 1 &&
          p->base.y == 2 + (double)p->base.offset && p->base.aK == p->base.y &&
          p->base.bm == p->base.y && p->base.z == 3 && p->base.aL == 3 && p->base.bn == 3 &&
          p->base.yaw == 4 && p->base.aT == 5);
    puts("COORDINATE_POSITION_NULL source=NPE complete-pose=1");
    p->bounds = permanent;
    const mc_entity_base_fields prefix = p->base;
    _mm_setcsr(mxcsr | 0x8000u);
    const mc_entity_status fp = mc_entity_source_position_rotation(p, 9, 10, 11, 12, 13, &error);
    _mm_setcsr(mxcsr);
    CHECK(fp == MC_ENTITY_UNSUPPORTED_FP && error.bytecode == 24 && error.position_bytecode == 0 &&
          error.native_policy && !error.source_null && p->base.x == 9 && p->base.aJ == 9 &&
          p->base.bl == 9 && p->base.y == prefix.y && p->base.aK == prefix.aK &&
          p->base.bm == prefix.bm && p->base.z == prefix.z && p->base.yaw == prefix.yaw &&
          p->base.aT == prefix.aT);
    CHECK(mc_entity_source_position_rotation(NULL, 1, 2, 3, 4, 5, &error) ==
              MC_ENTITY_STATE_ERROR &&
          error.bytecode == 5 && error.source_null && !error.native_policy);
    ok = !ferror(stdout);
cleanup:
    _mm_setcsr(mxcsr);
    p->base = original;
    p->bounds = permanent;
    *permanent = original_bounds;
    return ok;
}
