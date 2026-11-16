#include "draw_internal.h"

#include "../interpolation_internal.h"
#include "client/display/camera_surface.h"
#include "world/runtime/construction_internal.h"

#include <string.h>

static const char spawner_descriptor[] = "(Lcy;DDDF)V";

static void at(mc_client_tile_renderer_draw_call *c, int32_t pc) {
    tile_draw_at(c, "ag", spawner_descriptor, pc);
}

/* String value equality includes a distinct NULL key. Native bucket order is
 * not a claim about JDK HashMap iteration; AG performs only get/put. */
static bool name_equal(const mc_tile_entity_name *left, const mc_tile_entity_name *right) {
    mc_tile_entity_name_view a, b;
    if (mc_tile_entity_name_get_view(left, &a) != MC_TILE_ENTITY_OK ||
        mc_tile_entity_name_get_view(right, &b) != MC_TILE_ENTITY_OK || a.is_null != b.is_null ||
        a.text.length != b.text.length)
        return false;
    return a.text.length == 0 ||
           memcmp(a.text.units, b.text.units, a.text.length * sizeof(*a.text.units)) == 0;
}

static size_t name_bucket(const mc_tile_entity_name *name, size_t capacity) {
    mc_tile_entity_name_view view;
    if (mc_tile_entity_name_get_view(name, &view) != MC_TILE_ENTITY_OK)
        return 0;
    uint32_t hash = 0;
    for (size_t i = 0; i < view.text.length; ++i)
        hash = hash * UINT32_C(31) + view.text.units[i];
    return (size_t)hash & (capacity - 1);
}

static tile_renderer_spawner_node *find(tile_renderer_spawner_map *map,
                                        const mc_tile_entity_name *name) {
    if (map->capacity == 0)
        return NULL;
    for (tile_renderer_spawner_node *p = map->buckets[name_bucket(name, map->capacity)]; p != NULL;
         p = p->next)
        if (name_equal(p->key, name))
            return p;
    return NULL;
}

static bool get_name(mc_client_tile_renderer_draw_call *c, size_t index, int32_t pc) {
    at(c, pc);
    c->error.owner.tile_entity = mc_tile_entity_spawner_get_name(c->tile_entity, &c->names[index]);
    if (c->error.owner.tile_entity == MC_TILE_ENTITY_OK)
        return true;
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                          "AG actual CY.a String getter", true);
}

static bool map_put(mc_client_tile_renderer_draw_call *c, tile_renderer_spawner_map *map) {
    at(c, 63);
    mc_client_tile_renderer *renderer = c->renderer;
    tile_renderer_spawner_node *node = find(map, c->names[2]);
    if (c->factory_result != NULL) {
        if (mc_entity_retain(c->factory_result) != MC_ENTITY_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ENTITY_FAILED,
                                  "AG actual Map Entity value occurrence", true);
        c->map_value_pending = c->factory_result;
    }
    if (node == NULL) {
        mc_client_model_graph graph = tile_renderer_graph(renderer);
        if (map->capacity == 0 || map->size >= map->threshold) {
            const size_t capacity = map->capacity == 0 ? 16 : map->capacity * 2;
            if (capacity < map->capacity || capacity > SIZE_MAX / sizeof(*map->buckets))
                return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_LIMIT,
                                      "AG native Map capacity bound", true);
            tile_renderer_spawner_node **buckets =
                mc_client_model_graph_allocate(&graph, capacity * sizeof(*buckets), "ag", 63);
            if (buckets == NULL)
                goto model_failed;
            for (size_t i = 0; i < map->capacity; ++i) {
                tile_renderer_spawner_node *p = map->buckets[i];
                while (p != NULL) {
                    tile_renderer_spawner_node *next = p->next;
                    const size_t bucket = name_bucket(p->key, capacity);
                    p->next = buckets[bucket];
                    buckets[bucket] = p;
                    p = next;
                }
            }
            /* Old backing remains charged in the real renderer graph's
             * native allocation ledger until terminal graph retirement. */
            map->buckets = buckets;
            map->capacity = capacity;
            map->threshold = capacity - capacity / 4;
        }
        node = mc_client_model_graph_allocate(&graph, sizeof(*node), "ag", 63);
        if (node == NULL)
            goto model_failed;
        c->error.owner.tile_entity = mc_tile_entity_name_retain(c->names[2]);
        if (c->error.owner.tile_entity != MC_TILE_ENTITY_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                  "AG actual Map String key occurrence", true);
        node->key = c->names[2];
        const size_t bucket = name_bucket(node->key, map->capacity);
        node->next = map->buckets[bucket];
        map->buckets[bucket] = node;
        ++map->size;
        ++map->mod_count;
    }
    mc_entity *previous = node->value;
    node->value = c->factory_result; /* Actual Map.put63, including a NULL value. */
    c->map_value_pending = NULL;
    mc_entity_release(previous);
    c->state.cache_put_returned = true;
    c->state.cache_entries = map->size;
    c->state.cache_capacity = map->capacity;
    return true;
