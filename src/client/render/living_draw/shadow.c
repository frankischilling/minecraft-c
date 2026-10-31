#include "internal.h"
#include "mc/core/java/java.h"
#include "world/runtime/block_normal_internal.h"

static void shadow_at(ld_frame *f, int32_t pc) {
    ld_at(f, "bw", "c", "(Lsn;DDDFF)V", pc);
}

static bool shadow_floor(ld_frame *f, double value, int32_t *out, int32_t pc) {
    shadow_at(f, pc);
    if (!ld_capture(f, mc_client_draw_capture_math(f->scope, &f->error->detail.capture)))
        return false;
    if (f->scope->model.math == NULL)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Shadow reached actual Math namespace");
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_MATH;
    if (mc_math_classes_ensure(f->scope->model.math, &f->error->detail.math) != MC_MATH_CLASS_OK)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, "Shadow actual Math active use");
    if (!ld_fp(f))
        return false;
    *out = mc_math_floor_double(value);
    return true;
}

static bool query_begin(ld_frame *f, const void *receiver, const char *declaring,
                        const char *method, const char *descriptor, const int32_t *args,
                        size_t nargs, mc_client_manager_draw_query_kind kind, size_t *index) {
    const mc_client_manager_draw_query q = {.receiver = (uintptr_t)receiver,
                                            .method = {declaring, method, descriptor, 0},
                                            .caller = f->error->site,
                                            .nargs = nargs,
                                            .kind = kind};
    mc_client_manager_draw_query record = q;
    if (nargs != 0)
        memcpy(record.args, args, nargs * sizeof(*args));
    if (!mc_client_draw_query_begin(f->scope, &record, index, &f->error->capture_failure)) {
        f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_CAPTURE;
        f->error->throwable = MC_CLIENT_LIVING_DRAW_NO_THROWABLE;
        f->error->source_exception = false;
        f->error->exception_class = NULL;
        return ld_fail(f,
                       f->error->capture_failure.status == MC_CLIENT_MANAGER_LIMIT
                           ? MC_CLIENT_LIVING_DRAW_LIMIT
                           : MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                       f->error->capture_failure.operation);
    }
    return true;
}

static void query_end(ld_frame *f, size_t index, bool invoked, bool returned, uint32_t bits) {
    mc_client_draw_query_end(
        f->scope, index, invoked, returned, bits, !returned && f->error->native_policy,
        !returned && f->error->source_exception, returned ? NULL : f->error->exception_class);
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool query_failure(ld_frame *f, mc_world_runtime *world) {
    mc_client_living_draw_error *e = f->error;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_QUERY;
    e->child = ld_site("fd", "n", "(III)I", (int32_t)e->detail.query.bytecode);
    ld_registry_classify(f, mc_world_runtime_registry(world), &e->detail.query.registry);
    if (e->detail.query.provider_bytecode == 6 && world->fields.v == NULL) {
        e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
    }
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, e->detail.query.operation);
}

static bool block_failure(ld_frame *f, mc_world_runtime *world) {
    mc_client_living_draw_error *e = f->error;
    mc_world_runtime_acquisition_error *v = &e->detail.block.acquisition;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_BLOCK;
    e->child = ld_site("fd", "a", "(III)I", 0);
    ld_registry_classify(f, mc_world_runtime_registry(world), &v->registry);
    ld_access_classify(f, &v->batch.publication.world_access_failure);
    ld_access_classify(f, &v->tile_entity.world_access_failure);
    ld_access_classify(f, &v->tile_entity.publication.world_access_failure);
    ld_access_classify(f, &v->tile_entity.batch.publication.world_access_failure);
    if (world->fields.v == NULL && v->status == MC_CHUNK_ACQUIRE_OK) {
        e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        e->child = ld_site("fd", "c", "(II)Llm;", 6);
    }
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, e->detail.block.cause.operation);
}

