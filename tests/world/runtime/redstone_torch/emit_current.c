#include "emit_current.h"
#include "block/registry/registry_internal.h"
#include "world/runtime/runtime_internal.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static const size_t sizes[DB_PLANES] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                        MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                        MC_CHUNK_HEIGHTMAP_BYTES};

static const char *jb(bool value) {
    return value ? "true" : "false";
}

static void js(FILE *out, const char *text) {
    if (text == NULL) {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)text; *p != 0; ++p) {
        if (*p == '"' || *p == '\\')
            fprintf(out, "\\%c", *p);
        else if (*p < 32 || *p > 126)
            fprintf(out, "\\u%04x", (unsigned int)*p);
        else
            fputc(*p, out);
    }
    fputc('"', out);
}

static void literal(FILE *out, const mc_registry_string *text) {
    if (text == NULL) {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    for (size_t i = 0; i < text->length; ++i) {
        const uint16_t c = text->units[i];
        if (c == '"' || c == '\\')
            fprintf(out, "\\%c", (int)c);
        else if (c < 32 || c > 126)
            fprintf(out, "\\u%04x", (unsigned int)c);
        else
            fputc((int)c, out);
    }
    fputc('"', out);
}

static void pointer(FILE *out, const void *value) {
    if (value == NULL)
        fputs("null", out);
    else
        fprintf(out, "\"p:%" PRIxPTR "\"", (uintptr_t)value);
}

static void db_id(FILE *out, const char *domain, uint64_t value) {
    fprintf(out, "\"%s:%" PRIu64 "\"", domain, value);
}

static uint64_t d_bits(double value) {
    uint64_t v;
    memcpy(&v, &value, sizeof(v));
    return v;
}

static uint32_t f_bits(float value) {
    uint32_t v;
    memcpy(&v, &value, sizeof(v));
    return v;
}

static void jd(FILE *out, double value) {
    fprintf(out, "\"%016" PRIx64 "\"", d_bits(value));
}

static void jf(FILE *out, float value) {
    fprintf(out, "\"%08" PRIx32 "\"", f_bits(value));
}

static void vector(FILE *out, const double v[3]) {
    fputc('[', out);
    jd(out, v[0]);
    fputc(',', out);
    jd(out, v[1]);
    fputc(',', out);
    jd(out, v[2]);
    fputc(']', out);
}

static void ints(FILE *out, const int32_t *v, size_t n) {
    fputc('[', out);
    for (size_t i = 0; i < n; ++i)
        fprintf(out, "%s%" PRId32, i == 0 ? "" : ",", v[i]);
    fputc(']', out);
}

static void random_state(FILE *out, const mc_java_random *r) {
    fprintf(out, "{\"seed\":\"%012" PRIx64 "\",\"gaussian\":\"%016" PRIx64 "\",\"have\":%s}",
            r->state, d_bits(r->next_next_gaussian), jb(r->have_next_next_gaussian));
}

static void math_state(FILE *out, const db_math_state *m) {
    if (!m->observed)
        fputs("null", out);
    else
        fprintf(out, "{\"seed\":\"%012" PRIx64 "\",\"gaussian\":\"%016" PRIx64 "\",\"have\":%s}",
                m->seed, m->gaussian, jb(m->have));
}

/* Private observer SHA-256; its local words are utility scratch, not a game
 * snapshot. Inputs are the actual heap-copied bytes, never a modeled scene. */
static uint32_t rr(uint32_t v, unsigned int n) {
    return (v >> n) | (v << (32u - n));
}

static void hash_block(uint32_t h[8], const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i)
        w[i] = ((uint32_t)block[4 * i] << 24u) | ((uint32_t)block[4 * i + 1] << 16u) |
               ((uint32_t)block[4 * i + 2] << 8u) | block[4 * i + 3];
    for (size_t i = 16; i < 64; ++i) {
        const uint32_t a = w[i - 15], b = w[i - 2];
        w[i] = w[i - 16] + (rr(a, 7) ^ rr(a, 18) ^ (a >> 3u)) + w[i - 7] +
               (rr(b, 17) ^ rr(b, 19) ^ (b >> 10u));
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], q = h[7];
    for (size_t i = 0; i < 64; ++i) {
        const uint32_t t =
            q + (rr(e, 6) ^ rr(e, 11) ^ rr(e, 25)) + ((e & f) ^ ((~e) & g)) + k[i] + w[i];
        const uint32_t u = (rr(a, 2) ^ rr(a, 13) ^ rr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        q = g;
        g = f;
        f = e;
        e = d + t;
        d = c;
        c = b;
        b = a;
        a = t + u;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += q;
}

static void hash_value(const uint8_t *bytes, size_t length, uint32_t h[8]) {
    static const uint32_t initial[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(h, initial, sizeof(initial));
    size_t used = 0;
    while (length - used >= 64u) {
        hash_block(h, bytes + used);
        used += 64u;
    }
    uint8_t tail[128] = {0};
    const size_t remainder = length - used, total = remainder < 56u ? 64u : 128u;
    memcpy(tail, bytes + used, remainder);
    tail[remainder] = 0x80;
    const uint64_t bits = (uint64_t)length * 8u;
    for (size_t i = 0; i < 8; ++i)
        tail[total - 1u - i] = (uint8_t)(bits >> (8u * i));
    hash_block(h, tail);
    if (total == 128u)
        hash_block(h, tail + 64);
}

static void hash_bytes(FILE *out, const uint8_t *bytes, size_t length) {
    uint32_t h[8];
    hash_value(bytes, length, h);
    fputc('"', out);
    for (size_t i = 0; i < 8; ++i)
        fprintf(out, "%08" PRIx32, h[i]);
    fputc('"', out);
}

bool db_emit_utility_controls(void) {
    static const char *const expected[] = {
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3"};
    uint8_t *large = malloc(1000u);
    if (large == NULL)
        return false;
    memset(large, 'a', 1000u);
    const uint8_t *inputs[3] = {(const uint8_t *)"", (const uint8_t *)"abc", large};
    const size_t lengths[3] = {0, 3, 1000};
    bool okay = true;
    for (size_t i = 0; i < 3; ++i) {
        uint32_t h[8];
        char actual[65];
        hash_value(inputs[i], lengths[i], h);
        for (size_t n = 0; n < 8; ++n)
            if (snprintf(actual + 8 * n, 9, "%08" PRIx32, h[n]) != 8)
                okay = false;
        if (strcmp(actual, expected[i]) != 0)
            okay = false;
    }
    free(large);
    return okay;
}

static const uint8_t *bytes_at(const db_world_snapshot *w, size_t chunk, size_t plane) {
    size_t offset = chunk * DB_ARRAY_BYTES;
    for (size_t i = 0; i < plane; ++i)
        offset += sizes[i];
    return w->array_bytes + offset;
}

static void arrays(FILE *out, const db_snapshot *s) {
    bool first = true;
    fputc('[', out);
    for (size_t w = 0; w < s->world_count; ++w)
        for (size_t c = 0; c < DB_CHUNKS; ++c)
            for (size_t p = 0; p < DB_PLANES; ++p) {
                fprintf(out, "%s[%zu,%zu,%zu,", first ? "" : ",", w, c, p);
                first = false;
                pointer(out, s->worlds[w].array_owners[c][p]);
                fprintf(out, ",%zu,", sizes[p]);
                hash_bytes(out, bytes_at(&s->worlds[w], c, p), sizes[p]);
                fputc(']', out);
            }
    fputc(']', out);
}

static void changes(FILE *out, const db_snapshot *before, const db_snapshot *after) {
    bool first = true;
    fputc('[', out);
    for (size_t w = 0; w < before->world_count; ++w)
        for (size_t c = 0; c < DB_CHUNKS; ++c)
            for (size_t p = 0; p < DB_PLANES; ++p) {
                const uint8_t *a = bytes_at(&before->worlds[w], c, p),
                              *b = bytes_at(&after->worlds[w], c, p);
                for (size_t i = 0; i < sizes[p]; ++i)
                    if (a[i] != b[i]) {
                        fprintf(out, "%s[%zu,%zu,%zu,%zu,%u,%u]", first ? "" : ",", w, c, p, i,
                                (unsigned int)a[i], (unsigned int)b[i]);
                        first = false;
                    }
            }
    fputc(']', out);
}

static void center(FILE *out, const db_snapshot *s, const db_profile *p) {
    const int32_t x = p->values[DB_X], y = p->values[DB_Y], z = p->values[DB_Z];
    if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127) {
        fputs("null", out);
        return;
    }
    const db_world_snapshot *w = &s->worlds[p->values[DB_WORLD]];
    const size_t c = (size_t)(x >> 4) * 2u + (size_t)(z >> 4),
                 i = (((size_t)x & 15u) << 11u) | (((size_t)z & 15u) << 7u) | (size_t)y;
    fprintf(out, "[%u,%u]", (unsigned int)bytes_at(w, c, 0)[i],
            (unsigned int)((bytes_at(w, c, 1)[i >> 1u] >> ((i & 1u) * 4u)) & 15u));
}

static void scheduled(FILE *out, const db_scheduled_inventory *q) {
    fputs("{\"identity\":", out);
    pointer(out, q->owner);
    fputs(",\"records\":[", out);
    for (size_t i = 0; i < q->count; ++i) {
        const mc_scheduled_tick_fields *f = &q->fields[i];
        if (i != 0)
            fputc(',', out);
        fputs("{\"identity\":", out);
        pointer(out, q->records[i]);
        fprintf(out,
                ",\"xyz\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],\"block\":%" PRId32
                ",\"time\":%" PRId64 ",\"order\":%" PRId64 "}",
                f->x, f->y, f->z, f->block, f->due_time, f->sequence);
    }
    fputs("]}", out);
}

static void worlds(FILE *out, const db_snapshot *s) {
    fputc('[', out);
    for (size_t i = 0; i < s->world_count; ++i) {
        const db_world_snapshot *w = &s->worlds[i];
        if (i != 0)
            fputc(',', out);
        fprintf(out, "{\"role\":%zu,\"identity\":", i);
        pointer(out, w->identity);
        fprintf(out,
                ",\"time\":%" PRId64 ",\"a\":%s,\"B\":%s,\"o\":%s,\"s\":%s,\"M\":%" PRId32
                ",\"light\":{\"identity\":",
                w->time, jb(w->a), jb(w->remote), jb(w->o), jb(w->s), w->M);
        pointer(out, w->light_owner);
        fprintf(out, ",\"capacity\":%zu,\"records\":[", w->light_capacity);
        for (size_t n = 0; n < w->light_count; ++n) {
            if (n != 0)
                fputc(',', out);
            fputs("{\"identity\":", out);
            pointer(out, w->light_records[n]);
            fputs(",\"region\":", out);
            ints(out, w->light_regions[n], 7);
            fputc('}', out);
        }
        fputs("]},\"ordered\":", out);
        scheduled(out, &w->ordered);
        fputs(",\"membership\":", out);
        scheduled(out, &w->membership);
        fputs(",\"entities\":{\"identity\":", out);
        pointer(out, w->entity_list_owner);
        fprintf(out, ",\"capacity\":%zu,\"records\":[", w->entity_capacity);
        for (size_t n = 0; n < w->entity_count; ++n) {
            const mc_entity_snapshot *e = &w->entity_fields[n];
            const mc_item_stack_fields *t = &w->entity_stacks[n];
            if (n != 0)
                fputc(',', out);
            fputs("{\"identity\":", out);
            pointer(out, w->entities[n]);
            fprintf(out,
                    ",\"source\":%d,\"phase\":%d,\"stack\":[%" PRId32 ",%" PRId32 ",%" PRId32
                    "],\"defaults\":[%" PRId32 ",%" PRId32 ",%" PRId32 "]}",
                    (int)e->source, (int)e->phase, t->id, t->count, t->damage, e->item.c, e->item.b,
                    e->item.f);
        }
        fputs("]}}", out);
    }
    fputc(']', out);
}

static void history(FILE *out, const db_snapshot *s) {
    const mc_registry_redstone_torch_list_view *v = &s->history_view;
    fputs("{\"owner\":", out);
    pointer(out, s->history_owner);
    fprintf(out, ",\"identity\":%" PRIu64 ",\"backing\":", v->identity);
    db_id(out, "hb", v->array_identity);
    fprintf(out,
            ",\"backing_kind\":%d,\"allocated_records\":%" PRIu64 ",\"size\":%" PRId32
            ",\"capacity\":%" PRId32 ",\"modCount\":%" PRId32
            ",\"unused_slots_null\":%s,\"records\":[",
            (int)v->backing, v->allocated_records, v->size, v->capacity, v->mod_count,
            jb(v->unused_slots_null));
    for (int32_t i = 0; i < v->size; ++i) {
        const mc_registry_redstone_torch_record_view *r = &s->history_slots[i];
        if (i != 0)
            fputc(',', out);
        if (r->is_null) {
            fputs("null", out);
            continue;
        }
        fputs("{\"identity\":", out);
        db_id(out, "xu", r->identity);
        fprintf(out,
                ",\"constructed\":%s,\"xyz\":[%" PRId32 ",%" PRId32 ",%" PRId32
                "],\"time\":%" PRId64 "}",
                jb(r->constructed), r->x, r->y, r->z, r->time);
    }
    fputs("]}", out);
}

static void positions(FILE *out, const db_snapshot *s) {
    const mc_registry_redstone_set_view *v = &s->position_set;
    fputs("{\"owner\":", out);
    pointer(out, s->position_set_owner);
    fputs(",\"map\":", out);
    pointer(out, v->map);
    fputs(",\"table\":", out);
    pointer(out, v->table);
    fprintf(out,
            ",\"size\":%zu,\"capacity\":%" PRIu32 ",\"threshold\":%" PRId32 ",\"modCount\":%" PRId32
            ",\"constructed_keys\":%" PRIu64 ",\"load_factor\":",
            v->size, v->capacity, v->threshold, v->mod_count, v->constructed_keys);
    jf(out, v->load_factor);
    fprintf(out, ",\"output\":%s,\"nodes\":[", jb(s->position_output));
    for (size_t i = 0; i < v->size; ++i) {
        const mc_registry_redstone_node_view *n = &s->position_nodes[i];
        if (i != 0)
            fputc(',', out);
        fprintf(out,
                "{\"identity\":%" PRIu64 ",\"next\":%" PRIu64 ",\"previous\":%" PRIu64
                ",\"parent\":%" PRIu64 ",\"left\":%" PRIu64 ",\"right\":%" PRIu64
                ",\"key_identity\":%" PRIu64 ",\"xyz\":[%" PRId32 ",%" PRId32 ",%" PRId32
                "],\"hash\":%" PRIu32 ",\"bucket\":%" PRIu32 ",\"tree\":%s,\"red\":%s}",
                n->identity, n->next, n->previous, n->parent, n->left, n->right, n->key.identity,
                n->key.x, n->key.y, n->key.z, n->hash, n->bucket, jb(n->tree), jb(n->red));
    }
    fputs("]}", out);
}

static void snapshot(FILE *out, const db_snapshot *s) {
    fputs("{\"arrays\":", out);
    arrays(out, s);
    fputs(",\"worlds\":", out);
    worlds(out, s);
    fputs(",\"history\":", out);
    history(out, s);
    fputs(",\"position_set\":", out);
    positions(out, s);
    fprintf(out, ",\"sequence\":{\"initialized\":%s,\"next\":%" PRId64 "}}",
            jb(s->sequence.initialized), s->sequence.next);
}

static void owners(FILE *out, const db_fixture *f, const db_snapshot *s) {
    fputs("{\"worlds\":[", out);
    for (size_t i = 0; i < s->world_count; ++i) {
        if (i != 0)
            fputc(',', out);
        fprintf(out, "{\"role\":%zu,\"alias\":", i);
        pointer(out, s->worlds[i].random_owner);
        fputs(",\"state\":", out);
        random_state(out, &s->worlds[i].random);
        fputc('}', out);
    }
    fputs("],\"independent\":{\"alias\":", out);
    pointer(out, &f->independent);
    fputs(",\"state\":", out);
    random_state(out, &s->independent);
    fputs("}}", out);
}

static void event_log(FILE *out, const db_fixture *f) {
    fputc('[', out);
    for (size_t i = 0; i < f->event_count; ++i) {
        const db_event *e = &f->events[i];
        if (i != 0)
            fputc(',', out);
        static const char *const kinds[] = {"cell", "range", "sound", "particle", "item"};
        fputs("{\"kind\":", out);
        js(out, kinds[e->kind]);
        fprintf(out, ",\"world\":%zu", e->world_role);
        if (e->kind == DB_CELL) {
            fputs(",\"xyz\":", out);
            ints(out, e->integers, 3);
            fputs(",\"cell\":", out);
            if (e->cell_captured)
                ints(out, e->integers + 3, 2);
            else
                fputs("null", out);
        } else if (e->kind == DB_RANGE) {
            fputs(",\"bounds\":", out);
            ints(out, e->integers, 6);
        } else if (e->kind == DB_SOUND || e->kind == DB_PARTICLE) {
            fputs(",\"name\":", out);
            literal(out, e->name);
            fputs(",\"position\":", out);
            vector(out, e->position);
            if (e->kind == DB_SOUND) {
                fputs(",\"volume\":", out);
                jf(out, e->volume);
                fputs(",\"pitch\":", out);
                jf(out, e->pitch);
            } else {
                fputs(",\"motion\":", out);
                vector(out, e->motion);
            }
        } else {
            const mc_entity_snapshot *v = &e->item;
            const mc_item_stack_fields *s = &e->stack;
            fputs(",\"identity\":", out);
            pointer(out, e->entity);
            fprintf(out,
                    ",\"stack\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],\"defaults\":[%" PRId32
                    ",%" PRId32 ",%" PRId32 "],\"position\":[",
                    s->id, s->count, s->damage, v->item.c, v->item.b, v->item.f);
            jd(out, v->base.x);
            fputc(',', out);
            jd(out, v->base.y);
            fputc(',', out);
            jd(out, v->base.z);
            fputs("],\"motion\":[", out);
            jd(out, v->base.velocity_x);
            fputc(',', out);
            jd(out, v->base.velocity_y);
            fputc(',', out);
            jd(out, v->base.velocity_z);
            fputs("],\"yaw\":", out);
            jf(out, v->base.yaw);
            fputs(",\"bob\":", out);
            jf(out, v->item.d);
            fputs(",\"entity_random\":", out);
            random_state(out, &e->entity_random);
            fputs(",\"math_post\":", out);
            math_state(out, &e->math);
        }
        fprintf(out, ",\"uniquifier\":\"%016" PRIx64 "\",\"states\":{\"world\":", e->uniquifier);
        random_state(out, &e->world_random);
        fputs(",\"independent\":", out);
        random_state(out, &e->independent_random);
        fputs("}}", out);
    }
    fputc(']', out);
}

static void registry_error(FILE *out, const mc_registry_error *e) {
    fprintf(out, "{\"status\":%d,\"message\":", (int)e->status);
    js(out, e->message);
    fprintf(out, ",\"initializer_pc\":%" PRId32 ",\"block_id\":%" PRId32 ",\"constructor_class\":",
            e->initializer_pc, e->block_id);
    js(out, e->constructor_class);
    fprintf(out,
            ",\"constructor_pc\":%" PRId32 ",\"profile_status\":%d,\"active_initializer_class\":",
            e->constructor_pc, (int)e->profile_status);
    js(out, e->active_initializer_class);
    fprintf(out,
            ",\"active_initializer_pc\":%" PRId32 ",\"item_argument\":%" PRId32
            ",\"format\":{\"status\":%d,\"offset\":%zu,\"length\":%zu,\"value\":%" PRId32
            ",\"flags\":%u,\"conversion\":%u,\"date_time\":%s}}",
            e->active_initializer_pc, e->item_argument, (int)e->format_error.status,
            e->format_error.offset, e->format_error.length, e->format_error.value,
            (unsigned int)e->format_error.flags, (unsigned int)e->format_error.conversion,
            jb(e->format_error.date_time));
}

static void native_outcome(FILE *out, const db_outcome *v, bool invoked) {
    fprintf(out,
            "{\"invoke_ok\":%s,\"entered\":%s,\"returned\":%s,\"value\":%s,\"observer_refused\":%s,"
            "\"source_null\":%s,\"native_failure\":%s,\"error\":",
            jb(invoked), jb(v->entered), jb(v->returned), jb(v->value), jb(v->observer_refused),
            jb(v->source_null), jb(v->native_failure));
    const mc_world_mutation_error *e = v->error;
    if (e == NULL) {
        fputs("null}", out);
        return;
    }
    fprintf(out, "{\"status\":%d,\"operation\":", (int)e->status);
    js(out, e->operation);
    fputs(",\"hook_class\":", out);
    js(out, e->hook_class);
    fputs(",\"hook_method\":", out);
    js(out, e->hook_method);
    fprintf(out, ",\"hook_pc\":%u,\"cause_hook_class\":", e->hook_bytecode);
    js(out, e->cause_hook_class);
    fputs(",\"cause_hook_method\":", out);
    js(out, e->cause_hook_method);
    fprintf(out,
            ",\"cause_hook_pc\":%u,\"db_sites\":[%u,%u,%u,%u,%u,%u],\"db_source_null\":%s,\"vm_"
            "source_null\":%s,\"mod_count_stored\":%s,\"collection_status\":%d,\"collection_"
            "failure\":%d,\"info_status\":%d,\"world_pc\":%u,\"chunk_pc\":%u,\"world_access_pc\":%"
            "u,\"schedule_pc\":%u,\"cause\":{\"status\":%d,\"operation\":",
            e->cause_hook_bytecode, e->redstone_torch.added, e->redstone_torch.removed,
            e->redstone_torch.neighbor, e->redstone_torch.tick, e->redstone_torch.powered,
            e->redstone_torch.burnout, jb(e->redstone_torch_source_null), jb(e->torch_source_null),
            jb(e->redstone_torch_mod_count_stored), (int)e->redstone_torch_collection_status,
            (int)e->redstone_torch_collection_failure, (int)e->redstone_torch_info, e->bytecode,
            e->chunk_bytecode, e->world_access_bytecode, e->schedule_bytecode,
            (int)e->cause.status);
    js(out, e->cause.operation);
    fprintf(out, ",\"system_error\":%" PRIu32 "},\"registry\":", e->cause.system_error);
    registry_error(out, &e->registry);
    fprintf(out, ",\"schedule\":{\"status\":%d,\"operation\":", (int)e->schedule.status);
    js(out, e->schedule.operation);
    fprintf(out,
            ",\"pc\":%u,\"presence_pc\":%u,\"provider_pc\":%u,\"record\":%d,\"membership\":%d,"
            "\"ordered\":%d}}}",
            e->schedule.bytecode, e->schedule.presence_bytecode, e->schedule.provider_bytecode,
            (int)e->schedule.record, (int)e->schedule.membership, (int)e->schedule.ordered);
}

bool db_emit_worlds(FILE *out, const db_fixture *f) {
    for (size_t i = 0; i < f->world_count; ++i) {
        const db_world *w = &f->worlds[i];
        fprintf(out,
                "DB_CURRENT_WORLD\t{\"role\":%zu,\"time\":%" PRId64 ",\"loaded\":%s,\"identity\":",
                i, w->saved_time, jb(!w->world->fields.s));
        pointer(out, w->world);
        fprintf(out,
                ",\"uniquifier_before\":\"%016" PRIx64 "\",\"uniquifier_after\":\"%016" PRIx64
                "\",\"math_before\":",
                w->constructor_uniquifier_before, w->constructor_uniquifier_after);
        math_state(out, &w->constructor_math_before);
        fputs(",\"math_after\":", out);
        math_state(out, &w->constructor_math_after);
        fputs("}\n", out);
    }
    return !ferror(out);
}

bool db_emit_row(FILE *out, const char *cohort, size_t index, const db_profile *p,
                 const db_fixture *f, const db_snapshot *before, const db_snapshot *after,
                 const db_outcome *v, bool invoked) {
    fputs("DB_CURRENT\t{\"cohort\":", out);
    js(out, cohort);
    fprintf(out, ",\"index\":%zu,\"name\":", index);
    js(out, p->name);
    fputs(",\"values\":", out);
    ints(out, p->values, DB_OPERANDS);
    fputs(",\"native\":", out);
    native_outcome(out, v, invoked);
    fputs(",\"center_before\":", out);
    center(out, before, p);
    fputs(",\"center_after\":", out);
    center(out, after, p);
    fputs(",\"before\":", out);
    snapshot(out, before);
    fputs(",\"after\":", out);
    snapshot(out, after);
    fputs(",\"changes\":", out);
    changes(out, before, after);
    fputs(",\"events\":", out);
    event_log(out, f);
    fputs("}\n", out);
    fputs("DB_CURRENT_ENTROPY\t{\"cohort\":", out);
    js(out, cohort);
    fprintf(out, ",\"index\":%zu,\"name\":", index);
    js(out, p->name);
    fprintf(out, ",\"mode\":%" PRId32 ",\"owners_before\":", p->values[DB_RANDOM_MODE]);
    owners(out, f, before);
    fputs(",\"owners_after\":", out);
    owners(out, f, after);
    fputs(",\"supplied_alias\":", out);
    if (p->values[DB_RANDOM_MODE] == 0)
        pointer(out, before->worlds[p->values[DB_WORLD]].random_owner);
    else if (p->values[DB_RANDOM_MODE] == 1)
        pointer(out, &f->independent);
    else
        fputs("null", out);
    fputs(",\"math_before\":", out);
    math_state(out, &before->math);
    fputs(",\"math_after\":", out);
    math_state(out, &after->math);
    fprintf(out,
            ",\"seed_uniquifier_before\":\"%016" PRIx64 "\",\"seed_uniquifier_after\":\"%016" PRIx64
            "\"}\n",
            before->uniquifier, after->uniquifier);
    return !ferror(out);
}

bool db_emit_finish(FILE *out, const char *cohort, size_t rows, const db_fixture *f) {
    fputs("DB_CURRENT_FINISH\t{\"cohort\":", out);
    js(out, cohort);
    fprintf(out, ",\"rows\":%zu,\"retired\":true,\"terminal_chunk_counts\":[", rows);
    for (size_t i = 0; i < f->world_count; ++i)
        fprintf(out, "%s%zu", i == 0 ? "" : ",", f->worlds[i].terminal_chunk_count);
    fputs("]}\n", out);
    return !ferror(out);
}