model_failed:
    c->error.owner.model = renderer->failure;
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_MODEL_FAILED,
                          "AG actual source Map native storage", true);
}

bool tile_draw_spawner_cache(mc_client_tile_renderer_draw_call *c) {
    tile_renderer_spawner_map *map = c->renderer->spawner_map;
    if (!get_name(c, 0, 25))
        return false;
    if (map == NULL) {
        at(c, 28);
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/NullPointerException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE,
                              "actual AG null Map receiver", false);
    }
    at(c, 28);
    tile_renderer_spawner_node *node = find(map, c->names[0]);
    mc_entity *preview = node == NULL ? NULL : node->value;
    c->state.cache_hit = preview != NULL;
    c->state.cache_miss = preview == NULL;
    c->state.cache_entries = map->size;
    c->state.cache_capacity = map->capacity;
    if (preview == NULL) {
        if (!get_name(c, 1, 44))
            return false;
        mc_tile_entity_name_view name;
        c->error.owner.tile_entity = mc_tile_entity_name_get_view(c->names[1], &name);
        if (c->error.owner.tile_entity != MC_TILE_ENTITY_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                  "AG actual factory String operand", true);
        at(c, 48);
        ++c->state.factory_calls;
        const mc_entity_factory_status status =
            mc_entity_factory_named(c->dispatcher->classes->models->entities,
                                    c->dispatcher->classes->world_classes->registry, name.text,
                                    NULL, &c->factory_result, &c->factory);
        c->error.factory = &c->factory;
        if (status != MC_ENTITY_FACTORY_OK) {
            c->error.child = (mc_client_graphics_site){"jc", "a", "(Ljava/lang/String;Lfd;)Lsn;",
                                                       (int32_t)c->factory.bytecode};
            c->error.source_exception = c->factory.source_exception;
            c->error.exception_class = c->factory.exception_class;
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_FACTORY_FAILED, c->factory.operation,
                                  c->factory.native_policy);
        }
        c->state.factory_returned = true;
        preview = c->factory_result;
        tile_renderer_spawner_map *put_map = c->renderer->spawner_map; /* Actual getfield54. */
        if (!get_name(c, 2, 58))
            return false;
        if (put_map == NULL) {
            at(c, 63);
            c->error.source_exception = true;
            c->error.exception_class = "java/lang/NullPointerException";
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE,
                                  "actual AG null Map put receiver", false);
        }
        if (!map_put(c, put_map))
            return false;
    }
    if (preview != NULL) {
        if (mc_entity_retain(preview) != MC_ENTITY_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ENTITY_FAILED,
                                  "AG actual preview local occurrence", true);
        c->preview = preview;
        c->state.preview_identity = (uintptr_t)preview;
        mc_entity_snapshot state;
        c->error.owner.entity_status = mc_entity_get_snapshot(preview, &state);
        if (c->error.owner.entity_status != MC_ENTITY_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ENTITY_FAILED,
                                  "AG actual preview class observation", true);
        c->state.preview_class = state.class_literal;
    }
    return true;
}

