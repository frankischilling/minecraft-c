#include "rail_connections_internal.h"
#include "wrapper_transport_internal.h"

#include "../../../../block/registry/registry_internal.h"
#include "../../../../block/registry/runtime_internal.h"
#include "../../collision/blocks_internal.h"
#include "../mutation_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct rail_coordinate {
    int32_t x, y, z; /* The three immutable wf/oa fields, not interned values. */
} rail_coordinate;

typedef struct rail_list {
    rail_coordinate **items;
    size_t count, capacity;
} rail_list;

struct mc_rail_connections {
    mc_registry_classes *classes;       /* Defining code owner; no source field. */
    mc_world_runtime *allocation_world; /* Native budget owner, not source b. */
    const mc_block *outer;              /* final vo/nq.a; nullable and never dereferenced. */
    mc_world_runtime *world;            /* vo/nq.b */
    int32_t x, y, z;                    /* c/d/e */
    bool flagged;         /* final f, read from the actually selected rail, not outer. */
    rail_list *neighbors; /* g, an independently allocated actual List. */
};

typedef enum rail_method {
    RAIL_CONSTRUCTOR,
    RAIL_SHAPE_LIST,
    RAIL_PRUNE,
    RAIL_THREE_LEVELS,
    RAIL_RESOLVE,
    RAIL_CONTAINS_HELPER,
    RAIL_CONTAINS_XZ,
    RAIL_DEGREE,
    RAIL_ADMIT,
    RAIL_RECONNECT,
    RAIL_CONNECTABLE,
    RAIL_RESHAPE,
    RAIL_DEGREE_ACCESSOR
} rail_method;

typedef struct rail_frame {
    mc_registry_classes *classes;
    mc_world_runtime *world;
    mc_rail_connections_error *error;
    rail_method method;
} rail_frame;

static const char *rail_class(const rail_frame *frame) {
    return frame->classes->options.side == MC_REGISTRY_CLIENT ? "vo" : "nq";
}

static const char *rail_method_name(const rail_frame *frame) {
    const bool client = frame->classes->options.side == MC_REGISTRY_CLIENT;
    switch (frame->method) {
        case RAIL_CONSTRUCTOR:
            return client ? "<init>(Lpc;Lfd;III)V" : "<init>(Ljj;Ldj;III)V";
        case RAIL_SHAPE_LIST:
            return "a(I)V";
        case RAIL_PRUNE:
            return "a()V";
        case RAIL_THREE_LEVELS:
            return "a(III)Z";
        case RAIL_RESOLVE:
            return client ? "a(Lwf;)Lvo;" : "a(Loa;)Lnq;";
        case RAIL_CONTAINS_HELPER:
            return client ? "b(Lvo;)Z" : "b(Lnq;)Z";
        case RAIL_CONTAINS_XZ:
            return "b(III)Z";
        case RAIL_DEGREE:
            return "b()I";
        case RAIL_ADMIT:
            return client ? "c(Lvo;)Z" : "c(Lnq;)Z";
        case RAIL_RECONNECT:
            return client ? "d(Lvo;)V" : "d(Lnq;)V";
        case RAIL_CONNECTABLE:
            return "c(III)Z";
        case RAIL_RESHAPE:
            return "a(ZZ)V";
        case RAIL_DEGREE_ACCESSOR:
            return client ? "a(Lvo;)I" : "a(Lnq;)I";
    }
    return NULL;
}

static void rail_at(const rail_frame *frame, unsigned int pc) {
    frame->error->connection =
        (mc_rail_connection_site){true, rail_class(frame), rail_method_name(frame), pc};
    frame->error->selected |= MC_WORLD_RAIL_SITE_CONNECTION;
    frame->error->mutation.hook_class = rail_class(frame);
    frame->error->mutation.hook_method = rail_method_name(frame);
    frame->error->mutation.hook_bytecode = pc;
}

static void rail_cause(const rail_frame *frame) {
    mc_rail_connections_error *const error = frame->error;
    if ((error->selected & MC_WORLD_RAIL_SITE_CONNECTION) != 0 &&
        (error->selected & MC_WORLD_RAIL_SITE_CONNECTION_CAUSE) == 0) {
        error->cause_connection = error->connection;
        error->selected |= MC_WORLD_RAIL_SITE_CONNECTION_CAUSE;
        error->origin |= MC_WORLD_RAIL_SITE_CONNECTION;
    }
    if ((error->selected & MC_WORLD_RAIL_SITE_PREDICATE) != 0 &&
        (error->selected & MC_WORLD_RAIL_SITE_PREDICATE_CAUSE) == 0) {
        error->selected |= MC_WORLD_RAIL_SITE_PREDICATE_CAUSE;
        error->origin |= MC_WORLD_RAIL_SITE_PREDICATE;
    }
    if ((error->selected & MC_WORLD_RAIL_SITE_ACCESSOR) != 0 &&
        (error->selected & MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE) == 0) {
        error->selected |= MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE;
        error->origin |= MC_WORLD_RAIL_SITE_ACCESSOR;
    }
}