static bool quad(ld_frame *f, const mc_block *block, double x, double y, double z, int32_t ix,
                 int32_t iy, int32_t iz, float alpha, float radius, double ox, double oy,
                 double oz) {
    mc_client_living_draw_error *e = f->error;
    ld_at(f, "bw", "a", "(Luu;DDDIIIFFDDD)V", 0);
    mc_client_tessellator *nw = NULL;
    if (!ld_graphics(f, mc_client_nw_active_use(f->scope->model.graphics, e->site, &nw,
                                                &e->detail.graphics)))
        return false;
    ld_at(f, "bw", "a", "(Luu;DDDIIIFFDDD)V", 6);
    size_t normal_receipt;
    if (!query_begin(f, block, block == NULL ? "uu" : mc_world_source_block_normal_declaring(block),
                     "d", "()Z", NULL, 0u, MC_CLIENT_MANAGER_DRAW_QUERY_BOOLEAN, &normal_receipt))
        return false;
    if (block == NULL) {
        (void)ld_null(f, "Shadow quad null normal Block invoke");
        query_end(f, normal_receipt, false, false, 0u);
        return false;
    }
    ++e->state.shadow_normal_queries;
    mc_client_draw_query_enter(f->scope, normal_receipt);
    const bool normal = mc_world_source_block_normal(block);
    query_end(f, normal_receipt, true, true, normal ? 1u : 0u);
    if (!normal)
        return true;
    if (!ld_fp(f))
        return false;
    const double weighted =
        at_mul(at_sub(at_widen(alpha), at_div(at_sub(y, at_add((double)iy, oy)), 2.0)), .5);
    ld_at(f, "bw", "b", "()Lfd;", 4);
    mc_client_render_manager *manager = ld_manager(f, 4);
    if (manager == NULL)
        return false;
    mc_world_runtime *world = (mc_world_runtime *)manager->fields.g;
    if (!ld_capture(f, mc_client_draw_capture_world(f->scope, world, &e->detail.capture)))
        return false;
    e->state.last_quad_world_identity = (uintptr_t)world;
    ld_at(f, "bw", "a", "(Luu;DDDIIIFFDDD)V", 44);
    const int32_t brightness_args[] = {ix, iy, iz};
    size_t brightness_receipt;
    if (!query_begin(f, world, "fd", "c", "(III)F", brightness_args, 3u,
                     MC_CLIENT_MANAGER_DRAW_QUERY_FLOAT_BITS, &brightness_receipt))
        return false;
    if (world == NULL) {
        (void)ld_null(f, "Shadow quad null fresh brightness World");
        query_end(f, brightness_receipt, false, false, 0u);
        return false;
    }
    ++e->state.shadow_brightness_queries;
    float brightness;
    mc_client_draw_query_enter(f->scope, brightness_receipt);
    if (mc_world_runtime_brightness(world, ix, iy, iz, &brightness, &e->detail.brightness) !=
        MC_WORLD_STORAGE_OK) {
        e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_BRIGHTNESS;
        e->child = ld_site("fd", "c", "(III)F", (int32_t)e->detail.brightness.bytecode);
        const mc_world_brightness_exception cause = e->detail.brightness.source_exception;
        if (cause == MC_WORLD_BRIGHTNESS_SOURCE_NULL_DIMENSION ||
            cause == MC_WORLD_BRIGHTNESS_SOURCE_NULL_TABLE) {
            (void)ld_null(f, "Shadow actual brightness null receiver");
            query_end(f, brightness_receipt, true, false, 0u);
            return false;
        }
        if (cause == MC_WORLD_BRIGHTNESS_SOURCE_INDEX) {
            e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
            e->source_exception = true;
            e->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        } else if (e->detail.brightness.query.provider_bytecode == 6 && world->fields.v == NULL) {
            e->child = ld_site("fd", "g", "(II)Z", 6);
            (void)ld_null(f, "Shadow actual brightness null provider");
            query_end(f, brightness_receipt, true, false, 0u);
            return false;
        } else
            ld_registry_classify(f, mc_world_runtime_registry(world),
                                 &e->detail.brightness.query.registry);
        (void)ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, e->detail.brightness.operation);
        query_end(f, brightness_receipt, true, false, 0u);
        return false;
    }
    query_end(f, brightness_receipt, true, true, float_bits(brightness));
    ld_at(f, "bw", "a", "(Luu;DDDIIIFFDDD)V", 48);
    if (!ld_fp(f))
        return false;
    double opacity = at_mul(weighted, at_widen(brightness));
    if (opacity < 0.0)
        return true;
    if (opacity > 1.0)
        opacity = 1.0;
    ld_at(f, "bw", "a", "(Luu;DDDIIIFFDDD)V", 77);
    if (nw == NULL)
        return ld_null(f, "Shadow quad null Tessellator color invoke");
    if (!ld_graphics(f, mc_client_nw_color_rgba_float(nw, 1.0f, 1.0f, 1.0f, at_narrow(opacity),
                                                      &e->detail.graphics)))
        return false;
    /* All five bounds reads follow the actual brightness child. */
    const double x0 = at_add(at_add((double)ix, block->fields.bs), ox);
    const double x1 = at_add(at_add((double)ix, block->fields.bv), ox);
    const double yy = at_add(at_add(at_add((double)iy, block->fields.bt), oy), .015625);
    const double z0 = at_add(at_add((double)iz, block->fields.bu), oz);
    const double z1 = at_add(at_add((double)iz, block->fields.bx), oz);
    const float u0 = at_narrow(at_add(at_div(at_div(at_sub(x, x0), 2.0), at_widen(radius)), .5));
    const float u1 = at_narrow(at_add(at_div(at_div(at_sub(x, x1), 2.0), at_widen(radius)), .5));
    const float v0 = at_narrow(at_add(at_div(at_div(at_sub(z, z0), 2.0), at_widen(radius)), .5));
    const float v1 = at_narrow(at_add(at_div(at_div(at_sub(z, z1), 2.0), at_widen(radius)), .5));
    ld_at(f, "bw", "a", "(Luu;DDDIIIFFDDD)V", 241);
    if (f->scope->shadow_quads == f->scope->max_shadow_quads)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_LIMIT, "Shadow reached native quad ceiling");
    ++f->scope->shadow_quads;
    const double xx[] = {x0, x0, x1, x1}, zz[] = {z0, z1, z1, z0};
    const float uu[] = {u0, u0, u1, u1}, vv[] = {v0, v1, v1, v0};
    static const int32_t pcs[] = {241, 258, 275, 292};
    for (size_t i = 0; i < 4; ++i) {
        ld_at(f, "bw", "a", "(Luu;DDDIIIFFDDD)V", pcs[i]);
        if (!ld_work(f) ||
            !ld_graphics(f, mc_client_nw_vertex_uv(nw, xx[i], yy, zz[i], at_widen(uu[i]),
                                                   at_widen(vv[i]), &e->detail.graphics)))
            return false;
        ++e->state.shadow_vertices;
    }
    ++e->state.shadow_quads;
    return true;
}