static bool narrow(mc_client_tile_renderer_draw_call *c, double value, int32_t pc, float *out) {
    at(c, pc);
    if (!tile_draw_fp(c, pc))
        return false;
    *out = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value)));
    return true;
}

static bool preview_draw(mc_client_tile_renderer_draw_call *c) {
    mc_tile_entity_snapshot tile;
    at(c, 77);
    if (!tile_draw_snapshot(c, &tile, 77))
        return false;
    at(c, 80);
    c->error.entity = &c->entity;
    if (mc_entity_set_world(c->preview, tile.world, &c->entity) != MC_ENTITY_OK)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ENTITY_FAILED, c->entity.operation, true);
    c->state.world_rebound = true;
    c->state.preview_world_identity = (uintptr_t)tile.world;
    at(c, 91);
    if (!tile_draw_display(c, mc_client_display_source_translate(c->services.display, 0.0f, 0.4f,
                                                                 0.0f, &c->error.display)))
        return false;
    /* CY.c95, CY.b99 and CY.c103 are three fresh source field reads. */
    if (!tile_draw_snapshot(c, &tile, 95))
        return false;
    double previous;
    memcpy(&previous, &tile.spawner_previous_rotation_bits, sizeof(previous));
    if (!tile_draw_snapshot(c, &tile, 99))
        return false;
    double current;
    memcpy(&current, &tile.spawner_rotation_bits, sizeof(current));
    if (!tile_draw_snapshot(c, &tile, 103))
        return false;
    double fresh_previous;
    memcpy(&fresh_previous, &tile.spawner_previous_rotation_bits, sizeof(fresh_previous));
    mc_client_interpolation_error math = {0};
    if (!tile_draw_fp(c, 106))
        return false;
    if (!mc_client_interpolation_double_nan_admit(current, fresh_previous, 106, &math))
        goto numeric_failed;
    const double difference = mc_client_interpolation_dsub(current, fresh_previous);
    if (!tile_draw_fp(c, 109))
        return false;
    const double widened = mc_client_interpolation_widen(c->state.partial);
    if (!tile_draw_fp(c, 110))
        return false;
    if (!mc_client_interpolation_double_nan_admit(difference, widened, 110, &math))
        goto numeric_failed;
    const double scaled = mc_client_interpolation_dmul(difference, widened);
    if (!tile_draw_fp(c, 111))
        return false;
    /* The ADD's left operand is the earlier c95, not the c103 SUB operand. */
    if (!mc_client_interpolation_double_nan_admit(previous, scaled, 111, &math))
        goto numeric_failed;
    const double rotation = mc_client_interpolation_dadd(previous, scaled);
    if (!narrow(c, rotation, 112, &c->state.rotation) || !tile_draw_fp(c, 115))
        return false;
    c->state.rotation = mc_client_interpolation_fmul(c->state.rotation, 10.0f);
    at(c, 119);
    if (!tile_draw_display(c,
                           mc_client_display_source_rotate(c->services.display, c->state.rotation,
                                                           0.0f, 1.0f, 0.0f, &c->error.display)))
        return false;
    at(c, 127);
    if (!tile_draw_display(c, mc_client_display_source_rotate(c->services.display, -30.0f, 1.0f,
                                                              0.0f, 0.0f, &c->error.display)))
        return false;
    at(c, 134);
    if (!tile_draw_display(c, mc_client_display_source_translate(c->services.display, 0.0f, -0.4f,
                                                                 0.0f, &c->error.display)))
        return false;
    at(c, 143);
    if (!tile_draw_display(c, mc_client_display_source_scale(c->services.display, 0.4375, 0.4375,
                                                             0.4375, &c->error.display)))
        return false;
    at(c, 155);
    c->error.pose = &c->pose;
    if (mc_entity_source_position_rotation(c->preview, c->state.x, c->state.y, c->state.z, 0.0f,
                                           0.0f, &c->pose) != MC_ENTITY_OK) {
        c->error.source_exception = c->pose.source_null;
        if (c->pose.source_null)
            c->error.exception_class = "java/lang/NullPointerException";
        c->error.child =
            (mc_client_graphics_site){"sn", "c", "(DDDFF)V", (int32_t)c->pose.bytecode};
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ENTITY_FAILED, c->pose.operation,
                              c->pose.native_policy);
    }
    c->state.pose_returned = true;
    at(c, 158);
    mc_client_render_manager_classes *classes = c->dispatcher->classes->models->th;
    if (classes == NULL)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                              "AG reached TH defining namespace is absent", true);
    c->error.manager_owner = &c->manager_owner;
    if (mc_client_render_manager_consumer_attach(classes, &c->manager_consumer,
                                                 &c->manager_owner) != MC_CLIENT_MANAGER_OK)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_MANAGER_FAILED,
                              "AG actual TH defining namespace occurrence", true);
    mc_client_render_manager *manager = NULL;
    if (mc_client_render_manager_static_get(classes, &manager, &c->manager_owner) !=
        MC_CLIENT_MANAGER_OK)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_MANAGER_FAILED,
                              "AG actual TH singleton active use", true);
    at(c, 169);
    const mc_client_manager_status status = mc_client_render_manager_coordinates_draw_begin(
        manager, c->preview, 0.0, 0.0, 0.0, 0.0f, c->state.partial, &c->services,
        &c->limits.manager, &c->manager_call, &c->manager_error);
    tile_draw_manager_receipt(c);
    if (status != MC_CLIENT_MANAGER_OK) {
        c->error.manager = &c->manager_error;
        c->error.source_exception = c->manager_error.source_exception;
        c->error.exception_class = c->manager_error.exception_class;
        c->error.child = c->manager_error.site;
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_MANAGER_FAILED, c->manager_error.operation,
                              c->manager_error.native_policy);
    }
    if (mc_client_render_manager_draw_retire(&c->manager_call, &c->manager_cleanup) !=
        MC_CLIENT_MANAGER_OK) {
        c->error.manager = &c->manager_cleanup;
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_MANAGER_FAILED,
                              "AG completed TH call checked retirement", true);
    }
    c->state.manager_retired = true;
    return true;