static bool rail_fail(const rail_frame *frame, unsigned int pc, mc_world_storage_status status,
                      const char *operation) {
    rail_at(frame, pc);
    rail_cause(frame);
    return mc_world_mutation_fail(frame->world, &frame->error->mutation, status, operation);
}

static bool rail_storage_fail(const rail_frame *frame, mc_world_storage_error cause) {
    rail_cause(frame);
    mc_world_mutation_error *const error = &frame->error->mutation;
    error->status = cause.status;
    error->operation = cause.operation;
    error->cause = cause;
    if (frame->world != NULL)
        frame->world->last_error = cause;
    return false;
}

/* The current caller and deepest actual helper survive without allocating a
 * diagnostic stack. No source traversal or synchronous notification is capped.
 * The independent presence/accessor frames remain intact. */
static bool rail_nested(const rail_frame *frame, unsigned int pc) {
    mc_rail_connections_error *const error = frame->error;
    rail_cause(frame);
    if (error->mutation.cause_hook_class == NULL) {
        error->mutation.cause_hook_class = error->mutation.hook_class;
        error->mutation.cause_hook_method = error->mutation.hook_method;
        error->mutation.cause_hook_bytecode = error->mutation.hook_bytecode;
    }
    rail_at(frame, pc);
    return false;
}

static bool rail_registry_fail(const rail_frame *frame, mc_registry_status status,
                               const mc_registry_error *cause, const char *operation) {
    rail_cause(frame);
    mc_world_mutation_error *const error = &frame->error->mutation;
    error->registry = *cause;
    if (frame->world != NULL) {
        error->status =
            mc_world_runtime_registry_result(frame->world, status, cause, operation, &error->cause);
        error->operation = error->cause.operation;
    } else {
        error->status = mc_world_runtime_registry_status(status);
        error->operation = operation;
        error->cause = (mc_world_storage_error){error->status, operation, 0};
    }
    return false;
}

static bool rail_null(const rail_frame *frame, const char *operation) {
    mc_registry_error cause = mc_world_mutation_empty_registry();
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return rail_registry_fail(frame, cause.status, &cause, operation);
}

static bool rail_read(const rail_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                      bool metadata, int32_t *out) {
    mc_world_collision_error lower = mc_world_collision_empty_error();
    rail_at(frame, pc);
    if (mc_world_collision_block_value_held(frame->world, x, y, z, metadata, out, &lower) ==
        MC_WORLD_STORAGE_OK)
        return true;
    frame->error->mutation.nearby = lower.nearby;
    frame->error->mutation.rail_access = lower;
    frame->error->mutation.query = lower.nearby.query;
    frame->error->mutation.registry = lower.nearby.query.registry;
    frame->error->mutation.population = lower.nearby.query.population;
    return rail_storage_fail(frame, lower.nearby.query.cause);
}

static bool rail_write(const rail_frame *frame, unsigned int pc, const mc_rail_connections *self,
                       int32_t metadata) {
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    rail_at(frame, pc);
    if (mc_world_runtime_metadata_set(self->world, self->x, self->y, self->z, metadata, &lower) ==
        MC_WORLD_STORAGE_OK)
        return true;
    frame->error->mutation = lower; /* Complete lower mutation, not a status projection. */
    /* Actual typed public lower causes survive publication. No generic hook
     * class-name recognition reconstructs a connection frame on reentry. */
    return rail_nested(frame, pc);
}

