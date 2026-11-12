#include "render_global_jo.h"

#include "../../entity/player/tick_math_internal.h"
#include "render_global_update_sort.h"
#include "source_chunk_renderer_internal.h"

#include <limits.h>
#include <string.h>

static bool fail(mc_client_render_global_error *e, mc_client_render_global_status status,
                 const char *operation, const char *method, const char *descriptor, unsigned int pc,
                 bool native) {
    mc_render_global_source_fail(e, status, operation, "jo", method, descriptor, pc, native);
    return false;
}

static double negate(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    bits ^= UINT64_C(0x8000000000000000);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

bool mc_render_global_jo_construct(mc_render_global_jo *j, const mc_entity *camera,
                                   mc_client_render_global_error *e) {
    if (camera == NULL)
        return fail(e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                    "actual JO camera coordinate receiver", "<init>", "(Lsn;)V", 6, false);
    mc_entity_snapshot fields;
    e->entity.status = mc_entity_get_snapshot(camera, &fields);
    if (e->entity.status != MC_ENTITY_OK)
        return fail(e, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "actual JO camera native fields",
                    "<init>", "(Lsn;)V", 6, true);
    j->a = negate(fields.base.x);
    j->b = negate(fields.base.y);
    j->c = negate(fields.base.z);
    return true;
}

bool mc_render_global_jo_compare(const mc_render_global_jo *j, const mc_client_source_dk *left,
                                 const mc_client_source_dk *right, int *out,
                                 mc_client_render_global_error *e) {
    if (left == NULL)
        return fail(e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual JO left DK.q receiver", "a",
                    "(Ldk;Ldk;)I", 1, false);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(e, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                    "JO reached strict floating environment", "a", "(Ldk;Ldk;)I", 9, true);
    const double x = at_add((double)left->fields.q, j->a);
    const double y = at_add((double)left->fields.r, j->b);
    const double z = at_add((double)left->fields.s, j->c);
    if (right == NULL)
        return fail(e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual JO right DK.q receiver", "a",
                    "(Ldk;Ldk;)I", 36, false);
    const double xx = at_add((double)right->fields.q, j->a);
    const double yy = at_add((double)right->fields.r, j->b);
    const double zz = at_add((double)right->fields.s, j->c);
    const double a = at_add(at_add(at_mul(x, x), at_mul(y, y)), at_mul(z, z));
    const double b = at_add(at_add(at_mul(xx, xx), at_mul(yy, yy)), at_mul(zz, zz));
    *out = mc_java_double_to_i32(at_mul(at_sub(a, b), 1024.0));
    return true;
}

typedef struct compare_context {
    mc_render_global_jo fields;
    mc_client_render_global_error *error;
} compare_context;

static bool compare(void *context, mc_client_source_dk *left, mc_client_source_dk *right,
                    int *out) {
    compare_context *c = context;
    return mc_render_global_jo_compare(&c->fields, left, right, out, c->error);
}

bool mc_render_global_sort_at_entered(mc_client_render_global *n, const mc_entity *camera,
                                      mc_client_graphics_site caller,
                                      mc_client_render_global_error *e) {
    if (e == NULL)
        return false;
    if (n == NULL || !n->active) {
        mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                                     "JO sort requires the admitted actual N guard", NULL, NULL,
                                     NULL, 0, true);
        return false;
    }
    /* Source evaluates getfield n, then NEW JO/constructor, then Arrays.sort.
     * Even a NULL array must not skip the reached comparator constructor. */
    mc_client_source_dk **array = n->fields.n;
    compare_context c = {.error = e};
    if (!mc_render_global_jo_construct(&c.fields, camera, e)) {
        e->parent = caller;
        e->parent.pc -= 3; /* Actual JO constructor296/688 precedes sort299/691. */
        return false;
    }
    if (array == NULL) {
        mc_render_global_source_fail(
            e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual Arrays.sort DK array",
            caller.class_name, caller.method, caller.descriptor, (unsigned int)caller.pc, false);
        return false;
    }
    const bool ok = mc_render_global_update_sort(array, n->n_length, &c, compare, e);
    if (!ok) {
        if (e->source_class != NULL && strcmp(e->source_class, "n") == 0 &&
            e->source_descriptor != NULL && strcmp(e->source_descriptor, "(Lls;Z)Z") == 0 &&
            e->source_pc == 411) {
            /* The shared JDK kernel's own failure belongs to THIS Arrays.sort,
             * while a comparator's original JO failure remains untouched. */
            e->source_class = caller.class_name;
            e->source_method = caller.method;
            e->source_descriptor = caller.descriptor;
            e->source_pc = (unsigned int)caller.pc;
        } else
            e->parent = caller;
    }
    return ok;
}

bool mc_render_global_sort_entered(mc_client_render_global *n, const mc_entity *camera,
                                   mc_client_render_global_error *e) {
    return mc_render_global_sort_at_entered(
        n, camera, (mc_client_graphics_site){"n", "a", "(Lls;ID)I", 299}, e);
}
