#include "chunk_provider_internal.h"
#include "mc/core/java/java.h"
#include "mc/resources/files/files.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/propagation.h"
#include "owner.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read-only diagnostic transport. The only controlled game mutation is the
 * actual existing World Random.setSeed, after genuine construction. */
static FILE *queues;
static unsigned event_index;
static size_t queue_bytes = 4u;
static const char *output;
static mc_world_runtime *world;

static bool put32(FILE *f, uint32_t v) {
    const uint8_t b[4] = {(uint8_t)(v >> 24u), (uint8_t)(v >> 16u), (uint8_t)(v >> 8u), (uint8_t)v};
    return fwrite(b, 4, 1, f) == 1;
}

static bool put64(FILE *f, uint64_t v) {
    return put32(f, (uint32_t)(v >> 32u)) && put32(f, (uint32_t)v);
}

static FILE *open_output(const char *name) {
    char path[2048];
    FILE *f = NULL;
    const int length = snprintf(path, sizeof(path), "%s/%s", output, name);
    if (length < 0 || (size_t)length >= sizeof(path))
        return NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, path, "wb") != 0)
        return NULL;
#else
    f = fopen(path, "wb");
#endif
    return f;
}

static bool snapshot(const char *name) {
    FILE *f = open_output(name);
    if (f == NULL)
        return false;
    const mc_chunk_cache *cache = mc_world_runtime_acquisition(world)->cache;
    bool ok = put32(f, UINT32_C(0x4c495431)) && put32(f, (uint32_t)cache->count);
    for (size_t i = 0; ok && i < cache->count; ++i) {
        const mc_chunk_state *c = cache->entries[i].chunk;
        const mc_chunk_data *d = c->data;
        ok = !c->blank && d != NULL && d->blocks != NULL && put32(f, (uint32_t)c->x) &&
             put32(f, (uint32_t)c->z) && fwrite(d->blocks, 32768, 1, f) == 1 &&
             fwrite(d->metadata, 16384, 1, f) == 1 && fwrite(d->skylight, 16384, 1, f) == 1 &&
             fwrite(d->block_light, 16384, 1, f) == 1 && fwrite(d->heightmap, 256, 1, f) == 1;
    }
    if (fclose(f) != 0)
        ok = false;
    return ok;
}

static void rng_json(const mc_java_random *r) {
    uint64_t gaussian;
    memcpy(&gaussian, &r->next_next_gaussian, sizeof(gaussian));
    printf("[\"%012" PRIx64 "\",%s,\"%016" PRIx64 "\"]", r->state,
           r->have_next_next_gaussian ? "true" : "false", gaussian);
}