static void *rail_allocate(const rail_frame *frame, unsigned int pc, size_t bytes) {
    rail_at(frame, pc);
    if (frame->world != NULL) {
        const size_t used = frame->world->construction.direct_allocation_bytes;
        const size_t limit = frame->world->construction_limits.max_direct_allocation_bytes;
        if (used > limit || bytes > limit - used) {
            rail_fail(frame, pc, MC_WORLD_STORAGE_LIMIT, "Rail private allocation budget");
            return NULL;
        }
    }
    void *const memory = calloc(1, bytes);
    if (memory == NULL) {
        rail_fail(frame, pc, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Rail private allocation");
        return NULL;
    }
    if (frame->world != NULL)
        frame->world->construction.direct_allocation_bytes += bytes;
    return memory;
}

static void rail_deallocate(mc_world_runtime *world, void *memory, size_t bytes) {
    if (memory == NULL)
        return;
    free(memory);
    if (world != NULL)
        world->construction.direct_allocation_bytes -= bytes;
}

static rail_coordinate *rail_coordinate_new(const rail_frame *frame, unsigned int pc, int32_t x,
                                            int32_t y, int32_t z) {
    rail_coordinate *const coordinate = rail_allocate(frame, pc, sizeof(*coordinate));
    if (coordinate != NULL) {
        /* wf/oa.<init> stores a6, b11, c16 and returns19; no failing child. */
        coordinate->x = x;
        coordinate->y = y;
        coordinate->z = z;
    }
    return coordinate;
}

static void rail_list_clear(mc_rail_connections *self) {
    for (size_t i = 0; i < self->neighbors->count; ++i)
        rail_deallocate(self->allocation_world, self->neighbors->items[i],
                        sizeof(*self->neighbors->items[i]));
    self->neighbors->count = 0;
}

static bool rail_append(const rail_frame *frame, unsigned int pc, mc_rail_connections *self,
                        rail_coordinate *coordinate) {
    rail_at(frame, pc);
    rail_list *const list = self->neighbors;
    if (list->count == (size_t)INT32_MAX)
        return rail_fail(frame, pc, MC_WORLD_STORAGE_LIMIT, "Rail List signed size");
    if (list->count == list->capacity) {
        const size_t capacity = list->capacity == 0                       ? 4u
                                : list->capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX
                                                                          : list->capacity * 2u;
        if (capacity <= list->count || capacity > SIZE_MAX / sizeof(*list->items))
            return rail_fail(frame, pc, MC_WORLD_STORAGE_LIMIT, "Rail List capacity");
        const size_t bytes = capacity * sizeof(*list->items);
        rail_coordinate **const items = rail_allocate(frame, pc, bytes);
        if (items == NULL)
            return false;
        if (list->count != 0)
            memcpy(items, list->items, list->count * sizeof(*items));
        rail_coordinate **const old = list->items;
        const size_t old_bytes = list->capacity * sizeof(*old);
        list->items = items;
        list->capacity = capacity;
        rail_deallocate(self->allocation_world, old, old_bytes);
    }
    list->items[list->count++] = coordinate;
    return true;
}

void mc_rail_connections_destroy(mc_rail_connections *self) {
    if (self == NULL)
        return;
    mc_world_runtime *const world = self->allocation_world;
    if (self->neighbors != NULL) {
        rail_list_clear(self);
        rail_deallocate(world, self->neighbors->items,
                        self->neighbors->capacity * sizeof(*self->neighbors->items));
        rail_deallocate(world, self->neighbors, sizeof(*self->neighbors));
    }
    rail_deallocate(world, self, sizeof(*self));
}

static bool rail_active_use(const rail_frame *frame, const char *operation) {
    mc_registry_error cause = mc_world_mutation_empty_registry();
    if (frame->classes->entered == UINT_MAX) {
        cause.status = MC_REGISTRY_LIMIT;
        cause.message = "Rail declaring-class entry count";
        return rail_registry_fail(frame, cause.status, &cause, operation);
    }
    ++frame->classes->entered;
    const mc_registry_status status =
        mc_registry_block_class_initialize(frame->classes, MC_BLOCK_RAIL);
    --frame->classes->entered;
    if (status == MC_REGISTRY_OK)
        return true;
    cause = frame->classes->last_error;
    return rail_registry_fail(frame, status, &cause, operation);
}

/* Predicate entry is independent of its current vo/nq invocation site. An
 * active-use failure leaves entered=false; a body failure records its real PC.
 * Static and bn reads stay separate and late across each preceding operation. */
static void rail_predicate_at(const rail_frame *frame, bool id_only, bool entered,
                              unsigned int pc) {
    const bool client = frame->classes->options.side == MC_REGISTRY_CLIENT;
    const char *const name =
        id_only ? (client ? "d(I)Z" : "c(I)Z") : (client ? "h(Lfd;III)Z" : "g(Ldj;III)Z");
    frame->error->predicate = (mc_rail_connection_site){entered, client ? "pc" : "jj", name, pc};
    frame->error->selected |= MC_WORLD_RAIL_SITE_PREDICATE;
    frame->error->mutation.hook_class = client ? "pc" : "jj";
    frame->error->mutation.hook_method = name;
    frame->error->mutation.hook_bytecode = pc;
}

static bool rail_id_body(const rail_frame *frame, bool id_only, int32_t id, bool *out) {
    static const char *const names[3] = {"aH", "U", "V"};
    static const unsigned int presence_static[3] = {11, 22, 33};
    static const unsigned int presence_field[3] = {14, 25, 36};
    static const unsigned int id_static[3] = {1, 11, 21};
    static const unsigned int id_field[3] = {4, 14, 24};
    for (size_t i = 0; i < 3; ++i) {
        rail_predicate_at(frame, id_only, true, id_only ? id_static[i] : presence_static[i]);
        mc_registry_error registry = mc_world_mutation_empty_registry();
        const mc_block *selected;
        const mc_registry_status status =
            mc_registry_block_static_get(frame->classes, names[i], &selected, &registry);
        if (status != MC_REGISTRY_OK)
            return rail_registry_fail(frame, status, &registry, "Rail live singleton active use");
        rail_predicate_at(frame, id_only, true, id_only ? id_field[i] : presence_field[i]);
        if (selected == NULL)
            return rail_null(frame, "Rail singleton bn receiver");
        if (id == selected->fields.bn) {
            *out = true;
            return true;
        }
    }
    *out = false;
    return true;
}

static bool rail_presence(const rail_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                          bool *out) {
    rail_at(frame, pc);
    rail_predicate_at(frame, false, false, 0);
    if (!rail_active_use(frame, "Rail presence declaring-class active use"))
        return rail_nested(frame, pc);
    rail_predicate_at(frame, false, true, 4);
    int32_t id;
    mc_world_collision_error lower = mc_world_collision_empty_error();
    if (mc_world_collision_block_value_held(frame->world, x, y, z, false, &id, &lower) !=
        MC_WORLD_STORAGE_OK) {
        frame->error->mutation.nearby = lower.nearby;
        frame->error->mutation.rail_access = lower;
        frame->error->mutation.query = lower.nearby.query;
        frame->error->mutation.registry = lower.nearby.query.registry;
        frame->error->mutation.population = lower.nearby.query.population;
        rail_storage_fail(frame, lower.nearby.query.cause);
        return rail_nested(frame, pc);
    }
    if (!rail_id_body(frame, false, id, out))
        return rail_nested(frame, pc);
    frame->error->predicate = (mc_rail_connection_site){0};
    frame->error->selected &=
        ~(uint32_t)(MC_WORLD_RAIL_SITE_PREDICATE | MC_WORLD_RAIL_SITE_PREDICATE_CAUSE);
    frame->error->origin &= ~(uint32_t)MC_WORLD_RAIL_SITE_PREDICATE;
    rail_at(frame, pc);
    return true;
}

static bool rail_shape_list(mc_rail_connections *self, int32_t shape,
                            mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, self->world, error, RAIL_SHAPE_LIST};

    /* Exact a(I) branches. The table is only immutable literal offsets and NEW
     * and List.add PCs; it is not a graph or a World-derived shape cache. */
    static const struct {
        int32_t offset[2][3];
        unsigned int allocation[2], append[2];
    } rows[10] = {{{{0, 0, -1}, {0, 0, 1}}, {17, 48}, {38, 69}},
                  {{{-1, 0, 0}, {1, 0, 0}}, {87, 118}, {108, 139}},
                  {{{-1, 0, 0}, {1, 1, 0}}, {157, 188}, {178, 211}},
                  {{{-1, 1, 0}, {1, 0, 0}}, {229, 262}, {252, 283}},
                  {{{0, 1, -1}, {0, 0, 1}}, {301, 334}, {324, 355}},
                  {{{0, 0, -1}, {0, 1, 1}}, {373, 404}, {394, 427}},
                  {{{1, 0, 0}, {0, 0, 1}}, {446, 477}, {467, 498}},
                  {{{-1, 0, 0}, {0, 0, 1}}, {517, 548}, {538, 569}},
                  {{{-1, 0, 0}, {0, 0, -1}}, {588, 619}, {609, 640}},
                  {{{1, 0, 0}, {0, 0, -1}}, {659, 690}, {680, 711}}};

    rail_at(&frame, 4);
    rail_list_clear(self);
    if (shape < 0 || shape > 9)
        return true; /* return717 with the actual newly empty List. */
    for (size_t i = 0; i < 2; ++i) {
        const int32_t x = mc_java_i32_add(self->x, rows[shape].offset[i][0]);
        const int32_t y = mc_java_i32_add(self->y, rows[shape].offset[i][1]);
        const int32_t z = mc_java_i32_add(self->z, rows[shape].offset[i][2]);
        rail_coordinate *const coordinate =
            rail_coordinate_new(&frame, rows[shape].allocation[i], x, y, z);
        if (coordinate == NULL)
            return false;
        if (!rail_append(&frame, rows[shape].append[i], self, coordinate)) {
            rail_deallocate(self->allocation_world, coordinate, sizeof(*coordinate));
            return false;
        }
    }
    return true;
}