numeric_failed:
    at(c, math.pc);
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_UNSUPPORTED_NUMERIC,
                          "AG actual rotation unmeasured signaling NaN pair", true);
}

bool tile_draw_spawner(mc_client_tile_renderer_draw_call *c) {
    tile_draw_at(c, "ag", "(Low;DDDF)V", 2);
    mc_tile_entity_snapshot tile;
    if (!tile_draw_snapshot(c, &tile, 2))
        return false;
    if (tile.kind != MC_TILE_ENTITY_SPAWNER) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/ClassCastException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                              "AG actual bridge CY checkcast", false);
    }
    c->state.bridge_cast_returned = true;
    at(c, 0);
    if (!tile_draw_graphics(c) ||
        !tile_draw_display(c,
                           mc_client_display_source_push(c->services.display, &c->error.display)))
        return false;
    c->state.matrix_pushed = true;
    float x, y, z;
    if (!narrow(c, c->state.x, 4, &x) || !tile_draw_fp(c, 7))
        return false;
    x = mc_client_interpolation_fadd(x, 0.5f);
    if (!narrow(c, c->state.y, 10, &y) || !narrow(c, c->state.z, 13, &z) || !tile_draw_fp(c, 16))
        return false;
    z = mc_client_interpolation_fadd(z, 0.5f);
    at(c, 17);
    if (!tile_draw_display(c, mc_client_display_source_translate(c->services.display, x, y, z,
                                                                 &c->error.display)) ||
        !tile_draw_spawner_cache(c))
        return false;
    if (c->preview != NULL && !preview_draw(c))
        return false;
    at(c, 172);
    if (!tile_draw_display(c, mc_client_display_source_pop(c->services.display, &c->error.display)))
        return false;
    c->state.matrix_popped = true;
    at(c, 175);
    return true;
}
