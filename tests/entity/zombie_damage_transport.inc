/* Authored passive transport copied from the existing genuine Mob fixture. */
static FILE *output;
static bool first;

static void key(const char *name) {
    fprintf(output, "%s\"%s\":", first ? "" : ",", name);
    first = false;
}

static void emit_i(const char *name, int32_t v) {
    key(name);
    fprintf(output, "%" PRId32, v);
}

static void emit_z(const char *name, bool v) {
    key(name);
    fputs(v ? "true" : "false", output);
}

static const char *entity_reference_class(const mc_entity *entity) {
    if (!entity)
        return NULL;
    mc_entity_class_literal_view value;
    if (mc_entity_class_literal_inspect(entity->class_literal, &value) != MC_ENTITY_OK ||
        value.owner != entity->classes || value.source != MC_ENTITY_CLIENT)
        exit(3);
    return value.client_name;
}

static void emit_object(const void *value, const char *class_name) {
    if (!value) {
        fputs("null", output);
        return;
    }
    if (!class_name)
        exit(3);
    /* Comparison-only native pointer value, never a Java hash or source input. */
    fprintf(output, "{\"class\":\"%s\",\"identity\":\"%016" PRIxPTR "\"}", class_name,
            (uintptr_t)value);
}

static void emit_r(const char *name, const void *value, const char *class_name) {
    key(name);
    emit_object(value, class_name);
}

static void emit_f(const char *name, float v) {
    key(name);
    fprintf(output, "{\"F\":\"%08" PRIx32 "\"}", fbits(v));
}

static void emit_d(const char *name, double v) {
    key(name);
    fprintf(output, "{\"D\":\"%016" PRIx64 "\"}", dbits(v));
}

static void emit_s(const char *name, const mc_player_string *v) {
    key(name);
    if (!v) {
        fputs("null", output);
        return;
    }
    mc_player_string_view s;
    if (!mc_player_string_read(v, &s))
        exit(3);
    fputs("{\"utf16\":[", output);
    for (size_t i = 0; i < s.length; ++i)
        fprintf(output, "%s%u", i ? "," : "", (unsigned)s.units[i]);
    fputs("]}", output);
}

static void emit_bounds(const mc_entity_bounds *b) {
    first = true;
    emit_d("eq.a", b->min_x);
    emit_d("eq.b", b->min_y);
    emit_d("eq.c", b->min_z);
    emit_d("eq.d", b->max_x);
    emit_d("eq.e", b->max_y);
    emit_d("eq.f", b->max_z);
}