static bool rail_constructor(mc_rail_connections *self, const mc_block *outer,
                             mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                             mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, world, error, RAIL_CONSTRUCTOR};
    rail_at(&frame, 2);
    self->outer = outer;
    self->neighbors = rail_allocate(&frame, 10, sizeof(*self->neighbors));
    if (self->neighbors == NULL)
        return false;
    self->world = world; /* putfield22 */
    self->x = x;         /* 27 */
    self->y = y;         /* 33 */
    self->z = z;         /* 39 */
    int32_t id, metadata;
    if (!rail_read(&frame, 48, x, y, z, false, &id) ||
        !rail_read(&frame, 59, x, y, z, true, &metadata))
        return false;
    mc_registry_block_m_view array = {0};
    mc_registry_error registry = mc_world_mutation_empty_registry();
    rail_at(&frame, 64);
    mc_registry_status status = mc_registry_block_m_get(self->classes, &array, &registry);
    if (status != MC_REGISTRY_OK)
        return rail_registry_fail(&frame, status, &registry, "Rail constructor live Block.m");
    const mc_block *selected;
    rail_at(&frame, 69);
    status = mc_registry_block_m_at(&array, id, &selected, &registry);
    if (status != MC_REGISTRY_OK)
        return rail_registry_fail(&frame, status, &registry, "Rail constructor captured m element");
    rail_at(&frame, 70);
    if (selected != NULL &&
        (selected->classes != self->classes ||
         (selected->kind != MC_BLOCK_RAIL && selected->kind != MC_BLOCK_DETECTOR_RAIL)))
        return rail_fail(&frame, 70, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Rail constructor checkcast pc/jj");
    rail_at(&frame, 73);
    const bool client = self->classes->options.side == MC_REGISTRY_CLIENT;
    error->accessor =
        (mc_rail_connection_site){false, client ? "pc" : "jj", client ? "a(Lpc;)Z" : "a(Ljj;)Z", 0};
    error->selected |= MC_WORLD_RAIL_SITE_ACCESSOR;
    if (!rail_active_use(&frame, "Rail flag accessor declaring-class active use"))
        return false;
    error->accessor.entered = true;
    error->accessor.bytecode = 1;
    if (selected == NULL) {
        error->mutation.cause_hook_class = error->accessor.class_name;
        error->mutation.cause_hook_method = error->accessor.method;
        error->mutation.cause_hook_bytecode = 1;
        return rail_null(&frame, "Rail flag accessor getfield receiver");
    }
    self->flagged = selected->subtype.pc.a; /* putfield81 or96 */
    error->accessor = (mc_rail_connection_site){0};
    error->selected &= ~(uint32_t)(MC_WORLD_RAIL_SITE_ACCESSOR | MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE);
    error->origin &= ~(uint32_t)MC_WORLD_RAIL_SITE_ACCESSOR;
    if (self->flagged)
        metadata &= -9; /* iand88, not a three-bit mask. */
    if (!rail_shape_list(self, metadata, error))
        return rail_nested(&frame, 102);
    return true;
}

