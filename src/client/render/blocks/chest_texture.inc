/* Actual E.a(XP,IIII)I and KS's distinct single-Chest body. XP is the captured
 * source argument, never a sequence of unchecked callback return values. */
static bool chest_o(mc_cv_context *c, int32_t id, unsigned int getstatic_pc, unsigned int load_pc,
                    bool *out) {
    if (!mc_cv_block_statics(c, getstatic_pc))
        return false;
    const bool *o = c->registry->blocks.o;
    if (o == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual Chest uu.o array",
                          load_pc, false);
    if (id < 0 || id >= 256) {
        c->failure.registry =
            (mc_registry_error){.status = MC_REGISTRY_INDEX_OUT_OF_RANGE, .block_id = id};
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          "actual Chest uu.o signed array index", load_pc, false);
    }
    *out = o[id];
    return true;
}

static bool chest_pair(mc_cv_context *c, int32_t a, int32_t b, int32_t d, int32_t e,
                       unsigned int pc, bool *out) {
    bool value;
    if (!chest_o(c, a, pc, pc + 5, &value))
        return false;
    if (!value) {
        if (!chest_o(c, b, pc + 9, pc + 14, &value))
            return false;
        if (!value) {
            *out = false;
            return true;
        }
    }
    if (!chest_o(c, d, pc + 18, pc + 23, &value))
        return false;
    if (value) {
        *out = false;
        return true;
    }
    if (!chest_o(c, e, pc + 27, pc + 32, &value))
        return false;
    *out = !value;
    return true;
}

static bool chest_texture_checked(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y,
                                  int32_t z, int32_t side, int32_t *out) {
    if (side == 1 || side == 0) {
        *out = mc_java_i32_sub(b->fields.bm, 1);
        return true;
    }
    const void *xp = c->fields->c;
    int32_t n, s, w, e;
    if (!mc_cv_block_bound(c, xp, x, y, mc_java_i32_sub(z, 1), 32, &n) ||
        !mc_cv_block_bound(c, xp, x, y, mc_java_i32_add(z, 1), 46, &s) ||
        !mc_cv_block_bound(c, xp, mc_java_i32_sub(x, 1), y, z, 60, &w) ||
        !mc_cv_block_bound(c, xp, mc_java_i32_add(x, 1), y, z, 74, &e))
        return false;
    const bool locked = b->kind == MC_BLOCK_LOCKED_CHEST;
    if (!locked && (n == b->fields.bn || s == b->fields.bn)) {
        if (side == 2 || side == 3) {
            *out = b->fields.bm;
            return true;
        }
        int32_t offset = n == b->fields.bn ? -1 : 0;
        int32_t a, d;
        if (!mc_cv_block_bound(c, xp, mc_java_i32_sub(x, 1), y,
                               n == b->fields.bn ? mc_java_i32_sub(z, 1) : mc_java_i32_add(z, 1),
                               156, &a))
            return false;
        /* E171 rereads bn after the first real query has returned. */
        if (!mc_cv_block_bound(c, xp, mc_java_i32_add(x, 1), y,
                               n == b->fields.bn ? mc_java_i32_sub(z, 1) : mc_java_i32_add(z, 1),
                               188, &d))
            return false;
        if (side == 4)
            offset = mc_java_i32_sub(-1, offset);
        int32_t front = 5;
        bool condition;
        if (!chest_pair(c, w, a, e, d, 210, &condition))
            return false;
        if (condition)
            front = 5;
        if (!chest_pair(c, e, d, w, a, 249, &condition))
            return false;
        if (condition)
            front = 4;
        *out = mc_java_i32_add(mc_java_i32_add(b->fields.bm, side == front ? 16 : 32), offset);
        return true;
    }
    if (!locked && (w == b->fields.bn || e == b->fields.bn)) {
        if (side == 4 || side == 5) {
            *out = b->fields.bm;
            return true;
        }
        int32_t offset = w == b->fields.bn ? -1 : 0;
        int32_t a, d;
        if (!mc_cv_block_bound(c, xp,
                               w == b->fields.bn ? mc_java_i32_sub(x, 1) : mc_java_i32_add(x, 1), y,
                               mc_java_i32_sub(z, 1), 390, &a))
            return false;
        if (!mc_cv_block_bound(c, xp,
                               w == b->fields.bn ? mc_java_i32_sub(x, 1) : mc_java_i32_add(x, 1), y,
                               mc_java_i32_add(z, 1), 421, &d))
            return false;
        if (side == 3)
            offset = mc_java_i32_sub(-1, offset);
        int32_t front = 3;
        bool condition;
        if (!chest_pair(c, n, a, s, d, 443, &condition))
            return false;
        if (condition)
            front = 3;
        if (!chest_pair(c, s, d, n, a, 482, &condition))
            return false;
        if (condition)
            front = 2;
        *out = mc_java_i32_add(mc_java_i32_add(b->fields.bm, side == front ? 16 : 32), offset);
        return true;
    }
    const unsigned int base = locked ? 84 : 552;
    const int32_t first[4] = {n, s, w, e}, second[4] = {s, n, e, w};
    const int32_t fronts[4] = {3, 2, 5, 4};
    int32_t front = 3;
    for (unsigned int i = 0; i < 4; ++i) {
        bool solid;
        const unsigned int pc = base + i * 21;
        if (!chest_o(c, first[i], pc, pc + 5, &solid))
            return false;
        if (solid) {
            if (!chest_o(c, second[i], pc + 9, pc + 14, &solid))
                return false;
            if (!solid)
                front = fronts[i];
        }
    }
    *out = side == front ? mc_java_i32_add(b->fields.bm, 1) : b->fields.bm;
    return true;
}