static void emit_fields(mc_entity *entity, const mc_mob_state *value) {
    const mc_mob_state state = *value;
    first = true;
    /* Authored observation transport; original field order from pinned declarations. */
    emit_i("sn.aD", entity->base.id);
    emit_d("sn.aE", entity->base.aE);
    emit_z("sn.aF", entity->base.aF);
    emit_r("sn.aG", entity->base.aG, entity_reference_class(entity->base.aG));
    emit_r("sn.aH", entity->base.aH, entity_reference_class(entity->base.aH));
    emit_r("sn.aI", entity->base.world, "fd");
    emit_d("sn.aJ", entity->base.aJ);
    emit_d("sn.aK", entity->base.aK);
    emit_d("sn.aL", entity->base.aL);
    emit_d("sn.aM", entity->base.x);
    emit_d("sn.aN", entity->base.y);
    emit_d("sn.aO", entity->base.z);
    emit_d("sn.aP", entity->base.velocity_x);
    emit_d("sn.aQ", entity->base.velocity_y);
    emit_d("sn.aR", entity->base.velocity_z);
    emit_f("sn.aS", entity->base.yaw);
    emit_f("sn.aT", entity->base.aT);
    emit_f("sn.aU", entity->base.aU);
    emit_f("sn.aV", entity->base.aV);
    emit_r("sn.aW", entity->bounds, "eq");
    emit_z("sn.aX", entity->base.aX);
    emit_z("sn.aY", entity->base.aY);
    emit_z("sn.aZ", entity->base.aZ);
    emit_z("sn.ba", entity->base.ba);
    emit_z("sn.bb", entity->base.bb);
    emit_z("sn.bc", entity->base.bc);
    emit_z("sn.bd", entity->base.bd);
    emit_z("sn.be", entity->base.be);
    emit_f("sn.bf", entity->base.offset);
    emit_f("sn.bg", entity->base.width);
    emit_f("sn.bh", entity->base.height);
    emit_f("sn.bi", entity->base.bi);
    emit_f("sn.bj", entity->base.bj);
    emit_f("sn.bk", entity->base.bk);
    emit_i("sn.b", entity->base.private_b);
    emit_d("sn.bl", entity->base.bl);
    emit_d("sn.bm", entity->base.bm);
    emit_d("sn.bn", entity->base.bn);
    emit_f("sn.bo", entity->base.y_size);
    emit_f("sn.bp", entity->base.bp);
    emit_z("sn.bq", entity->base.bq);
    emit_f("sn.br", entity->base.br);
    emit_r("sn.bs", entity->random, "java.util.Random");
    emit_i("sn.bt", entity->base.bt);
    emit_i("sn.bu", entity->base.bu);
    emit_i("sn.bv", entity->base.bv);
    emit_i("sn.bw", entity->base.bw);
    emit_z("sn.bx", entity->base.bx);
    emit_i("sn.by", entity->base.by);
    emit_i("sn.bz", entity->base.bz);
    emit_z("sn.c", entity->base.private_c);
    emit_s("sn.bA", (const mc_player_string *)entity->base.client_only.bA);
    emit_s("sn.bB", entity->base.client_only.bB);
    emit_z("sn.bC", entity->base.bC);
    emit_r("sn.bD", entity->watcher, "ud");
    emit_f("sn.bE", entity->base.bE);
    emit_d("sn.d", entity->base.private_d);
    emit_d("sn.e", entity->base.private_e);
    emit_z("sn.bF", entity->base.bF);
    emit_i("sn.bG", entity->base.bG);
    emit_i("sn.bH", entity->base.bH);
    emit_i("sn.bI", entity->base.bI);
    emit_i("sn.bJ", entity->base.client_only.bJ);
    emit_i("sn.bK", entity->base.client_only.bK);
    emit_i("sn.bL", entity->base.client_only.bL);
    emit_z("sn.bM", entity->base.bM);
    emit_i("ls.E", state.living.E);
    emit_f("ls.F", state.living.F);
    emit_f("ls.G", state.living.G);
    emit_f("ls.H", state.living.H);
    emit_f("ls.I", state.living.I);
    emit_f("ls.J", state.living.J);
    emit_f("ls.K", state.living.K);
    emit_f("ls.L", state.living.L);
    emit_f("ls.M", state.living.M);
    emit_z("ls.N", state.living.N);
    emit_s("ls.O", state.living.O);
    emit_z("ls.P", state.living.P);
    emit_f("ls.Q", state.living.Q);
    emit_s("ls.R", state.living.R);
    emit_f("ls.S", state.living.S);
    emit_i("ls.T", state.living.T);
    emit_f("ls.U", state.living.U);
    emit_z("ls.V", state.living.V);
    emit_f("ls.W", state.living.W);
    emit_f("ls.X", state.living.X);
    emit_i("ls.Y", state.living.Y);
    emit_i("ls.Z", state.living.Z);
    emit_i("ls.a", state.living.private_a);
    emit_i("ls.aa", state.living.aa);
    emit_i("ls.ab", state.living.ab);
    emit_f("ls.ac", state.living.ac);
    emit_i("ls.ad", state.living.ad);
    emit_i("ls.ae", state.living.ae);
    emit_f("ls.af", state.living.af);
    emit_f("ls.ag", state.living.ag);
    emit_z("ls.ah", state.living.ah);
    emit_i("ls.ai", state.living.ai);
    emit_f("ls.aj", state.living.aj);
    emit_f("ls.ak", state.living.ak);
    emit_f("ls.al", state.living.al);
    emit_f("ls.am", state.living.am);
    emit_i("ls.an", state.living.an);
    emit_d("ls.ao", state.living.ao);
    emit_d("ls.ap", state.living.ap);
    emit_d("ls.aq", state.living.aq);
    emit_d("ls.ar", state.living.ar);
    emit_d("ls.as", state.living.as);
    emit_f("ls.at", state.living.at);
    emit_i("ls.au", state.living.au);
    emit_i("ls.av", state.living.av);
    emit_f("ls.aw", state.living.aw);
    emit_f("ls.ax", state.living.ax);
    emit_f("ls.ay", state.living.ay);
    emit_z("ls.az", state.living.az);
    emit_f("ls.aA", state.living.aA);
    emit_f("ls.aB", state.living.aB);
    emit_r("ls.b", state.living.private_b, entity_reference_class(state.living.private_b));
    emit_i("ls.aC", state.living.aC);

    if (state.kind != MC_ENTITY_CLASS_KIND_SLIME) {
        emit_r("ii.a", state.creature_path, "dh");
        emit_r("ii.d", state.creature_target, entity_reference_class(state.creature_target));
        emit_z("ii.e", state.creature_attacking);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_SPIDER || state.kind == MC_ENTITY_CLASS_KIND_ZOMBIE ||
        state.kind == MC_ENTITY_CLASS_KIND_SKELETON || state.kind == MC_ENTITY_CLASS_KIND_CREEPER) {
        emit_i("gz.c", state.monster_damage);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_CREEPER) {
        emit_i("gb.a", state.creeper_a);
        emit_i("gb.b", state.creeper_b);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_SLIME) {
        emit_f("uw.a", state.slime_a);
        emit_f("uw.b", state.slime_b);
        emit_i("uw.c", state.slime_jump_delay);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_CHICKEN) {
        emit_z("ww.a", state.chicken.a);
        emit_f("ww.b", state.chicken.b);
        emit_f("ww.c", state.chicken.c);
        emit_f("ww.f", state.chicken.f);
        emit_f("ww.g", state.chicken.g);
        emit_f("ww.h", state.chicken.h);
        emit_i("ww.i", state.chicken.i);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_WOLF) {
        emit_z("gi.a", state.wolf.a);
        emit_f("gi.b", state.wolf.b);
        emit_f("gi.c", state.wolf.c);
        emit_z("gi.f", state.wolf.f);
        emit_z("gi.g", state.wolf.g);
        emit_f("gi.h", state.wolf.h);
        emit_f("gi.i", state.wolf.i);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_SQUID) {
        emit_f("xt.a", state.squid.a);
        emit_f("xt.b", state.squid.b);
        emit_f("xt.c", state.squid.c);
        emit_f("xt.f", state.squid.f);
        emit_f("xt.g", state.squid.g);
        emit_f("xt.h", state.squid.h);
        emit_f("xt.i", state.squid.i);
        emit_f("xt.j", state.squid.j);
        emit_f("xt.k", state.squid.k);
        emit_f("xt.l", state.squid.l);
        emit_f("xt.m", state.squid.m);
        emit_f("xt.n", state.squid.n);
        emit_f("xt.o", state.squid.o);
        emit_f("xt.p", state.squid.p);
    }
}