static bool rail_create_child(const rail_frame *frame, unsigned int allocation_pc,
                              unsigned int constructor_pc, const mc_rail_connections *parent,
                              int32_t x, int32_t y, int32_t z, mc_rail_connections **out) {
    mc_rail_connections *const result = rail_allocate(frame, allocation_pc, sizeof(*result));
    if (result == NULL)
        return false;
    result->classes = parent->classes;
    result->allocation_world = parent->allocation_world;
    if (!rail_constructor(result, parent->outer, parent->world, x, y, z, frame->error)) {
        mc_rail_connections_destroy(result);
        return rail_nested(frame, constructor_pc);
    }
    *out = result;
    return true;
}

static bool rail_three_levels(mc_rail_connections *self, int32_t x, int32_t y, int32_t z, bool *out,
                              mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, self->world, error, RAIL_THREE_LEVELS};
    bool present;
    if (!rail_presence(&frame, 7, x, y, z, &present))
        return false;
    if (present) {
        *out = true;
        return true;
    }
    if (!rail_presence(&frame, 24, x, mc_java_i32_add(y, 1), z, &present))
        return false;
    if (present) {
        *out = true;
        return true;
    }
    if (!rail_presence(&frame, 41, x, mc_java_i32_sub(y, 1), z, &present))
        return false;
    *out = present;
    return true;
}

static bool rail_resolve(mc_rail_connections *self, const rail_coordinate *coordinate,
                         mc_rail_connections **out, mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, self->world, error, RAIL_RESOLVE};
    bool present;
    if (!rail_presence(&frame, 16, coordinate->x, coordinate->y, coordinate->z, &present))
        return false;
    if (present)
        return rail_create_child(&frame, 22, 46, self, coordinate->x, coordinate->y, coordinate->z,
                                 out);
    if (!rail_presence(&frame, 68, coordinate->x, mc_java_i32_add(coordinate->y, 1), coordinate->z,
                       &present))
        return false;
    if (present)
        return rail_create_child(&frame, 74, 100, self, coordinate->x,
                                 mc_java_i32_add(coordinate->y, 1), coordinate->z, out);
    if (!rail_presence(&frame, 122, coordinate->x, mc_java_i32_sub(coordinate->y, 1), coordinate->z,
                       &present))
        return false;
    if (present)
        return rail_create_child(&frame, 128, 154, self, coordinate->x,
                                 mc_java_i32_sub(coordinate->y, 1), coordinate->z, out);
    *out = NULL;
    return true;
}

/* The private opaque List contains only its actual wf/oa occurrences. All
 * source size/get/field operations occur here, without foreign mutable List
 * controls or an invented Y comparison. Neither method invokes a child. */
static bool rail_contains_helper(mc_rail_connections *self, const mc_rail_connections *other,
                                 mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, self->world, error, RAIL_CONTAINS_HELPER};
    for (size_t i = 0; i < self->neighbors->count; ++i) { /* size7/get20 */
        rail_at(&frame, 30);
        const rail_coordinate *const coordinate = self->neighbors->items[i];
        if (coordinate->x == other->x && coordinate->z == other->z)
            return true;
    }
    return false;
}

static bool rail_contains_xz(mc_rail_connections *self, int32_t x, int32_t y, int32_t z,
                             mc_rail_connections_error *error) {
    (void)y;
    const rail_frame frame = {self->classes, self->world, error, RAIL_CONTAINS_XZ};
    for (size_t i = 0; i < self->neighbors->count; ++i) { /* size9/get23 */
        rail_at(&frame, 35);
        const rail_coordinate *const coordinate = self->neighbors->items[i];
        if (coordinate->x == x && coordinate->z == z)
            return true;
    }
    return false;
}

