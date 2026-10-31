#include "source_screen_internal.h"

#include <stdlib.h>
#include <string.h>

mc_client_screen_status mc_screen_publish(const mc_client_screen_error *e,
                                          mc_client_screen_error *out) {
    if (out != NULL)
        *out = *e;
    return e->status;
}

mc_client_screen_status mc_screen_fail(mc_client_screen_error *e, mc_client_screen_status status,
                                       const char *operation, const char *cl, const char *method,
                                       const char *descriptor, unsigned int pc) {
    e->status = status;
    e->source_returned = false;
    e->operation = operation;
    e->site = (mc_client_screen_site){cl, method, descriptor, pc};
    e->native_policy = true;
    return status;
}

bool mc_screen_lock(mc_client_screen_classes *c, mc_client_screen_error *e) {
    if (c == NULL) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen class receiver", NULL, NULL,
                       NULL, 0);
        return false;
    }
    const int status = mtx_trylock(&c->mutex);
    if (status == thrd_success)
        return true;
    e->native_thread_error = status;
    mc_screen_fail(e, status == thrd_busy ? MC_CLIENT_SCREEN_BUSY : MC_CLIENT_SCREEN_THREAD_FAILED,
                   "screen native monitor", NULL, NULL, NULL, 0);
    return false;
}

bool mc_screen_class_hold_locked(mc_client_screen_classes *c, mc_client_screen_error *e) {
    if (c->retiring || c->holds == SIZE_MAX) {
        mc_screen_fail(e, c->retiring ? MC_CLIENT_SCREEN_BUSY : MC_CLIENT_SCREEN_LIMIT,
                       "screen class occurrence", NULL, NULL, NULL, 0);
        return false;
    }
    ++c->holds;
    return true;
}

bool mc_screen_active_locked(mc_client_screen_classes *c, mc_client_screen_error *e) {
    if (c->retiring) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "screen namespace retiring", NULL, NULL, NULL, 0);
        return false;
    }
    if (c->phase == MC_CLIENT_SCREEN_CLASS_READY)
        return true;
    if (c->phase == MC_CLIENT_SCREEN_CLASS_FAILED) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_CLASS_ERRONEOUS, "previous fu class failure", "fu",
                       "<clinit>", "()V", c->failure.site.pc);
        e->first_class_status = c->failure.status;
        e->first_class_site = c->failure.site;
        e->first_class_random = c->failure.random;
        return false;
    }
    if (c->phase == MC_CLIENT_SCREEN_CLASS_INITIALIZING) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "fu class initializer active", "fu", "<clinit>",
                       "()V", c->last_pc);
        return false;
    }
    c->phase = MC_CLIENT_SCREEN_CLASS_INITIALIZING;
    /* Actual ub/da have no <clinit>. fu allocates and constructs its OWN Random. */
    c->last_pc = 0;
    c->pending_random = calloc(1, sizeof(*c->pending_random));
    if (c->pending_random == NULL)
        mc_screen_fail(e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "fu static new Random", "fu",
                       "<clinit>", "()V", 0);
    else {
        c->last_pc = 4;
        e->random = mc_java_random_construct_default(c->profile, c->pending_random);
        if (e->random != MC_JAVA_RANDOM_PROFILE_OK)
            mc_screen_fail(e, MC_CLIENT_SCREEN_RANDOM_FAILED, "fu actual default Random", "fu",
                           "<clinit>", "()V", 4);
        else {
            c->a = c->pending_random;
            c->pending_random = NULL;
            c->last_store_pc = 7;
            c->last_pc = 10;
            c->phase = MC_CLIENT_SCREEN_CLASS_READY;
            return true;
        }
    }
    c->failure = *e;
    c->phase = MC_CLIENT_SCREEN_CLASS_FAILED;
    return false;
}