static bool observe(const char *phase, unsigned phase_code, unsigned point, unsigned call,
                    bool more) {
    mc_world_runtime_fields f;
    mc_chunk_provider_fields provider;
    mc_world_light_list_view list;
    int32_t fc, ec;
    if (mc_world_runtime_fields_get(world, &f) != MC_WORLD_STORAGE_OK ||
        mc_chunk_provider_bind_fields(f.v, &provider, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_light_list_get(world, &list, NULL) != MC_WORLD_STORAGE_OK ||
        mc_scheduled_membership_size(f.F, &fc) != MC_SCHEDULED_MEMBERSHIP_OK ||
        mc_scheduled_ordered_size(f.E, &ec) != MC_SCHEDULED_ORDERED_OK)
        return false;
    printf("{\"event\":%u,\"phase\":\"%s\",\"point\":%u,\"call\":%u,\"more\":%s,\"C\":%zu,\"M\":%d,"
           "\"F\":%d,\"E\":%d,\"cache\":%zu,\"world_rng\":",
           event_index, phase, point, call, more ? "true" : "false", list.count, f.M, fc, ec,
           provider.cache->count);
    rng_json(f.r);
    printf(",\"generator_rng\":");
    rng_json(&provider.generator->generator_random);
    printf(",\"cave_rng\":");
    rng_json(&provider.generator->cave_random);
    puts("}");
    /* Explicit observation budget; exhaustion is a failed capture, never a
     * successful game result or a skipped queue record. */
    const size_t remaining = 512u * 1024u * 1024u - queue_bytes;
    if (remaining < 20u || list.count > (remaining - 20u) / 28u)
        return false;
    queue_bytes += 20u + list.count * 28u;
    bool ok = put32(queues, event_index++) && put32(queues, phase_code) && put32(queues, point) &&
              put32(queues, call) && put32(queues, (uint32_t)list.count);
    for (size_t i = 0; ok && i < list.count; ++i) {
        const mc_world_light_region *region;
        if (mc_world_runtime_light_at(world, i, &region, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        const mc_world_light_region_fields *r = mc_world_runtime_light_region_fields_get(region);
        if (r == NULL)
            return false;
        const mc_world_light_type_fields *t = mc_world_runtime_light_type_fields_get(r->a);
        const mc_light_bounds *b = &r->bounds;
        ok = put32(queues, t == NULL ? 2u : (uint32_t)t->kind) &&
             put32(queues, (uint32_t)b->min_x) && put32(queues, (uint32_t)b->min_y) &&
             put32(queues, (uint32_t)b->min_z) && put32(queues, (uint32_t)b->max_x) &&
             put32(queues, (uint32_t)b->max_y) && put32(queues, (uint32_t)b->max_z);
    }
    return ok && !ferror(queues);
}

static int compare_ticks(const void *a, const void *b) {
    const mc_scheduled_tick_fields *x = a, *y = b;
#define CMP(f)                                                                                     \
    if (x->f != y->f)                                                                              \
    return x->f < y->f ? -1 : 1
    CMP(x);
    CMP(y);
    CMP(z);
    CMP(block);
    CMP(due_time);
    CMP(sequence);
#undef CMP
    return 0;
}

static int compare_pointers(const void *a, const void *b) {
    uintptr_t x = (uintptr_t) * (const mc_scheduled_tick *const *)a,
              y = (uintptr_t) * (const mc_scheduled_tick *const *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

static bool write_tick(FILE *f, const mc_scheduled_tick_fields *t) {
    return put32(f, (uint32_t)t->x) && put32(f, (uint32_t)t->y) && put32(f, (uint32_t)t->z) &&
           put32(f, (uint32_t)t->block) && put64(f, (uint64_t)t->due_time) &&
           put64(f, (uint64_t)t->sequence);
}

static bool scheduler(void) {
    mc_world_runtime_fields f;
    int32_t fc, ec;
    size_t written;
    if (mc_world_runtime_fields_get(world, &f) != MC_WORLD_STORAGE_OK ||
        mc_scheduled_membership_size(f.F, &fc) != MC_SCHEDULED_MEMBERSHIP_OK ||
        mc_scheduled_ordered_size(f.E, &ec) != MC_SCHEDULED_ORDERED_OK)
        return false;
    const mc_scheduled_tick **fp = malloc((size_t)fc * sizeof(*fp)),
                            **ep = malloc((size_t)ec * sizeof(*ep));
    mc_scheduled_tick_fields *fields = malloc((size_t)fc * sizeof(*fields));
    if ((fc != 0 && (fp == NULL || fields == NULL)) || (ec != 0 && ep == NULL)) {
        free(fp);
        free(ep);
        free(fields);
        return false;
    }
    if (mc_scheduled_membership_snapshot(f.F, fp, (size_t)fc, &written) !=
            MC_SCHEDULED_MEMBERSHIP_OK ||
        written != (size_t)fc ||
        mc_scheduled_ordered_snapshot(f.E, ep, (size_t)ec, &written) != MC_SCHEDULED_ORDERED_OK ||
        written != (size_t)ec)
        return false;
    for (int32_t i = 0; i < fc; ++i)
        if (mc_scheduled_tick_fields_get(fp[i], &fields[i]) != MC_SCHEDULED_TICK_OK)
            return false;
    qsort(fields, (size_t)fc, sizeof(*fields), compare_ticks);
    qsort(fp, (size_t)fc, sizeof(*fp), compare_pointers);
    FILE *file = open_output("scheduler.bin");
    if (file == NULL)
        return false;
    bool ok =
        put32(file, UINT32_C(0x53434831)) && put32(file, (uint32_t)fc) && put32(file, (uint32_t)ec);
    for (int32_t i = 0; ok && i < fc; ++i)
        ok = write_tick(file, &fields[i]);
    for (int32_t i = 0; ok && i < ec; ++i) {
        mc_scheduled_tick_fields t;
        ok =
            mc_scheduled_tick_fields_get(ep[i], &t) == MC_SCHEDULED_TICK_OK &&
            write_tick(file, &t) &&
            put32(file,
                  bsearch(&ep[i], fp, (size_t)fc, sizeof(*fp), compare_pointers) != NULL ? 1u : 0u);
    }
    if (fclose(file) != 0)
        ok = false;
    free(fields);
    free(fp);
    free(ep);
    return ok;
}

static bool drain_world(unsigned point) {
    bool more;
    unsigned calls = 0;
    mc_world_light_operation_error error;
    do {
        if (++calls > 10000)
            return false;
        if (mc_world_runtime_light_drain(world, SIZE_MAX, &more, &error) != MC_WORLD_STORAGE_OK) {
            fprintf(stderr, "drain error %d %s region%u\n", (int)error.status, error.operation,
                    error.region_bytecode);
            return false;
        }
        if (!observe("drain", 11, point, calls, more))
            return false;
    } while (more);
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    propagation_reference_owner owner = {0};
#define REQUIRE(v)                                                                                 \
    do {                                                                                           \
        if (!(v)) {                                                                                \
            fprintf(stderr, "observer line%d: %s\n", __LINE__, #v);                                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)
    REQUIRE(argc == 5 && (strcmp(argv[3], "view") == 0 || strcmp(argv[3], "fresh") == 0 ||
                          strcmp(argv[3], "warm") == 0));
    output = argv[4];
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    queues = open_output("queues.bin");
    REQUIRE(queues != NULL && put32(queues, UINT32_C(0x51433031)));
    REQUIRE(propagation_reference_owner_create(&owner, argv[1], argv[2]));
    world = owner.world;
    mc_world_runtime_fields initial;
    REQUIRE(mc_world_runtime_fields_get(world, &initial) == MC_WORLD_STORAGE_OK &&
            initial.r != NULL);
    mc_java_random_reseed(initial.r, INT64_C(0x0123456789abcdef));
    const mc_world_runtime_state *live = mc_world_runtime_live(world);
    int32_t sx = *live->spawn_x, sz = *live->spawn_z;
    printf(
        "{\"inputs\":true,\"world_seed\":23,\"controlled_world_random_seed\":\"0123456789abcdef\","
        "\"spawn\":[%d,%d,%d],\"native_caller_policy\":[262144,16777216,512]}\n",
        sx, *live->spawn_y, sz);
    printf("{\"native_abi\":true,\"entity_literal_selectors\":%d,\"C_standard\":201710}\n",
           (int)MC_ENTITY_CLASS_KIND_COUNT);
    REQUIRE(observe("constructed", 0, 0, 0, false) && snapshot("constructed.chunks"));
    if (strcmp(argv[3], "fresh") != 0) {
        int32_t cx = (sx >> 4) * 16, cz = (sz >> 4) * 16;
        for (int32_t x = -1; x <= 1; ++x)
            for (int32_t z = -1; z <= 1; ++z)
                for (int32_t y = 0; y < 128; y += 16) {
                    (void)y;
                    for (int32_t dx = -1; dx <= 1; ++dx)
                        for (int32_t dz = -1; dz <= 1; ++dz) {
                            mc_chunk_state *chunk;
                            REQUIRE(mc_world_runtime_get(world, (cx >> 4) + x + dx,
                                                         (cz >> 4) + z + dz,
                                                         &chunk) == MC_CHUNK_ACQUIRE_OK);
                        }
                }
        REQUIRE(observe("view-undrained", 1, 0, 0, false) && snapshot("view-undrained.chunks") &&
                drain_world(0) && snapshot("view-drained.chunks"));
    }
    unsigned point = 0;
    if (strcmp(argv[3], "view") != 0)
        for (int32_t x = -128; x <= 128; x += 16)
            for (int32_t z = -128; z <= 128; z += 16) {
                int32_t id;
                REQUIRE(mc_world_runtime_block_get(world, mc_java_i32_add(sx, x), 64,
                                                   mc_java_i32_add(sz, z), &id,
                                                   NULL) == MC_WORLD_STORAGE_OK);
                ++point;
                REQUIRE(observe("acquired", 10, point, 0, false) && drain_world(point));
            }
    REQUIRE(snapshot("final.chunks") && scheduler() && observe("complete", 12, point, 0, false));
    result = 0;
cleanup:
    if (queues != NULL && fclose(queues) != 0)
        result = 1;
    if (!propagation_reference_owner_retire(&owner))
        return 2;
    return result;
}