static bool rail_prune(mc_rail_connections *self, mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, self->world, error, RAIL_PRUNE};
    for (int32_t i = 0; (size_t)i < self->neighbors->count; i = mc_java_i32_add(i, 1)) {
        rail_at(&frame, 21);
        rail_coordinate *const previous = self->neighbors->items[i];
        mc_rail_connections *neighbor = NULL;
        if (!rail_resolve(self, previous, &neighbor, error))
            return rail_nested(&frame, 29);
        const bool connected = neighbor != NULL && rail_contains_helper(neighbor, self, error);
        if (!connected) {
            rail_at(&frame, 53);
            const size_t index = (size_t)i;
            memmove(self->neighbors->items + index, self->neighbors->items + index + 1,
                    (self->neighbors->count - index - 1) * sizeof(*self->neighbors->items));
            --self->neighbors->count;
            rail_deallocate(self->allocation_world, previous, sizeof(*previous));
            i = mc_java_i32_sub(i, 1); /* iinc50 then iinc92; -1 is never indexed. */
        } else {
            rail_coordinate *const replacement =
                rail_coordinate_new(&frame, 67, neighbor->x, neighbor->y, neighbor->z);
            if (replacement == NULL) {
                mc_rail_connections_destroy(neighbor);
                return false;
            }
            rail_at(&frame, 86);
            self->neighbors->items[i] = replacement;
            rail_deallocate(self->allocation_world, previous, sizeof(*previous));
        }
        mc_rail_connections_destroy(neighbor);
    }
    return true;
}

static bool rail_admit(mc_rail_connections *self, const mc_rail_connections *other,
                       mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, self->world, error, RAIL_ADMIT};
    if (rail_contains_helper(self, other, error))
        return true; /* existing connection9 */
    rail_at(&frame, 14);
    if (self->neighbors->count == 2)
        return false;
    rail_at(&frame, 29);
    if (self->neighbors->count == 0)
        return true;
    rail_at(&frame, 44);
    const rail_coordinate *const first = self->neighbors->items[0];
    rail_at(&frame, 54);
    if (other->y == self->y && first->y == self->y)
        return true; /* return76 */
    return true;     /* return78, deliberately no height rejection. */
}

static bool rail_connectable(mc_rail_connections *self, int32_t x, int32_t y, int32_t z, bool *out,
                             mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, self->world, error, RAIL_CONNECTABLE};
    rail_coordinate *const coordinate = rail_coordinate_new(&frame, 1, x, y, z);
    if (coordinate == NULL)
        return false;
    mc_rail_connections *neighbor = NULL;
    const bool resolved = rail_resolve(self, coordinate, &neighbor, error);
    rail_deallocate(self->allocation_world, coordinate, sizeof(*coordinate));
    if (!resolved)
        return rail_nested(&frame, 11);
    if (neighbor == NULL) {
        *out = false;
        return true;
    }
    if (!rail_prune(neighbor, error)) {
        mc_rail_connections_destroy(neighbor);
        return rail_nested(&frame, 25);
    }
    const bool admitted = rail_admit(neighbor, self, error);
    mc_rail_connections_destroy(neighbor);
    *out = admitted;
    return true;
}

static bool rail_slopes(const rail_frame *frame, mc_rail_connections *self, int32_t *shape,
                        unsigned int north_pc, unsigned int south_pc, unsigned int east_pc,
                        unsigned int west_pc) {
    bool present;
    if (*shape == 0) {
        if (!rail_presence(frame, north_pc, self->x, mc_java_i32_add(self->y, 1),
                           mc_java_i32_sub(self->z, 1), &present))
            return false;
        if (present)
            *shape = 4;
        /* Entered once on shape0; the second read is still reached after4. */
        if (!rail_presence(frame, south_pc, self->x, mc_java_i32_add(self->y, 1),
                           mc_java_i32_add(self->z, 1), &present))
            return false;
        if (present)
            *shape = 5;
    }
    if (*shape == 1) {
        if (!rail_presence(frame, east_pc, mc_java_i32_add(self->x, 1), mc_java_i32_add(self->y, 1),
                           self->z, &present))
            return false;
        if (present)
            *shape = 2;
        if (!rail_presence(frame, west_pc, mc_java_i32_sub(self->x, 1), mc_java_i32_add(self->y, 1),
                           self->z, &present))
            return false;
        if (present)
            *shape = 3;
    }
    return true;
}