bool ld_shadow(ld_frame *f, double x, double y, double z, float alpha, float partial) {
    mc_client_living_draw_error *e = f->error;
    shadow_at(f, 3);
    if (!ld_capture(f, mc_client_draw_capture_graphics(f->scope, &e->detail.capture)))
        return false;
    if (!ld_display(f, mc_client_display_source_blend_enable(f->scope->model.display, true,
                                                             &e->detail.display)))
        return false;
    shadow_at(f, 12);
    if (!ld_display(f, mc_client_display_source_blend_function(f->scope->model.display, 770, 771,
                                                               &e->detail.display)))
        return false;
    shadow_at(f, 19);
    mc_client_render_manager *texture_manager = ld_manager(f, 19);
    if (texture_manager == NULL)
        return false;
    mc_client_render_engine *engine = texture_manager->fields.e;
    if (!ld_texture_engine(f, engine, ld_shadow_literal(), ld_site("bw", "c", "(Lsn;DDDFF)V", 30),
                           ld_site("bw", "c", "(Lsn;DDDFF)V", 33)))
        return false;
    ld_at(f, "bw", "b", "()Lfd;", 4);
    mc_client_render_manager *manager = ld_manager(f, 4);
    if (manager == NULL)
        return false;
    mc_world_runtime *world = (mc_world_runtime *)manager->fields.g;
    if (!ld_capture(f, mc_client_draw_capture_world(f->scope, world, &e->detail.capture)))
        return false;
    e->state.shadow_world_identity = (uintptr_t)world;
    shadow_at(f, 43);
    if (!ld_display(f, mc_client_display_source_depth_mask(f->scope->model.display, false,
                                                           &e->detail.display)))
        return false;
    const float radius = f->renderer->shadow_size;
    shadow_at(f, 56);
    if (f->actor == NULL)
        return ld_null(f, "Shadow interpolation null actor");
    if (!ld_fp(f))
        return false;
    const double xx = at_add(
        f->actor->base.bl, at_mul(at_sub(f->actor->base.x, f->actor->base.bl), at_widen(partial)));
    double yy = at_add(f->actor->base.bm,
                       at_mul(at_sub(f->actor->base.y, f->actor->base.bm), at_widen(partial)));
    shadow_at(f, 91);
    float offset;
    if (!ld_entity(f, mc_entity_render_half_height(f->actor, &offset, &e->detail.entity)))
        return false;
    yy = at_add(yy, at_widen(offset));
    const double zz = at_add(
        f->actor->base.bn, at_mul(at_sub(f->actor->base.z, f->actor->base.bn), at_widen(partial)));
    int32_t minx, maxx, miny, maxy, minz, maxz;
    if (!shadow_floor(f, at_sub(xx, at_widen(radius)), &minx, 124) ||
        !shadow_floor(f, at_add(xx, at_widen(radius)), &maxx, 135) ||
        !shadow_floor(f, at_sub(yy, at_widen(radius)), &miny, 146) ||
        !shadow_floor(f, yy, &maxy, 153) ||
        !shadow_floor(f, at_sub(zz, at_widen(radius)), &minz, 164) ||
        !shadow_floor(f, at_add(zz, at_widen(radius)), &maxz, 175))
        return false;
    const double ox = at_sub(x, xx), oy = at_sub(y, yy), oz = at_sub(z, zz);
    shadow_at(f, 200);
    mc_client_tessellator *nw = NULL;
    if (!ld_graphics(f, mc_client_nw_active_use(f->scope->model.graphics, e->site, &nw,
                                                &e->detail.graphics)))
        return false;
    shadow_at(f, 207);
    if (nw == NULL)
        return ld_null(f, "Shadow null Tessellator begin invoke");
    if (!ld_graphics(f, mc_client_nw_begin_quads(nw, &e->detail.graphics)))
        return false;
    for (int32_t ix = minx; ix <= maxx; ix = mc_java_i32_add(ix, 1))
        for (int32_t iy = miny; iy <= maxy; iy = mc_java_i32_add(iy, 1))
            for (int32_t iz = minz; iz <= maxz; iz = mc_java_i32_add(iz, 1)) {
                shadow_at(f, 253);
                e->state.shadow_x = ix;
                e->state.shadow_y = iy;
                e->state.shadow_z = iz;
                if (!ld_work(f))
                    return false;
                if (f->scope->shadow_cells == f->scope->max_shadow_cells)
                    return ld_fail(f, MC_CLIENT_LIVING_DRAW_LIMIT,
                                   "Shadow reached native cell ceiling");
                ++f->scope->shadow_cells;
                const int32_t block_args[] = {ix, mc_java_i32_sub(iy, 1), iz};
                size_t block_receipt;
                if (!query_begin(f, world, "fd", "a", "(III)I", block_args, 3u,
                                 MC_CLIENT_MANAGER_DRAW_QUERY_INTEGER, &block_receipt))
                    return false;
                if (world == NULL) {
                    (void)ld_null(f, "Shadow null captured terrain World");
                    query_end(f, block_receipt, false, false, 0u);
                    return false;
                }
                ++e->state.shadow_block_queries;
                int32_t id;
                mc_client_draw_query_enter(f->scope, block_receipt);
                if (mc_world_runtime_block_get_detailed(world, block_args[0], block_args[1],
                                                        block_args[2], &id, &e->detail.block.cause,
                                                        &e->detail.block.acquisition) !=
                    MC_WORLD_STORAGE_OK) {
                    (void)block_failure(f, world);
                    query_end(f, block_receipt, true, false, 0u);
                    return false;
                }
                query_end(f, block_receipt, true, true, (uint32_t)id);
                e->state.shadow_block = id;
                if (id <= 0)
                    continue;
                shadow_at(f, 271);
                const int32_t light_args[] = {ix, iy, iz};
                size_t light_receipt;
                if (!query_begin(f, world, "fd", "n", "(III)I", light_args, 3u,
                                 MC_CLIENT_MANAGER_DRAW_QUERY_INTEGER, &light_receipt))
                    return false;
                ++e->state.shadow_light_queries;
                int32_t light;
                mc_client_draw_query_enter(f->scope, light_receipt);
                if (mc_world_runtime_block_light(world, ix, iy, iz, &light, &e->detail.query) !=
                    MC_WORLD_STORAGE_OK) {
                    (void)query_failure(f, world);
                    query_end(f, light_receipt, true, false, 0u);
                    return false;
                }
                query_end(f, light_receipt, true, true, (uint32_t)light);
                e->state.shadow_light = light;
                if (light <= 3)
                    continue;
                shadow_at(f, 279);
                const mc_block *block = NULL;
                mc_registry_classes *registry = f->renderer->parent->classes->registry;
                if (mc_registry_block_get(registry, id, &block, &e->detail.registry) !=
                    MC_REGISTRY_OK) {
                    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_REGISTRY;
                    ld_registry_classify(f, registry, &e->detail.registry);
                    if (e->detail.registry.active_initializer_class == NULL &&
                        (e->detail.registry.status == MC_REGISTRY_NULL_REFERENCE ||
                         e->detail.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE))
                        shadow_at(f, 284);
                    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                                   "Shadow actual current Block array entry");
                }
                shadow_at(f, 289);
                if (!ld_entity(f,
                               mc_entity_render_half_height(f->actor, &offset, &e->detail.entity)))
                    return false;
                const double argument_y = at_add(y, at_widen(offset));
                shadow_at(f, 311);
                if (!ld_entity(f,
                               mc_entity_render_half_height(f->actor, &offset, &e->detail.entity)))
                    return false;
                const double argument_oy = at_add(oy, at_widen(offset));
                if (!quad(f, block, x, argument_y, z, ix, iy, iz, alpha, radius, ox, argument_oy,
                          oz))
                    return false;
            }
    shadow_at(f, 341);
    if (!ld_graphics(f, mc_client_nw_draw(nw, f->scope->model.display, &e->detail.graphics)))
        return false;
    shadow_at(f, 348);
    if (!ld_display(f, mc_client_display_source_color(f->scope->model.display, 1.0f, 1.0f, 1.0f,
                                                      1.0f, &e->detail.display)))
        return false;
    shadow_at(f, 354);
    if (!ld_display(f, mc_client_display_source_blend_enable(f->scope->model.display, false,
                                                             &e->detail.display)))
        return false;
    shadow_at(f, 358);
    if (!ld_display(f, mc_client_display_source_depth_mask(f->scope->model.display, true,
                                                           &e->detail.display)))
        return false;
    shadow_at(f, 361);
    e->state.shadow_returned = true;
    return true;
}