static bool emit_watcher(const mc_entity *entity) {
    CHECK(entity->watcher && entity->watcher->map);
    fprintf(output, "{\"size\":%zu,\"dirty\":%s,\"records\":[", entity->watcher->map->size,
            entity->watcher->dirty ? "true" : "false");
    size_t seen = 0;
    for (int32_t index = 0; index < 32; ++index)
        for (const mc_entity_watch_node *n = entity->watcher->map->head; n; n = n->next)
            if (n->key == index) {
                CHECK(n->record && n->record->index == index &&
                      (n->record->type == 0 || n->record->type == 2 || n->record->type == 4));
                fprintf(output, "%s{\"index\":%d,\"type\":%d,\"value\":", seen++ ? "," : "", index,
                        n->record->type);
                if (n->record->type == 4) {
                    mc_player_string_view text;
                    CHECK(mc_player_string_read(n->record->string_value, &text));
                    fputs("{\"utf16\":[", output);
                    for (size_t j = 0; j < text.length; ++j)
                        fprintf(output, "%s%u", j ? "," : "", (unsigned int)text.units[j]);
                    fputs("]}", output);
                } else
                    fprintf(output, "%d",
                            n->record->type == 0 ? (int)n->record->byte_value
                                                 : n->record->integer_value);
                fprintf(output, ",\"dirty\":%s}", n->record->dirty ? "true" : "false");
            }
    CHECK(seen == entity->watcher->map->size);
    fputs("]}", output);
    return true;
}