static bool rail_reconnect(mc_rail_connections *self, const mc_rail_connections *other,
                           mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, self->world, error, RAIL_RECONNECT};
    rail_coordinate *const coordinate =
        rail_coordinate_new(&frame, 4, other->x, other->y, other->z);
    if (coordinate == NULL)
        return false;
    if (!rail_append(&frame, 23, self, coordinate)) {
        rail_deallocate(self->allocation_world, coordinate, sizeof(*coordinate));
        return false;
    }
    const bool north = rail_contains_xz(self, self->x, self->y, mc_java_i32_sub(self->z, 1), error);
    const bool south = rail_contains_xz(self, self->x, self->y, mc_java_i32_add(self->z, 1), error);
    const bool west = rail_contains_xz(self, mc_java_i32_sub(self->x, 1), self->y, self->z, error);
    const bool east = rail_contains_xz(self, mc_java_i32_add(self->x, 1), self->y, self->z, error);
    int32_t shape = -1;
    if (north || south)
        shape = 0;
    if (west || east)
        shape = 1;
    if (!self->flagged) {
        if (south && east && !north && !west)
            shape = 6;
        if (south && west && !north && !east)
            shape = 7;
        if (north && west && !south && !east)
            shape = 8;
        if (north && east && !south && !west)
            shape = 9;
    }
    if (!rail_slopes(&frame, self, &shape, 254, 283, 318, 347))
        return false;
    if (shape < 0)
        shape = 0;
    int32_t metadata = shape;
    if (self->flagged) {
        int32_t current;
        if (!rail_read(&frame, 391, self->x, self->y, self->z, true, &current))
            return false;
        metadata = (current & 8) | shape;
    }
    /* Always notifying, even equal; g is not rebuilt in d(helper). */
    return rail_write(&frame, 420, self, metadata);
}

static bool rail_reshape(mc_rail_connections *self, bool powered, bool force,
                         mc_rail_connections_error *error) {
    const rail_frame frame = {self->classes, self->world, error, RAIL_RESHAPE};
    bool north, south, west, east;
    if (!rail_connectable(self, self->x, self->y, mc_java_i32_sub(self->z, 1), &north, error))
        return rail_nested(&frame, 15);
    if (!rail_connectable(self, self->x, self->y, mc_java_i32_add(self->z, 1), &south, error))
        return rail_nested(&frame, 34);
    if (!rail_connectable(self, mc_java_i32_sub(self->x, 1), self->y, self->z, &west, error))
        return rail_nested(&frame, 54);
    if (!rail_connectable(self, mc_java_i32_add(self->x, 1), self->y, self->z, &east, error))
        return rail_nested(&frame, 74);
    int32_t shape = -1;
    if ((north || south) && !west && !east)
        shape = 0;
    if ((west || east) && !north && !south)
        shape = 1;
    if (!self->flagged) {
        if (south && east && !north && !west)
            shape = 6;
        if (south && west && !north && !east)
            shape = 7;
        if (north && west && !south && !east)
            shape = 8;
        if (north && east && !south && !west)
            shape = 9;
    }
    if (shape == -1) {
        if (north || south)
            shape = 0;
        if (west || east)
            shape = 1;
        if (!self->flagged) {
            if (powered) {
                if (south && east)
                    shape = 6;
                if (west && south)
                    shape = 7;
                if (east && north)
                    shape = 9;
                if (north && west)
                    shape = 8;
            } else {
                if (north && west)
                    shape = 8;
                if (east && north)
                    shape = 9;
                if (west && south)
                    shape = 7;
                if (south && east)
                    shape = 6;
            }
        }
    }
    if (!rail_slopes(&frame, self, &shape, 403, 432, 467, 496))
        return false;
    if (shape < 0)
        shape = 0;
    if (!rail_shape_list(self, shape, error))
        return rail_nested(&frame, 516);
    int32_t metadata = shape;
    if (self->flagged) {
        int32_t current;
        if (!rail_read(&frame, 546, self->x, self->y, self->z, true, &current))
            return false;
        metadata = (current & 8) | shape;
    }
    if (!force) {
        int32_t current;
        if (!rail_read(&frame, 577, self->x, self->y, self->z, true, &current))
            return false;
        if (current == metadata)
            return true; /* return677; rebuilt g remains. */
    }
    if (!rail_write(&frame, 603, self, metadata))
        return false;
    for (int32_t i = 0; (size_t)i < self->neighbors->count; i = mc_java_i32_add(i, 1)) {
        rail_at(&frame, 630);
        mc_rail_connections *neighbor = NULL;
        if (!rail_resolve(self, self->neighbors->items[i], &neighbor, error))
            return rail_nested(&frame, 638);
        if (neighbor == NULL)
            continue;
        if (!rail_prune(neighbor, error)) {
            mc_rail_connections_destroy(neighbor);
            return rail_nested(&frame, 653);
        }
        if (rail_admit(neighbor, self, error) && !rail_reconnect(neighbor, self, error)) {
            mc_rail_connections_destroy(neighbor);
            return rail_nested(&frame, 668);
        }
        mc_rail_connections_destroy(neighbor);
    }
    return true;
}

static mc_rail_connections_error rail_empty_error(void) {
    mc_rail_connections_error error = {0};
    error.mutation = mc_world_mutation_empty_error();
    return error;
}