mc_client_screen_status mc_client_screen_classes_create(mc_java_random_profile *profile,
                                                        const mc_registry_resources *resources,
                                                        mc_registry_classes *registry,
                                                        mc_client_screen_classes **out,
                                                        mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (profile == NULL || out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                       "screen namespace dependencies/output", NULL, NULL, NULL, 0);
    else {
        mc_client_screen_classes *c = calloc(1, sizeof(*c));
        if (c == NULL)
            mc_screen_fail(&e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "screen cold namespace", NULL,
                           NULL, NULL, 0);
        else {
            const int result = mtx_init(&c->mutex, mtx_plain);
            if (result != thrd_success) {
                e.native_thread_error = result;
                mc_screen_fail(&e, MC_CLIENT_SCREEN_THREAD_FAILED, "screen native monitor creation",
                               NULL, NULL, NULL, 0);
                free(c);
            } else {
                c->profile = profile;
                c->resources = resources;
                c->registry = registry;
                if (registry != NULL) {
                    e.registry.status = mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT,
                                                                   profile, &e.registry);
                    if (e.registry.status != MC_REGISTRY_OK)
                        mc_screen_fail(&e, MC_CLIENT_SCREEN_REGISTRY_FAILED,
                                       "screen same Registry/profile hold", NULL, NULL, NULL, 0);
                    else
                        c->registry_held = true;
                }
                if (e.status == MC_CLIENT_SCREEN_OK) {
                    c->holds = 1;
                    *out = c;
                } else {
                    mtx_destroy(&c->mutex);
                    free(c);
                }
            }
        }
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_menu_active_use(mc_client_screen_classes *c,
                                                         mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (mc_screen_lock(c, &e)) {
        (void)mc_screen_active_locked(c, &e);
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_classes_retain(mc_client_screen_classes *c,
                                                        mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (mc_screen_lock(c, &e)) {
        (void)mc_screen_class_hold_locked(c, &e);
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

bool mc_screen_class_final_locked(mc_client_screen_classes *c, mc_client_screen_error *e) {
    c->retiring = true;
    if (c->registry_held) {
        e->registry.status = mc_registry_classes_release(c->registry, &e->registry);
        if (e->registry.status != MC_REGISTRY_OK) {
            mc_screen_fail(e,
                           e->registry.status == MC_REGISTRY_BUSY
                               ? MC_CLIENT_SCREEN_BUSY
                               : MC_CLIENT_SCREEN_REGISTRY_FAILED,
                           "screen Registry occurrence retirement", NULL, NULL, NULL, 0);
            return false;
        }
        c->registry_held = false;
    }
    return true;
}

void mc_screen_class_drop_locked(mc_client_screen_classes *c) {
    const bool final = --c->holds == 0;
    (void)mtx_unlock(&c->mutex);
    if (final) {
        free(c->a);
        free(c->pending_random);
        mc_player_string_release(c->missingno);
        mc_player_string_release(c->splash_path);
        while (c->literals != NULL) {
            mc_screen_literal *n = c->literals;
            c->literals = n->next;
            mc_player_string_release(n->value);
            free(n);
        }
        mtx_destroy(&c->mutex);
        free(c);
    }
}

mc_client_screen_status mc_client_screen_classes_release(mc_client_screen_classes **slot,
                                                         mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (slot == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen class release slot", NULL,
                       NULL, NULL, 0);
    else if (*slot != NULL && mc_screen_lock(*slot, &e)) {
        mc_client_screen_classes *c = *slot;
        if (c->holds == 1 && !mc_screen_class_final_locked(c, &e))
            (void)mtx_unlock(&c->mutex);
        else {
            *slot = NULL;
            mc_screen_class_drop_locked(c);
        }
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_classes_inspect(mc_client_screen_classes *c,
                                                         mc_client_screen_class_state *out,
                                                         mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen class inspection output",
                       NULL, NULL, NULL, 0);
    else if (mc_screen_lock(c, &e)) {
        *out = (mc_client_screen_class_state){c->phase,       c->holds,          c->retiring,
                                              c->a,           c->pending_random, c->missingno,
                                              c->splash_path, c->last_pc,        c->last_store_pc,
                                              c->failure};
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

void mc_screen_memory_init(mc_screen_memory *m, const mc_client_screen_options *options,
                           size_t size) {
    *m = (mc_screen_memory){0};
    m->options = options != NULL ? *options
                                 : (mc_client_screen_options){1024u * 1024u, SIZE_MAX,
                                                              1024u * 1024u, 4096u, 512u};
    m->bytes = size;
}

void *mc_screen_allocate(mc_screen_memory *m, size_t size) {
    ++m->attempts;
    m->allocation_status = MC_CLIENT_SCREEN_LIMIT;
    if (m->allocations >= m->options.maximum_allocations ||
        size > SIZE_MAX - sizeof(mc_screen_block) || m->bytes > m->options.maximum_bytes ||
        size + sizeof(mc_screen_block) > m->options.maximum_bytes - m->bytes)
        return NULL;
    m->allocation_status = MC_CLIENT_SCREEN_ALLOCATION_FAILED;
    mc_screen_block *block = malloc(sizeof(*block));
    if (block == NULL)
        return NULL;
    block->memory = calloc(1, size == 0 ? 1 : size);
    if (block->memory == NULL) {
        free(block);
        return NULL;
    }
    block->next = m->blocks;
    m->blocks = block;
    m->bytes += size + sizeof(*block);
    ++m->allocations;
    m->allocation_status = MC_CLIENT_SCREEN_OK;
    return block->memory;
}

void mc_screen_memory_free(mc_screen_memory *m) {
    while (m->blocks != NULL) {
        mc_screen_block *b = m->blocks;
        m->blocks = b->next;
        free(b->memory);
        free(b);
    }
}

bool mc_client_screen_text_read(mc_client_screen_text text, mc_player_string_view *out) {
    if (out == NULL)
        return false;
    if (text.kind == MC_CLIENT_SCREEN_TEXT_PLAYER && text.registry == NULL)
        return mc_player_string_read(text.player, out);
    if (text.kind == MC_CLIENT_SCREEN_TEXT_REGISTRY && text.player == NULL) {
        mc_registry_string_view v;
        if (!mc_registry_string_read(text.registry, &v))
            return false;
        *out = (mc_player_string_view){v.units, v.length};
        return true;
    }
    return false;
}

bool mc_screen_literal_locked(mc_client_screen_classes *c, const char *ascii,
                              mc_player_string **out, mc_client_screen_error *e) {
    mc_screen_literal *node = c->literals;
    while (node != NULL && strcmp(node->ascii, ascii) != 0)
        node = node->next;
    if (node == NULL) {
        node = calloc(1, sizeof(*node));
        if (node == NULL) {
            mc_screen_fail(e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "screen class literal binding",
                           NULL, NULL, NULL, 0);
            return false;
        }
        node->ascii = ascii;
        node->next = c->literals;
        c->literals = node;
    }
    if (node->value == NULL) {
        const size_t length = strlen(ascii);
        uint16_t units[128];
        if (length > sizeof(units) / sizeof(units[0])) {
            mc_screen_fail(e, MC_CLIENT_SCREEN_LIMIT, "screen literal bound", NULL, NULL, NULL, 0);
            return false;
        }
        for (size_t i = 0; i < length; ++i)
            units[i] = (uint8_t)ascii[i];
        e->string = mc_player_string_create(units, length, &node->value);
        if (e->string != MC_PLAYER_OK) {
            mc_screen_fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "screen actual String literal", NULL,
                           NULL, NULL, 0);
            return false;
        }
    }
    *out = node->value;
    return true;
}