bool mc_client_entity_post_draw_entered(mc_client_draw_scope *scope, mc_client_managed_renderer *r,
                                        mc_entity *actor, double x, double y, double z, float yaw,
                                        float partial, mc_client_graphics_site caller,
                                        mc_client_living_draw_error *e) {
    (void)yaw;
    if (e == NULL)
        return false;
    mc_client_living_draw_error_init(e);
    e->caller = caller;
    ld_frame f = {scope, r, actor, e};
    if (scope == NULL || r == NULL)
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_ARGUMENT, "Postdraw native scope/renderer");
    ld_at(&f, "bw", "b", "(Lsn;DDDFF)V", 4);
    mc_client_render_manager *manager = ld_manager(&f, 4);
    if (manager == NULL)
        return false;
    mc_game_settings *settings = (mc_game_settings *)manager->fields.k;
    if (!ld_capture(&f, mc_client_draw_capture_settings(scope, settings, &e->detail.capture)))
        return false;
    ld_at(&f, "bw", "b", "(Lsn;DDDFF)V", 7);
    if (settings == NULL)
        return ld_null(&f, "Postdraw null Settings shadow flag");
    mc_game_settings_state values;
    if (!mc_game_settings_read_state(settings, &values))
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_STATE, "Postdraw actual Settings fields");
    e->state.shadow_settings = values.values.fancy_graphics;
    if (values.values.fancy_graphics && r->shadow_size > 0.0f) {
        ld_at(&f, "bw", "b", "(Lsn;DDDFF)V", 27);
        if (actor == NULL)
            return ld_null(&f, "Postdraw null actor distance arguments");
        const double ax = actor->base.x, ay = actor->base.y, az = actor->base.z;
        ld_at(&f, "th", "a", "(DDD)D", 5);
        if (!ld_fp(&f))
            return false;
        const double dx = at_sub(ax, manager->fields.l);
        const double dy = at_sub(ay, manager->fields.m);
        const double dz = at_sub(az, manager->fields.n);
        const double distance = at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz));
        ld_at(&f, "bw", "b", "(Lsn;DDDFF)V", 57);
        const float alpha =
            at_narrow(at_mul(at_sub(1.0, at_div(distance, 256.0)), at_widen(r->shadow_opacity)));
        e->state.shadow_alpha = alpha;
        if (alpha > 0.0f) {
            e->state.shadow_started = true;
            if (!ld_shadow(&f, x, y, z, alpha, partial))
                return false;
        }
    }
    ld_at(&f, "bw", "b", "(Lsn;DDDFF)V", 82);
    if (actor == NULL)
        return ld_null(&f, "Postdraw null actor burning invoke");
    bool burning;
    if (!ld_entity(&f, mc_entity_render_burning(actor, &burning, &e->detail.entity)))
        return false;
    e->state.fire_checked = true;
    e->state.burning = burning;
    if (burning) {
        ld_at(&f, "bw", "b", "(Lsn;DDDFF)V", 97);
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Postdraw actual burning billboard child");
    }
    ld_at(&f, "bw", "b", "(Lsn;DDDFF)V", 100);
    e->state.returned = true;
    return true;
}