static mc_world_storage_status rail_arguments(mc_rail_connections_error *error) {
    if (error != NULL) {
        *error = rail_empty_error();
        mc_world_mutation_fail(NULL, &error->mutation, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                               "Rail native entry arguments");
        mc_rail_connections_error_publish(error);
    }
    return MC_WORLD_STORAGE_INVALID_ARGUMENT;
}

static mc_world_storage_status rail_result(bool completed, mc_rail_connections_error *error) {
    if (!completed) {
        mc_rail_connections_error_publish(error);
        return error->mutation.status;
    }
    *error = rail_empty_error();
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_rail_connections_create(mc_registry_classes *classes,
                                                   const mc_block *outer, mc_world_runtime *world,
                                                   int32_t x, int32_t y, int32_t z,
                                                   mc_rail_connections **out,
                                                   mc_rail_connections_error *error) {
    if (classes == NULL || out == NULL || error == NULL)
        return rail_arguments(error);
    *error = rail_empty_error();
    const rail_frame frame = {classes, world, error, RAIL_CONSTRUCTOR};
    /* Caller owns its actual source NEW site; zero here is a native entry
     * allocation diagnostic, not an invented constructor allocation opcode. */
    mc_rail_connections *const result = rail_allocate(&frame, 0, sizeof(*result));
    if (result == NULL) {
        error->connection.entered = false;
        if ((error->selected & MC_WORLD_RAIL_SITE_CONNECTION_CAUSE) != 0)
            error->cause_connection.entered = false;
        return rail_result(false, error);
    }
    result->classes = classes;
    result->allocation_world = world;
    if (!rail_constructor(result, outer, world, x, y, z, error)) {
        mc_rail_connections_destroy(result);
        return rail_result(false, error);
    }
    *out = result;
    return rail_result(true, error);
}

mc_world_storage_status mc_rail_connections_reshape(mc_rail_connections *self, bool powered,
                                                    bool force, mc_rail_connections_error *error) {
    if (self == NULL || error == NULL)
        return rail_arguments(error);
    *error = rail_empty_error();
    return rail_result(rail_reshape(self, powered, force, error), error);
}

mc_world_storage_status mc_rail_connections_degree(mc_rail_connections *self, int32_t *out,
                                                   mc_rail_connections_error *error) {
    if (self == NULL || out == NULL || error == NULL)
        return rail_arguments(error);
    *error = rail_empty_error();
    const rail_frame frame = {self->classes, self->world, error, RAIL_DEGREE};
    static const int32_t offsets[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    static const unsigned int calls[4] = {17, 41, 65, 89};
    int32_t count = 0;
    for (size_t i = 0; i < 4; ++i) {
        bool present;
        if (!rail_three_levels(self, mc_java_i32_add(self->x, offsets[i][0]), self->y,
                               mc_java_i32_add(self->z, offsets[i][1]), &present, error)) {
            rail_nested(&frame, calls[i]);
            const rail_frame accessor = {self->classes, self->world, error, RAIL_DEGREE_ACCESSOR};
            rail_nested(&accessor, 1);
            return rail_result(false, error);
        }
        if (present)
            count = mc_java_i32_add(count, 1);
    }
    *out = count;
    return rail_result(true, error);
}

mc_world_storage_status mc_rail_present_held(mc_registry_classes *classes, mc_world_runtime *world,
                                             int32_t x, int32_t y, int32_t z, bool *out,
                                             mc_rail_connections_error *error) {
    if (classes == NULL || out == NULL || error == NULL)
        return rail_arguments(error);
    *error = rail_empty_error();
    const rail_frame frame = {classes, world, error, RAIL_THREE_LEVELS};
    bool value;
    const bool completed = rail_presence(&frame, 0, x, y, z, &value);
    /* This direct API has no vo/nq caller; retain only the actual predicate. */
    error->connection = (mc_rail_connection_site){0};
    error->cause_connection = (mc_rail_connection_site){0};
    error->selected &=
        ~(uint32_t)(MC_WORLD_RAIL_SITE_CONNECTION | MC_WORLD_RAIL_SITE_CONNECTION_CAUSE);
    error->origin &= ~(uint32_t)MC_WORLD_RAIL_SITE_CONNECTION;
    if (completed)
        *out = value;
    else {
        error->mutation.hook_class = error->predicate.class_name;
        error->mutation.hook_method = error->predicate.method;
        error->mutation.hook_bytecode = error->predicate.bytecode;
    }
    return rail_result(completed, error);
}

mc_world_storage_status mc_rail_is_id_held(mc_registry_classes *classes, int32_t id, bool *out,
                                           mc_rail_connections_error *error) {
    if (classes == NULL || out == NULL || error == NULL)
        return rail_arguments(error);
    *error = rail_empty_error();
    const rail_frame frame = {classes, NULL, error, RAIL_THREE_LEVELS};
    rail_predicate_at(&frame, true, false, 0);
    if (!rail_active_use(&frame, "Rail ID predicate declaring-class active use"))
        return rail_result(false, error);
    bool value;
    const bool completed = rail_id_body(&frame, true, id, &value);
    if (completed)
        *out = value;
    return rail_result(completed, error);
}
