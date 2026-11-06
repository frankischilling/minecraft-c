/* Authored native transport for the separately sealed original forty-row
 * parent matrix. Only declared BEFORE inputs are consumed. No answer, source
 * getter substitute, PC93 probe, source replay or phase rewrite is supplied. */
#include "mc/entity/player/coordinates.h"
#include "mc/entity/player/sleep.h"
#include "mc/world/runtime/loading/player.h"
#include <errno.h>

enum { NSM_COLUMNS = 26, NSM_CASES = 40 };

typedef struct nsm_fixture {
    nbt_metadata_fixture owner;
    mc_chunk_data *data;
    nbt_input input;
    const mc_nbt_tag *tag;
    mc_world_info_player *saved;
    mc_entity_nbt_error read;
    mc_world_player_load_error load;
    mc_player_sleep_call *preparation;
    mc_player_session_classes *sessions;
    mc_entity_bounds *bounds;
    struct mc_player_nbt_coordinate *pins[3];
    mc_item_stack *old_slots[40]; /* Owned Stack occurrences, not old array owners. */
    uintptr_t before_v, before_q, before_bn, before_main, before_armor, before_list;
    mc_world_info *before_info;
    bool attached;
} nsm_fixture;

static FILE *nsm_stream;

static const char *nsm_bool(bool v) {
    return v ? "true" : "false";
}

static bool nsm_key(const char *name, bool *first) {
    const int result = fprintf(nsm_stream, "%s\"%s\":", *first ? "" : ",", name);
    *first = false;
    return result >= 0;
}

static bool nsm_integer(const char *name, int64_t v, bool *first) {
    return nsm_key(name, first) && fprintf(nsm_stream, "%" PRId64, v) >= 0;
}

static bool nsm_boolean(const char *name, bool v, bool *first) {
    return nsm_key(name, first) && fprintf(nsm_stream, "%s", nsm_bool(v)) >= 0;
}

static bool nsm_float(const char *name, float v, bool *first) {
    return nsm_key(name, first) &&
           fprintf(nsm_stream, "{\"F\":\"%08" PRIx32 "\"}", movement_float_bits(v)) >= 0;
}

static bool nsm_double(const char *name, double v, bool *first) {
    return nsm_key(name, first) &&
           fprintf(nsm_stream, "{\"D\":\"%016" PRIx64 "\"}", movement_double_bits(v)) >= 0;
}

static bool nsm_pointer(const char *name, const void *v, bool *first) {
    return nsm_key(name, first) &&
           (v == NULL ? fprintf(nsm_stream, "null")
                      : fprintf(nsm_stream, "{\"native_identity\":\"%016" PRIxPTR "\"}",
                                (uintptr_t)v)) >= 0;
}

/* Actual inspected fields: constructor-derived values are within-call only. */
static bool nsm_raw_actor(const mc_entity *a) {
    bool first = true;
    if (fputc('{', nsm_stream) == EOF)
        return false;
    if (!nsm_integer("SN.id", a->base.id, &first))
        return false;
    if (!nsm_integer("SN.private_b", a->base.private_b, &first))
        return false;
    if (!nsm_integer("SN.bt", a->base.bt, &first))
        return false;
    if (!nsm_integer("SN.bu", a->base.bu, &first))
        return false;
    if (!nsm_integer("SN.bv", a->base.bv, &first))
        return false;
    if (!nsm_integer("SN.bw", a->base.bw, &first))
        return false;
    if (!nsm_integer("SN.by", a->base.by, &first))
        return false;
    if (!nsm_integer("SN.bz", a->base.bz, &first))
        return false;
    if (!nsm_integer("SN.bG", a->base.bG, &first))
        return false;
    if (!nsm_integer("SN.bH", a->base.bH, &first))
        return false;
    if (!nsm_integer("SN.bI", a->base.bI, &first))
        return false;
    if (!nsm_integer("SN.client_only.bJ", a->base.client_only.bJ, &first))
        return false;
    if (!nsm_integer("SN.client_only.bK", a->base.client_only.bK, &first))
        return false;
    if (!nsm_integer("SN.client_only.bL", a->base.client_only.bL, &first))
        return false;
    if (!nsm_double("SN.aE", a->base.aE, &first))
        return false;
    if (!nsm_double("SN.aJ", a->base.aJ, &first))
        return false;
    if (!nsm_double("SN.aK", a->base.aK, &first))
        return false;
    if (!nsm_double("SN.aL", a->base.aL, &first))
        return false;
    if (!nsm_double("SN.x", a->base.x, &first))
        return false;
    if (!nsm_double("SN.y", a->base.y, &first))
        return false;
    if (!nsm_double("SN.z", a->base.z, &first))
        return false;
    if (!nsm_double("SN.velocity_x", a->base.velocity_x, &first))
        return false;
    if (!nsm_double("SN.velocity_y", a->base.velocity_y, &first))
        return false;
    if (!nsm_double("SN.velocity_z", a->base.velocity_z, &first))
        return false;
    if (!nsm_double("SN.bl", a->base.bl, &first))
        return false;
    if (!nsm_double("SN.bm", a->base.bm, &first))
        return false;
    if (!nsm_double("SN.bn", a->base.bn, &first))
        return false;
    if (!nsm_double("SN.private_d", a->base.private_d, &first))
        return false;
    if (!nsm_double("SN.private_e", a->base.private_e, &first))
        return false;
    if (!nsm_float("SN.yaw", a->base.yaw, &first))
        return false;
    if (!nsm_float("SN.aT", a->base.aT, &first))
        return false;
    if (!nsm_float("SN.aU", a->base.aU, &first))
        return false;
    if (!nsm_float("SN.aV", a->base.aV, &first))
        return false;
    if (!nsm_float("SN.offset", a->base.offset, &first))
        return false;
    if (!nsm_float("SN.width", a->base.width, &first))
        return false;
    if (!nsm_float("SN.height", a->base.height, &first))
        return false;
    if (!nsm_float("SN.bi", a->base.bi, &first))
        return false;
    if (!nsm_float("SN.bj", a->base.bj, &first))
        return false;
    if (!nsm_float("SN.bk", a->base.bk, &first))
        return false;
    if (!nsm_float("SN.y_size", a->base.y_size, &first))
        return false;
    if (!nsm_float("SN.bp", a->base.bp, &first))
        return false;
    if (!nsm_float("SN.br", a->base.br, &first))
        return false;
    if (!nsm_float("SN.bE", a->base.bE, &first))
        return false;
    if (!nsm_boolean("SN.aF", a->base.aF, &first))
        return false;
    if (!nsm_boolean("SN.aX", a->base.aX, &first))
        return false;
    if (!nsm_boolean("SN.aY", a->base.aY, &first))
        return false;
    if (!nsm_boolean("SN.aZ", a->base.aZ, &first))
        return false;
    if (!nsm_boolean("SN.ba", a->base.ba, &first))
        return false;
    if (!nsm_boolean("SN.bb", a->base.bb, &first))
        return false;
    if (!nsm_boolean("SN.bc", a->base.bc, &first))
        return false;
    if (!nsm_boolean("SN.bd", a->base.bd, &first))
        return false;
    if (!nsm_boolean("SN.be", a->base.be, &first))
        return false;
    if (!nsm_boolean("SN.bq", a->base.bq, &first))
        return false;
    if (!nsm_boolean("SN.bx", a->base.bx, &first))
        return false;
    if (!nsm_boolean("SN.private_c", a->base.private_c, &first))
        return false;
    if (!nsm_boolean("SN.bC", a->base.bC, &first))
        return false;
    if (!nsm_boolean("SN.bF", a->base.bF, &first))
        return false;
    if (!nsm_boolean("SN.bM", a->base.bM, &first))
        return false;
    if (!nsm_pointer("SN.aG", a->base.aG, &first))
        return false;
    if (!nsm_pointer("SN.aH", a->base.aH, &first))
        return false;
    if (!nsm_pointer("SN.world", a->base.world, &first))
        return false;
    if (!nsm_pointer("SN.client_only.bA", a->base.client_only.bA, &first))
        return false;
    if (!nsm_pointer("SN.client_only.bB", a->base.client_only.bB, &first))
        return false;
    if (!nsm_integer("LS.E", a->local_player->fields.living.E, &first))
        return false;
    if (!nsm_integer("LS.T", a->local_player->fields.living.T, &first))
        return false;
    if (!nsm_integer("LS.Y", a->local_player->fields.living.Y, &first))
        return false;
    if (!nsm_integer("LS.Z", a->local_player->fields.living.Z, &first))
        return false;
    if (!nsm_integer("LS.private_a", a->local_player->fields.living.private_a, &first))
        return false;
    if (!nsm_integer("LS.aa", a->local_player->fields.living.aa, &first))
        return false;
    if (!nsm_integer("LS.ab", a->local_player->fields.living.ab, &first))
        return false;
    if (!nsm_integer("LS.ad", a->local_player->fields.living.ad, &first))
        return false;
    if (!nsm_integer("LS.ae", a->local_player->fields.living.ae, &first))
        return false;
    if (!nsm_integer("LS.ai", a->local_player->fields.living.ai, &first))
        return false;
    if (!nsm_integer("LS.an", a->local_player->fields.living.an, &first))
        return false;
    if (!nsm_integer("LS.au", a->local_player->fields.living.au, &first))
        return false;
    if (!nsm_integer("LS.av", a->local_player->fields.living.av, &first))
        return false;
    if (!nsm_integer("LS.aC", a->local_player->fields.living.aC, &first))
        return false;
    if (!nsm_float("LS.F", a->local_player->fields.living.F, &first))
        return false;
    if (!nsm_float("LS.G", a->local_player->fields.living.G, &first))
        return false;
    if (!nsm_float("LS.H", a->local_player->fields.living.H, &first))
        return false;
    if (!nsm_float("LS.I", a->local_player->fields.living.I, &first))
        return false;
    if (!nsm_float("LS.J", a->local_player->fields.living.J, &first))
        return false;
    if (!nsm_float("LS.K", a->local_player->fields.living.K, &first))
        return false;
    if (!nsm_float("LS.L", a->local_player->fields.living.L, &first))
        return false;
    if (!nsm_float("LS.M", a->local_player->fields.living.M, &first))
        return false;
    if (!nsm_float("LS.Q", a->local_player->fields.living.Q, &first))
        return false;
    if (!nsm_float("LS.S", a->local_player->fields.living.S, &first))
        return false;
    if (!nsm_float("LS.U", a->local_player->fields.living.U, &first))
        return false;
    if (!nsm_float("LS.W", a->local_player->fields.living.W, &first))
        return false;
    if (!nsm_float("LS.X", a->local_player->fields.living.X, &first))
        return false;
    if (!nsm_float("LS.ac", a->local_player->fields.living.ac, &first))
        return false;
    if (!nsm_float("LS.af", a->local_player->fields.living.af, &first))
        return false;
    if (!nsm_float("LS.ag", a->local_player->fields.living.ag, &first))
        return false;
    if (!nsm_float("LS.aj", a->local_player->fields.living.aj, &first))
        return false;
    if (!nsm_float("LS.ak", a->local_player->fields.living.ak, &first))
        return false;
    if (!nsm_float("LS.al", a->local_player->fields.living.al, &first))
        return false;
    if (!nsm_float("LS.am", a->local_player->fields.living.am, &first))
        return false;
    if (!nsm_float("LS.at", a->local_player->fields.living.at, &first))
        return false;
    if (!nsm_float("LS.aw", a->local_player->fields.living.aw, &first))
        return false;
    if (!nsm_float("LS.ax", a->local_player->fields.living.ax, &first))
        return false;
    if (!nsm_float("LS.ay", a->local_player->fields.living.ay, &first))
        return false;
    if (!nsm_float("LS.aA", a->local_player->fields.living.aA, &first))
        return false;
    if (!nsm_float("LS.aB", a->local_player->fields.living.aB, &first))
        return false;
    if (!nsm_double("LS.ao", a->local_player->fields.living.ao, &first))
        return false;
    if (!nsm_double("LS.ap", a->local_player->fields.living.ap, &first))
        return false;
    if (!nsm_double("LS.aq", a->local_player->fields.living.aq, &first))
        return false;
    if (!nsm_double("LS.ar", a->local_player->fields.living.ar, &first))
        return false;
    if (!nsm_double("LS.as", a->local_player->fields.living.as, &first))
        return false;
    if (!nsm_boolean("LS.N", a->local_player->fields.living.N, &first))
        return false;
    if (!nsm_boolean("LS.P", a->local_player->fields.living.P, &first))
        return false;
    if (!nsm_boolean("LS.V", a->local_player->fields.living.V, &first))
        return false;
    if (!nsm_boolean("LS.ah", a->local_player->fields.living.ah, &first))
        return false;
    if (!nsm_boolean("LS.az", a->local_player->fields.living.az, &first))
        return false;
    if (!nsm_pointer("LS.O", a->local_player->fields.living.O, &first))
        return false;
    if (!nsm_pointer("LS.R", a->local_player->fields.living.R, &first))
        return false;
    if (!nsm_pointer("LS.private_b", a->local_player->fields.living.private_b, &first))
        return false;
    if (!nsm_integer("GS.f", a->local_player->fields.player.f, &first))
        return false;
    if (!nsm_integer("GS.g", a->local_player->fields.player.g, &first))
        return false;
    if (!nsm_integer("GS.k", a->local_player->fields.player.k, &first))
        return false;
    if (!nsm_integer("GS.m", a->local_player->fields.player.m, &first))
        return false;
    if (!nsm_integer("GS.private_a", a->local_player->fields.player.private_a, &first))
        return false;
    if (!nsm_integer("GS.z", a->local_player->fields.player.z, &first))
        return false;
    if (!nsm_integer("GS.private_bO", a->local_player->fields.player.private_bO, &first))
        return false;
    if (!nsm_float("GS.h", a->local_player->fields.player.h, &first))
        return false;
    if (!nsm_float("GS.i", a->local_player->fields.player.i, &first))
        return false;
    if (!nsm_float("GS.w", a->local_player->fields.player.w, &first))
        return false;
    if (!nsm_float("GS.x", a->local_player->fields.player.x, &first))
        return false;
    if (!nsm_float("GS.y", a->local_player->fields.player.y, &first))
        return false;
    if (!nsm_float("GS.B", a->local_player->fields.player.B, &first))
        return false;
    if (!nsm_float("GS.C", a->local_player->fields.player.C, &first))
        return false;
    if (!nsm_double("GS.o", a->local_player->fields.player.o, &first))
        return false;
    if (!nsm_double("GS.p", a->local_player->fields.player.p, &first))
        return false;
    if (!nsm_double("GS.q", a->local_player->fields.player.q, &first))
        return false;
    if (!nsm_double("GS.r", a->local_player->fields.player.r, &first))
        return false;
    if (!nsm_double("GS.s", a->local_player->fields.player.s, &first))
        return false;
    if (!nsm_double("GS.t", a->local_player->fields.player.t, &first))
        return false;
    if (!nsm_boolean("GS.j", a->local_player->fields.player.j, &first))
        return false;
    if (!nsm_boolean("GS.u", a->local_player->fields.player.u, &first))
        return false;
    if (!nsm_boolean("GS.A", a->local_player->fields.player.A, &first))
        return false;
    if (!nsm_pointer("GS.c", a->local_player->fields.player.c, &first))
        return false;
    if (!nsm_pointer("GS.d", a->local_player->fields.player.d, &first))
        return false;
    if (!nsm_pointer("GS.e", a->local_player->fields.player.e, &first))
        return false;
    if (!nsm_pointer("GS.l", a->local_player->fields.player.l, &first))
        return false;
    if (!nsm_pointer("GS.n", a->local_player->fields.player.n, &first))
        return false;
    if (!nsm_pointer("GS.v", a->local_player->fields.player.v, &first))
        return false;
    if (!nsm_pointer("GS.private_b", a->local_player->fields.player.private_b, &first))
        return false;
    if (!nsm_pointer("GS.private_bN", a->local_player->fields.player.private_bN, &first))
        return false;
    if (!nsm_pointer("GS.D", a->local_player->fields.player.D, &first))
        return false;
    if (!nsm_pointer("DC.input", a->local_player->fields.input, &first))
        return false;
    if (!nsm_pointer("DC.minecraft", a->local_player->fields.minecraft, &first))
        return false;
    if (!nsm_pointer("DC.filter0", a->local_player->fields.filters[0], &first))
        return false;
    if (!nsm_pointer("DC.filter1", a->local_player->fields.filters[1], &first))
        return false;
    if (!nsm_pointer("DC.filter2", a->local_player->fields.filters[2], &first))
        return false;
    if (!nsm_pointer("DC.skin_url", a->local_player->fields.skin_url, &first))
        return false;
    if (!nsm_pointer("native.bounds", a->bounds, &first))
        return false;
    if (!nsm_pointer("native.random", a->random, &first))
        return false;
    if (!nsm_pointer("native.watcher", a->watcher, &first))
        return false;
    if (!nsm_pointer("native.class_literal", a->class_literal, &first))
        return false;
    if (!nsm_integer("native.phase", a->phase, &first))
        return false;
    return fputc('}', nsm_stream) != EOF;
}

static bool nsm_raw_world(const mc_world_runtime *a) {
    bool first = true;
    if (fputc('{', nsm_stream) == EOF)
        return false;
    if (!nsm_boolean("FD.a", a->fields.a, &first))
        return false;
    if (!nsm_boolean("FD.o", a->fields.o, &first))
        return false;
    if (!nsm_boolean("FD.s", a->fields.s, &first))
        return false;
    if (!nsm_boolean("FD.y", a->fields.y, &first))
        return false;
    if (!nsm_boolean("FD.J", a->fields.J, &first))
        return false;
    if (!nsm_boolean("FD.L", a->fields.L, &first))
        return false;
    if (!nsm_boolean("FD.N", a->fields.N, &first))
        return false;
    if (!nsm_boolean("FD.O", a->fields.O, &first))
        return false;
    if (!nsm_boolean("FD.B", a->fields.B, &first))
        return false;
    if (!nsm_integer("FD.H", a->fields.H, &first))
        return false;
    if (!nsm_integer("FD.f", a->fields.f, &first))
        return false;
    if (!nsm_integer("FD.g", a->fields.g, &first))
        return false;
    if (!nsm_integer("FD.h", a->fields.h, &first))
        return false;
    if (!nsm_integer("FD.m", a->fields.m, &first))
        return false;
    if (!nsm_integer("FD.n", a->fields.n, &first))
        return false;
    if (!nsm_integer("FD.I", a->fields.I, &first))
        return false;
    if (!nsm_integer("FD.p", a->fields.p, &first))
        return false;
    if (!nsm_integer("FD.q", a->fields.q, &first))
        return false;
    if (!nsm_integer("FD.M", a->fields.M, &first))
        return false;
    if (!nsm_integer("FD.Q", a->fields.Q, &first))
        return false;
    if (!nsm_float("FD.i", a->fields.i, &first))
        return false;
    if (!nsm_float("FD.j", a->fields.j, &first))
        return false;
    if (!nsm_float("FD.k", a->fields.k, &first))
        return false;
    if (!nsm_float("FD.l", a->fields.l, &first))
        return false;
    if (!nsm_pointer("FD.C", a->fields.C, &first))
        return false;
    if (!nsm_pointer("FD.b", a->fields.b, &first))
        return false;
    if (!nsm_pointer("FD.D", a->fields.D, &first))
        return false;
    if (!nsm_pointer("FD.E", a->fields.E, &first))
        return false;
    if (!nsm_pointer("FD.F", a->fields.F, &first))
        return false;
    if (!nsm_pointer("FD.c", a->fields.c, &first))
        return false;
    if (!nsm_pointer("FD.G", a->fields.G, &first))
        return false;
    if (!nsm_pointer("FD.d", a->fields.d, &first))
        return false;
    if (!nsm_pointer("FD.e", a->fields.e, &first))
        return false;
    if (!nsm_pointer("FD.r", a->fields.r, &first))
        return false;
    if (!nsm_pointer("FD.t", a->fields.t, &first))
        return false;
    if (!nsm_pointer("FD.u", a->fields.u, &first))
        return false;
    if (!nsm_pointer("FD.v", a->fields.v, &first))
        return false;
    if (!nsm_pointer("FD.w", a->fields.w, &first))
        return false;
    if (!nsm_pointer("FD.x", a->fields.x, &first))
        return false;
    if (!nsm_pointer("FD.z", a->fields.z, &first))
        return false;
    if (!nsm_pointer("FD.K", a->fields.K, &first))
        return false;
    if (!nsm_pointer("FD.P", a->fields.P, &first))
        return false;
    if (!nsm_pointer("FD.R", a->fields.R, &first))
        return false;
    return fputc('}', nsm_stream) != EOF;
}

static bool nsm_raw_provider(const mc_world_provider *a) {
    bool first = true;
    mc_world_provider_fields v;
    if (mc_world_provider_fields_get(a, &v) != MC_WORLD_PROVIDER_OK)
        return false;
    if (fputc('{', nsm_stream) == EOF)
        return false;
    if (!nsm_pointer("XA.a", v.a, &first))
        return false;
    if (!nsm_pointer("XA.b", v.b, &first))
        return false;
    if (!nsm_pointer("XA.f", v.f, &first))
        return false;
    if (!nsm_pointer("XA.h", v.h, &first))
        return false;
    if (!nsm_boolean("XA.c", v.c, &first))
        return false;
    if (!nsm_boolean("XA.d", v.d, &first))
        return false;
    if (!nsm_boolean("XA.e", v.e, &first))
        return false;
    if (!nsm_integer("XA.g", v.g, &first))
        return false;
    return fputc('}', nsm_stream) != EOF;
}

static bool nsm_raw_info(const mc_world_info *a) {
    bool first = true;
    mc_world_info_fields v;
    if (mc_world_info_fields_get(a, &v) != MC_WORLD_INFO_OK)
        return false;
    if (fputc('{', nsm_stream) == EOF)
        return false;
    if (!nsm_integer("Info.seed", v.value.seed, &first))
        return false;
    if (!nsm_integer("Info.spawn_x", v.value.spawn_x, &first))
        return false;
    if (!nsm_integer("Info.spawn_y", v.value.spawn_y, &first))
        return false;
    if (!nsm_integer("Info.spawn_z", v.value.spawn_z, &first))
        return false;
    if (!nsm_integer("Info.time", v.value.time, &first))
        return false;
    if (!nsm_integer("Info.last_played", v.value.last_played, &first))
        return false;
    if (!nsm_integer("Info.size_on_disk", v.value.size_on_disk, &first))
        return false;
    if (!nsm_integer("Info.player_dimension", v.value.player_dimension, &first))
        return false;
    if (!nsm_integer("Info.version", v.value.version, &first))
        return false;
    if (!nsm_integer("Info.rain_time", v.value.rain_time, &first))
        return false;
    if (!nsm_integer("Info.thunder_time", v.value.thunder_time, &first))
        return false;
    if (!nsm_boolean("Info.raining", v.value.raining, &first))
        return false;
    if (!nsm_boolean("Info.thundering", v.value.thundering, &first))
        return false;
    if (!nsm_pointer("Info.name", v.name, &first))
        return false;
    if (!nsm_pointer("Info.player", v.player, &first))
        return false;
    return fputc('}', nsm_stream) != EOF;
}

static bool nsm_raw_inventory(const mc_inventory_player *a) {
    bool first = true;
    if (fputc('{', nsm_stream) == EOF)
        return false;
    if (!nsm_pointer("Inventory.main", a->main_slots, &first))
        return false;
    if (!nsm_pointer("Inventory.armor", a->armor_slots, &first))
        return false;
    if (!nsm_pointer("Inventory.owner", a->player, &first))
        return false;
    if (!nsm_pointer("Inventory.cursor", a->cursor, &first))
        return false;
    if (!nsm_integer("Inventory.selected", a->selected, &first))
        return false;
    if (!nsm_boolean("Inventory.dirty", a->changed, &first))
        return false;
    return fputc('}', nsm_stream) != EOF;
}

/* RAW_FIELD_FUNCTIONS_END */

static bool nsm_xyz(const mc_player_coordinates *p) {
    return p == NULL ? fprintf(nsm_stream, "null") >= 0
                     : fprintf(nsm_stream, "[%" PRId32 ",%" PRId32 ",%" PRId32 "]", p->x, p->y,
                               p->z) >= 0;
}

static bool nsm_bounds(const mc_entity_bounds *p) {
    return p == NULL ? fprintf(nsm_stream, "null") >= 0
                     : fprintf(nsm_stream,
                               "[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                               "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
                               movement_double_bits(p->min_x), movement_double_bits(p->min_y),
                               movement_double_bits(p->min_z), movement_double_bits(p->max_x),
                               movement_double_bits(p->max_y), movement_double_bits(p->max_z)) >= 0;
}

static bool nsm_random(const mc_java_random *r) {
    return fprintf(nsm_stream, "[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]", r->state,
                   nsm_bool(r->have_next_next_gaussian),
                   movement_double_bits(r->next_next_gaussian)) >= 0;
}

static bool nsm_runs(const uint8_t *v, size_t length) {
    if (fputc('[', nsm_stream) == EOF)
        return false;
    bool first = true;
    for (size_t i = 0; i < length;) {
        size_t end = i + 1;
        while (end < length && v[end] == v[i])
            ++end;
        if (v[i] != 0) {
            if (fprintf(nsm_stream, "%s[%zu,%zu,%u]", first ? "" : ",", i, end - i,
                        (unsigned int)v[i]) < 0)
                return false;
            first = false;
        }
        i = end;
    }
    return fputc(']', nsm_stream) != EOF;
}

static bool nsm_planes(const mc_chunk_data *d) {
    return fprintf(nsm_stream, "{\"blocks\":") >= 0 && nsm_runs(d->blocks, MC_CHUNK_BLOCKS) &&
           fprintf(nsm_stream, ",\"metadata\":") >= 0 &&
           nsm_runs(d->metadata, MC_CHUNK_NIBBLE_BYTES) && fprintf(nsm_stream, ",\"sky\":") >= 0 &&
           nsm_runs(d->skylight, MC_CHUNK_NIBBLE_BYTES) &&
           fprintf(nsm_stream, ",\"block_light\":") >= 0 &&
           nsm_runs(d->block_light, MC_CHUNK_NIBBLE_BYTES) &&
           fprintf(nsm_stream, ",\"height\":") >= 0 &&
           nsm_runs(d->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) && fputc('}', nsm_stream) != EOF;
}

static bool nsm_stacks(mc_item_stack *const *slots, size_t length) {
    if (fputc('[', nsm_stream) == EOF)
        return false;
    for (size_t i = 0; i < length; ++i) {
        if (i != 0 && fputc(',', nsm_stream) == EOF)
            return false;
        if (slots[i] == NULL) {
            if (fprintf(nsm_stream, "null") < 0)
                return false;
        } else {
            mc_item_stack_fields f;
            if (mc_item_stack_fields_get(slots[i], &f) != MC_ITEM_STACK_OK ||
                fprintf(nsm_stream,
                        "{\"identity\":\"%016" PRIxPTR "\",\"fields\":{\"iz.a\":%" PRId32
                        ",\"iz.b\":%" PRId32 ",\"iz.c\":%" PRId32 ",\"iz.d\":%" PRId32 "}}",
                        (uintptr_t)slots[i], f.count, f.animation, f.id, f.damage) < 0)
                return false;
        }
    }
    return fputc(']', nsm_stream) != EOF;
}

static bool nsm_tag(const mc_nbt_tag *tag, unsigned int depth) {
    if (tag == NULL || depth > 8 ||
        fprintf(nsm_stream, "{\"kind\":%u", (unsigned int)tag->type) < 0)
        return false;
    if (tag->type == MC_NBT_COMPOUND) {
        if (fprintf(nsm_stream, ",\"values\":{") < 0)
            return false;
        for (size_t i = 0; i < tag->value.compound.length; ++i) {
            const mc_nbt_tag *child = tag->value.compound.items[i];
            if (child == NULL || child->name.length > 24 ||
                fprintf(nsm_stream, "%s\"", i == 0 ? "" : ",") < 0)
                return false;
            for (size_t j = 0; j < child->name.length; ++j) {
                const uint16_t c = child->name.units[j];
                if (c < 32 || c > 126 || c == '"' || c == '\\' || fputc((int)c, nsm_stream) == EOF)
                    return false;
            }
            if (fprintf(nsm_stream, "\":") < 0 || !nsm_tag(child, depth + 1))
                return false;
        }
        if (fputc('}', nsm_stream) == EOF)
            return false;
    } else if (tag->type == MC_NBT_LIST) {
        if (fprintf(nsm_stream, ",\"element_type\":%u,\"values\":[",
                    (unsigned int)tag->value.list.element_type) < 0)
            return false;
        for (size_t i = 0; i < tag->value.list.length; ++i) {
            if (i != 0 && fputc(',', nsm_stream) == EOF)
                return false;
            if (!nsm_tag(tag->value.list.items[i], depth + 1))
                return false;
        }
        if (fputc(']', nsm_stream) == EOF)
            return false;
    } else {
        if (fprintf(nsm_stream, ",\"value\":") < 0)
            return false;
        if (tag->type == MC_NBT_FLOAT) {
            if (fprintf(nsm_stream, "{\"F\":\"%08" PRIx32 "\"}", tag->value.float_bits) < 0)
                return false;
        } else if (tag->type == MC_NBT_DOUBLE) {
            if (fprintf(nsm_stream, "{\"D\":\"%016" PRIx64 "\"}", tag->value.double_bits) < 0)
                return false;
        } else if (tag->type == MC_NBT_BYTE || tag->type == MC_NBT_SHORT ||
                   tag->type == MC_NBT_INT) {
            const int32_t v = tag->type == MC_NBT_BYTE    ? tag->value.byte_value
                              : tag->type == MC_NBT_SHORT ? tag->value.short_value
                                                          : tag->value.int_value;
            if (fprintf(nsm_stream, "%" PRId32, v) < 0)
                return false;
        } else
            return false;
    }
    return fputc('}', nsm_stream) != EOF;
}

static bool nsm_selected(const nsm_fixture *s) {
    const mc_entity *a = s->owner.player;
    const mc_player_fields *p = &a->local_player->fields.player;
    const mc_living_entity_fields *l = &a->local_player->fields.living;
    const mc_world_runtime_fields *w = &s->owner.world->fields;
    mc_world_info_player *info_player = NULL;
    uint8_t metadata = 0;
    if (mc_world_info_player_get(w->x, &info_player) != MC_WORLD_INFO_OK ||
        !mc_chunk_nibbles_get(s->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, &metadata))
        return false;
    if (fprintf(nsm_stream,
                "{\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                "\"],\"history\":[{\"D\":\"%016" PRIx64 "\"},{\"D\":\"%016" PRIx64
                "\"},{\"D\":\"%016" PRIx64 "\"},{\"D\":\"%016" PRIx64 "\"},{\"D\":\"%016" PRIx64
                "\"},{\"D\":\"%016" PRIx64 "\"}],\"base_flags\":{\"bk\":{\"F\":\"%08" PRIx32
                "\"},\"bv\":%" PRId32 ",\"bz\":%" PRId32 ",\"aX\":%s},\"dimensions\":[\"%08" PRIx32
                "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\"],\"angles\":[\"%08" PRIx32 "\",\"%08" PRIx32
                "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\"],\"sleep_offsets\":[\"%08" PRIx32
                "\",\"%08" PRIx32 "\"],\"living\":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
                "],\"dimension\":%" PRId32 ",\"sleeping\":%s,\"timer\":%" PRId32
                ",\"score\":%" PRId32 ",\"v\":",
                movement_double_bits(a->base.x), movement_double_bits(a->base.y),
                movement_double_bits(a->base.z), movement_double_bits(a->base.velocity_x),
                movement_double_bits(a->base.velocity_y), movement_double_bits(a->base.velocity_z),
                movement_double_bits(a->base.aJ), movement_double_bits(a->base.aK),
                movement_double_bits(a->base.aL), movement_double_bits(a->base.bl),
                movement_double_bits(a->base.bm), movement_double_bits(a->base.bn),
                movement_float_bits(a->base.bk), a->base.bv, a->base.bz, nsm_bool(a->base.aX),
                movement_float_bits(a->base.width), movement_float_bits(a->base.height),
                movement_float_bits(a->base.offset), movement_float_bits(a->base.yaw),
                movement_float_bits(a->base.aT), movement_float_bits(a->base.aU),
                movement_float_bits(a->base.aV), movement_float_bits(p->w),
                movement_float_bits(p->y), l->Y, l->aa, l->ad, l->ae, p->m, nsm_bool(p->u),
                p->private_a, p->g) < 0 ||
        !nsm_xyz(p->v) || fprintf(nsm_stream, ",\"Q\":") < 0 || !nsm_xyz(p->private_b) ||
        fprintf(nsm_stream, ",\"private_bN\":") < 0 || !nsm_xyz(p->private_bN) ||
        fprintf(nsm_stream, ",\"bounds\":") < 0 || !nsm_bounds(a->bounds) ||
        fprintf(nsm_stream, ",\"old_bounds\":") < 0 || !nsm_bounds(s->bounds))
        return false;
    if (fprintf(nsm_stream,
                ",\"world_J\":%s,\"world_same\":%s,\"bounds_same\":%s,\"v_same\":%s,\"Q_same\":%s,"
                "\"Q_v_same\":%s,\"private_bN_same\":%s,\"world_players\":%zu,\"world_entities\":%"
                "zu,\"player_list_same\":%s,\"player_list_members\":[",
                nsm_bool(w->J), nsm_bool(a->base.world == s->owner.world),
                nsm_bool(a->bounds == s->bounds), nsm_bool((uintptr_t)p->v == s->before_v),
                nsm_bool((uintptr_t)p->private_b == s->before_q), nsm_bool(p->private_b == p->v),
                nsm_bool((uintptr_t)p->private_bN == s->before_bn), w->d->count, w->b->count,
                nsm_bool((uintptr_t)w->d == s->before_list)) < 0)
        return false;
    for (size_t i = 0; i < w->d->count; ++i) {
        const mc_entity *member = w->d->items[i];
        if (member == NULL || !mc_entity_is_kind(member, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
            fprintf(nsm_stream, "%s{\"class\":\"dc\",\"actor\":%s}", i ? "," : "",
                    nsm_bool(member == a)) < 0)
            return false;
    }
    return fprintf(nsm_stream,
                   "],\"client_null\":%s,\"main_same\":%s,\"armor_same\":%s,\"inventory_selected\":"
                   "%" PRId32 ",\"inventory_dirty\":%s,\"info_same\":%s,\"info_player_present\":%s,"
                   "\"info_player_input\":%s,\"main\":",
                   nsm_bool(a->local_player->fields.minecraft == NULL),
                   nsm_bool((uintptr_t)p->c->main_slots == s->before_main),
                   nsm_bool((uintptr_t)p->c->armor_slots == s->before_armor), p->c->selected,
                   nsm_bool(p->c->changed), nsm_bool(w->x == s->before_info),
                   nsm_bool(info_player != NULL),
                   nsm_bool(info_player != NULL &&
                            mc_world_info_player_root(info_player) == s->tag)) >= 0 &&
           nsm_stacks(p->c->main_slots, 36) && fprintf(nsm_stream, ",\"armor\":") >= 0 &&
           nsm_stacks(p->c->armor_slots, 4) &&
           fprintf(nsm_stream, ",\"head\":[%u,%u],\"planes\":",
                   (unsigned int)s->data->blocks[boxes_index(8, 64, 8)],
                   (unsigned int)metadata) >= 0 &&
           nsm_planes(s->data) && fputc('}', nsm_stream) != EOF;
}

static bool nsm_snapshot(const nsm_fixture *s) {
    const mc_entity *a = s->owner.player;
    if (fprintf(nsm_stream, "{\"selected\":") < 0 || !nsm_selected(s) ||
        fprintf(nsm_stream, ",\"actor_fields\":") < 0 || !nsm_raw_actor(a) ||
        fprintf(nsm_stream, ",\"world_fields\":") < 0 || !nsm_raw_world(s->owner.world) ||
        fprintf(nsm_stream, ",\"provider_fields\":") < 0 ||
        !nsm_raw_provider(s->owner.world->fields.t) ||
        fprintf(nsm_stream, ",\"info_fields\":") < 0 || !nsm_raw_info(s->owner.world->fields.x) ||
        fprintf(nsm_stream, ",\"inventory_fields\":") < 0 ||
        !nsm_raw_inventory(a->local_player->fields.player.c) ||
        fprintf(nsm_stream, ",\"old_slot_value_view\":") < 0 || !nsm_stacks(s->old_slots, 40) ||
        fprintf(nsm_stream, ",\"rooted_entry_coordinates\":[") < 0)
        return false;
    for (size_t i = 0; i < 3; ++i) {
        if (i && fputc(',', nsm_stream) == EOF)
            return false;
        if (!nsm_xyz(s->pins[i] == NULL ? NULL : &s->pins[i]->value))
            return false;
    }
    return fprintf(nsm_stream, "],\"input_tree\":") >= 0 && nsm_tag(s->tag, 0) &&
           fprintf(nsm_stream, ",\"random\":[") >= 0 && nsm_random(a->random) &&
           fputc(',', nsm_stream) != EOF && nsm_random(s->owner.world->fields.r) &&
           fprintf(nsm_stream, "]}") >= 0;
}

static bool nsm_input(nsm_fixture *s, const int32_t p[NSM_COLUMNS]) {
    bool ok = false;
    nbt_input *a = &s->input;
    CHECK(nbt_make_input(a, "sleep") != NULL);
    nbt_input_node *root = &a->nodes[0];
    const double pos[] = {8.5, 64.9375, 8.5};
    CHECK(nbt_vector(a, root, "Pos", MC_NBT_DOUBLE, pos, 3) &&
          nbt_scalar(a, root, "Health", MC_NBT_SHORT, p[9]));
    const char *keys[] = {"Sleeping", "SleepTimer", "SpawnX", "SpawnY", "SpawnZ", "Score"};
    const int kinds[] = {5, 7, 11, 13, 15, 17}, values[] = {6, 8, 12, 14, 16, 18};
    for (size_t i = 0; i < COUNT(keys); ++i) {
        nbt_sleep_omit(root, keys[i]);
        const bool present = i < 2 || i == 5 || (p[10] & (1 << (i - 2))) != 0;
        if (present && p[kinds[i]] != 0)
            CHECK(nbt_scalar(a, root, keys[i], (mc_nbt_type)p[kinds[i]], p[values[i]]));
    }
    if (p[23] != 9) {
        nbt_sleep_omit(root, "Inventory");
        CHECK(nbt_scalar(a, root, "Inventory", (mc_nbt_type)p[23], 7));
    }
    s->tag = &root->tag;
    ok = true;
cleanup:
    return ok;
}

static bool nsm_prepare(nsm_fixture *s, fixture *f, client_application *app,
                        const int32_t p[NSM_COLUMNS]) {
    bool ok = false;
    mc_item_stack *common = NULL;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL, *token = NULL;
    uint8_t map[256];
    char path[8192];
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const int n = snprintf(path, sizeof(path), "%s/../nbt-matrix-%" PRId32,
                           mc_world_store_root(f->stores[0]), p[0]);
    const mc_world_store_options options = {.session_token = 701u + (uint32_t)p[0],
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
    static const uint16_t name[] = {'N', 'b', 't', 'S', 'l', 'e', 'e',
                                    'p', 'M', 'a', 't', 'r', 'i', 'x'};
    static const uint16_t sid[] = {'-'};
    CHECK(n > 0 && (size_t)n < sizeof(path) &&
          mc_world_store_open(path, f->regions, &options, &s->owner.store, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_info_name_copy((mc_nbt_string){name, COUNT(name)}, SIZE_MAX, &s->owner.name) ==
              MC_WORLD_INFO_OK &&
          mc_world_provider_create(0, NULL, &s->owner.dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 512;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          &limits, &s->owner.world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {.store = s->owner.store,
                                                   .name = s->owner.name,
                                                   .seed = 5143,
                                                   .dimension = s->owner.dimension};
    CHECK(mc_world_runtime_construct_world(s->owner.world, &args, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(s->owner.world->fields.v, &s->owner.provider, NULL) ==
              MC_WORLD_STORAGE_OK);
    s->owner.bound = true;
    mc_world_runtime *world = s->owner.world;
    mc_chunk_state *chunk = NULL;
    CHECK(mc_world_info_spawn_set(world->fields.x, 8, 64, 8) == MC_WORLD_INFO_OK &&
          mc_world_runtime_get(world, 0, 0, &chunk) == MC_CHUNK_ACQUIRE_OK && chunk != NULL &&
          !chunk->blank && chunk->data != NULL);
    s->data = chunk->data;
    memset(s->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(s->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(s->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(s->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(s->data->heightmap, 64, MC_CHUNK_HEIGHTMAP_BYTES);
    s->data->minimum_height = 64;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z) {
            CHECK(nbt_sleep_cell_input(s->data, x, 63, z, 2, 0));
            for (int32_t y = 64; y < 128; ++y)
                CHECK(mc_chunk_nibbles_set(s->data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z, 15));
        }
    static const int32_t facing[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};
    const int32_t direction = p[3], bed = p[4];
    CHECK(nbt_sleep_cell_input(s->data, 8, 64, 8,
                               bed == 1   ? 0
                               : bed == 2 ? 1
                                          : 26,
                               (uint8_t)(bed == 1   ? 0
                                         : bed == 2 ? direction
                                                    : direction | 12)) &&
          nbt_sleep_cell_input(s->data, 8 - facing[direction][0], 64, 8 - facing[direction][1],
                               bed == 1 || bed == 2 ? 0 : 26,
                               (uint8_t)(bed == 1 || bed == 2 ? 0 : direction)));
    if (bed == 3)
        for (int32_t x = 5; x <= 11; ++x)
            for (int32_t z = 5; z <= 11; ++z)
                CHECK(nbt_sleep_cell_input(s->data, x, 65, z, 1, 0));
    CHECK(mc_player_session_classes_create(app->entities, app->registry, &s->sessions, NULL) ==
              MC_PLAYER_OK &&
          mc_player_string_create(name, COUNT(name), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK &&
          mc_player_session_create(s->sessions, username, token, &session, NULL) == MC_PLAYER_OK &&
          mc_local_player_allocate(app->entities, &s->owner.player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(s->owner.player, world, NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          mc_entity_set_position(s->owner.player, 8.5, 64.0 + (double)1.62f, 8.5) == MC_ENTITY_OK);
    mc_entity *actor = s->owner.player;
    mc_player_fields *fields = &actor->local_player->fields.player;
    const mc_player_coordinates q = {42, 66, 44};
    CHECK(mc_player_saved_spawn_set(actor, p[19] ? &q : NULL, NULL) == MC_PLAYER_OK);
    struct mc_player_nbt_coordinate *bn = NULL;
    CHECK(mc_player_coordinate_new_held(actor, (mc_player_site){0}, "authored NBT matrix bN input",
                                        &bn, NULL) == MC_PLAYER_OK);
    bn->value = (mc_player_coordinates){-11, 65, 13};
    mc_player_nbt_coordinate_store_held(actor, bn, &fields->private_bN);
    fields->g = 900;
    CHECK(mc_item_stack_allocate(&common) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(common, 2, 5, 9) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(fields->c, 0, common, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_set(fields->c, 36, common, NULL) == MC_INVENTORY_OK &&
          nsm_input(s, p));
    fields->c->selected = 7;
    fields->c->changed = true;
    if (p[24]) {
        CHECK(mc_world_info_player_copy(s->tag, NULL, &s->saved, NULL) == MC_WORLD_INFO_OK &&
              mc_world_info_player_set(world->fields.x, s->saved) == MC_WORLD_INFO_OK);
        s->tag = mc_world_info_player_root(s->saved);
    }
    world->fields.B = p[22] != 0;
    if (p[25]) {
        /* Actual FD publication, then actual GS.b and World.y. No J/u store. */
        CHECK(!p[24] && world->fields.d->count == 0 && world->fields.b->count == 0 &&
              mc_world_runtime_load_player(world, actor, &s->load) == MC_WORLD_STORAGE_OK &&
              s->load.source_returned && !s->load.saved_data_present && !s->load.nbt_returned &&
              mc_world_player_load_error_retire(&s->load));
        mc_world_player_load_error_init(&s->load);
        world->fields.f = 5;
        const mc_player_sleep_context context = {.code_registry = app->registry};
        mc_player_sleep_state state = {0};
        mc_player_sleep_result_view result = {0};
        CHECK(mc_player_try_sleep_begin(actor, 8, 64, 8, &context, &s->preparation, NULL) ==
                  MC_PLAYER_SLEEP_OK &&
              mc_player_sleep_inspect(s->preparation, &state) && state.returned &&
              state.result != NULL && mc_player_sleep_result_inspect(state.result, &result) &&
              result.ordinal == 0 && fields->u && world->fields.J &&
              mc_player_sleep_retire(&s->preparation, NULL) == MC_PLAYER_SLEEP_OK);
    }
    if (p[20])
        actor->base.world = NULL;
    s->bounds = actor->bounds;
    if (p[21])
        actor->bounds = NULL;
    s->before_v = (uintptr_t)fields->v;
    s->before_q = (uintptr_t)fields->private_b;
    s->before_bn = (uintptr_t)fields->private_bN;
    s->before_main = (uintptr_t)fields->c->main_slots;
    s->before_armor = (uintptr_t)fields->c->armor_slots;
    s->before_list = (uintptr_t)world->fields.d;
    s->before_info = world->fields.x;
    const mc_player_coordinates *coordinates[] = {fields->v, fields->private_b, fields->private_bN};
    for (size_t i = 0; i < 3; ++i)
        CHECK(mc_player_coordinate_pin_held(actor, coordinates[i], &s->pins[i], NULL) ==
              MC_PLAYER_OK);
    for (size_t i = 0; i < 40; ++i) {
        mc_item_stack *stack = i < 36 ? fields->c->main_slots[i] : fields->c->armor_slots[i - 36];
        if (stack != NULL)
            CHECK(mc_item_stack_retain(stack) == MC_ITEM_STACK_OK);
        s->old_slots[i] = stack;
    }
    CHECK(world->fields.d->count == (size_t)p[25] && world->fields.b->count == (size_t)p[25]);
    ok = true;
cleanup:
    mc_item_stack_release(common);
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    mc_player_string_release(token);
    return ok;
}

static bool nsm_site(mc_player_site site) {
    return site.class_name == NULL
               ? fprintf(nsm_stream, "null") >= 0
               : site.method != NULL && site.descriptor != NULL &&
                     fprintf(nsm_stream,
                             "{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%u}",
                             site.class_name, site.method, site.descriptor, site.pc) >= 0;
}

static bool nsm_text(const char *s) {
    return s == NULL ? fprintf(nsm_stream, "null") >= 0 : fprintf(nsm_stream, "\"%s\"", s) >= 0;
}

static bool nsm_diagnostic(const nsm_fixture *s) {
    const mc_entity_nbt_error *e = s->attached ? &s->load.nbt : &s->read;
    if (fprintf(nsm_stream, "{\"nbt_status\":\"%s\",\"nbt_code\":%d,\"field\":",
                mc_entity_nbt_status_string(e->status), (int)e->status) < 0 ||
        !nsm_text(e->field) ||
        fprintf(nsm_stream,
                ",\"base_pc\":%u,\"living_pc\":%u,\"player_pc\":%u,\"local_pc\":%u,\"owned_work\":%"
                "s,\"sleep\":",
                e->base_bytecode,
                e->late_hook == MC_ENTITY_NBT_LATE_PLAYER ? e->living_bytecode : 0,
                e->player_bytecode, e->local_player_bytecode, nsm_bool(e->sleep_work != NULL)) < 0)
        return false;
    if (e->sleep == NULL) {
        if (fprintf(nsm_stream, "null") < 0)
            return false;
    } else {
        const mc_player_sleep_error *w = e->sleep;
        if (fprintf(nsm_stream,
                    "{\"status\":%d,\"source_null\":%s,\"source_exception\":%s,\"source_error\":%s,"
                    "\"native_policy\":%s,\"exception_class\":",
                    (int)w->status, nsm_bool(w->status == MC_PLAYER_SLEEP_SOURCE_NULL),
                    nsm_bool(w->source_exception), nsm_bool(w->source_error),
                    nsm_bool(w->native_policy)) < 0 ||
            !nsm_text(w->exception_class) || fprintf(nsm_stream, ",\"site\":") < 0 ||
            !nsm_site(w->site) || fprintf(nsm_stream, ",\"child\":") < 0 || !nsm_site(w->child) ||
            fputc('}', nsm_stream) == EOF)
            return false;
    }
    if (fprintf(nsm_stream, ",\"fd\":") < 0)
        return false;
    if (!s->attached)
        return fprintf(nsm_stream, "null}") >= 0;
    const mc_world_player_load_error *f = &s->load;
    return fprintf(nsm_stream,
                   "{\"status\":%d,\"pc\":%u,\"saved_data_present\":%s,\"nbt_returned\":%s,\"info_"
                   "cleared\":%s,\"publication_returned\":%s,\"source_returned\":%s,\"source_"
                   "exception\":%s,\"source_error\":%s,\"native_policy\":%s,\"catch_pc\":%u}}",
                   (int)f->status, f->bytecode, nsm_bool(f->saved_data_present),
                   nsm_bool(f->nbt_returned), nsm_bool(f->info_cleared),
                   nsm_bool(f->publication_returned), nsm_bool(f->source_returned),
                   nsm_bool(f->source_exception), nsm_bool(f->source_error),
                   nsm_bool(f->native_policy), f->catch_bytecode) >= 0;
}

static FILE *nsm_temporary(void) {
    FILE *snapshot_stream = NULL;
#if defined(_MSC_VER)
    if (tmpfile_s(&snapshot_stream) != 0)
        snapshot_stream = NULL;
#else
    snapshot_stream = tmpfile();
#endif
    return snapshot_stream;
}

static bool nsm_copy(FILE *source) {
    if (fflush(source) != 0 || fseek(source, 0, SEEK_SET) != 0)
        return false;
    unsigned char bytes[4096];
    size_t count;
    while ((count = fread(bytes, 1, sizeof(bytes), source)) != 0)
        if (fwrite(bytes, 1, count, nsm_stream) != count)
            return false;
    return !ferror(source);
}

static bool nsm_retire(nsm_fixture *s) {
    if (!mc_entity_nbt_error_retire(&s->read) || !mc_world_player_load_error_retire(&s->load) ||
        mc_player_sleep_retire(&s->preparation, NULL) != MC_PLAYER_SLEEP_OK)
        return false;
    if (s->owner.player != NULL) {
        for (size_t i = 0; i < 3; ++i) {
            if (s->pins[i] != NULL && mc_player_coordinate_unpin_admit_held(
                                          s->owner.player, s->pins[i], 1) != MC_PLAYER_OK)
                return false;
            mc_player_coordinate_unpin_commit_held(s->owner.player, &s->pins[i]);
        }
        s->owner.player->base.world = s->owner.world;
        if (s->bounds != NULL)
            s->owner.player->bounds = s->bounds;
    }
    for (size_t i = 0; i < 40; ++i) {
        mc_item_stack_release(s->old_slots[i]);
        s->old_slots[i] = NULL;
    }
    mc_world_info_player_release(s->saved);
    s->saved = NULL;
    if (!nbt_metadata_close_world(&s->owner) ||
        mc_world_store_free_checked(&s->owner.store, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (mc_player_session_classes_free(&s->sessions, NULL) != MC_PLAYER_OK)
        return false;
    mc_world_info_name_release(s->owner.name);
    s->owner.name = NULL;
    return true;
}

static bool nsm_row(fixture *f, client_application *app, const int32_t p[NSM_COLUMNS],
                    const char *name) {
    bool ok = false;
    nsm_fixture *s = calloc(1, sizeof(*s));
    FILE *before = NULL, *after = NULL, *retired = NULL, *diagnostic = NULL;
    const unsigned int entered = app->registry->entered;
    CHECK(s != NULL);
    mc_entity_nbt_error_init(&s->read);
    mc_world_player_load_error_init(&s->load);
    CHECK(nsm_prepare(s, f, app, p));
    s->attached = p[2] != 0;
    before = nsm_temporary();
    after = nsm_temporary();
    retired = nsm_temporary();
    diagnostic = nsm_temporary();
    CHECK(before != NULL && after != NULL && retired != NULL && diagnostic != NULL);
    nsm_stream = before;
    CHECK(nsm_snapshot(s) && fflush(before) == 0 && fflush(stdout) == 0);
    const size_t borrows = app->registry->runtime_borrows,
                 bindings = s->owner.world->persistent_bindings;
    if (s->attached)
        (void)mc_world_runtime_load_player(s->owner.world, s->owner.player, &s->load);
    else
        (void)mc_entity_read_nbt_in_context(s->owner.player, app->registry, s->tag, NULL, &s->read);
    nsm_stream = after;
    CHECK(nsm_snapshot(s));
    nsm_stream = diagnostic;
    CHECK(nsm_diagnostic(s));
    mc_entity_nbt_error *e = s->attached ? &s->load.nbt : &s->read;
    const bool owned = e->sleep_work != NULL, failed_wake = e->sleep != NULL;
    const mc_entity_nbt_status status = e->status;
    const char *const operation = e->operation, *const field = e->field;
    const unsigned int base_pc = e->base_bytecode, player_pc = e->player_bytecode,
                       local_pc = e->local_player_bytecode;
    const size_t source_holds = s->owner.player->holds;
    bool refused = false;
    if (owned) {
        app->registry->entered = 1; /* Explicit native cleanup input AFTER parent terminal. */
        refused = s->attached ? !mc_world_player_load_error_retire(&s->load)
                              : !mc_entity_nbt_error_retire(e);
        app->registry->entered = entered;
        CHECK(refused && e->status == status && e->operation == operation && e->field == field &&
              e->base_bytecode == base_pc && e->player_bytecode == player_pc &&
              e->local_player_bytecode == local_pc);
    }
    CHECK((s->attached ? mc_world_player_load_error_retire(&s->load)
                       : mc_entity_nbt_error_retire(e)) &&
          e->sleep_work == NULL && e->sleep == NULL && e->sleep_cleanup == NULL &&
          e->status == status && e->operation == operation && e->field == field &&
          e->base_bytecode == base_pc && e->player_bytecode == player_pc &&
          e->local_player_bytecode == local_pc && app->registry->runtime_borrows == borrows &&
          s->owner.world->persistent_bindings == bindings &&
          s->owner.player->holds == source_holds - (failed_wake ? 1u : 0u));
    nsm_stream = retired;
    CHECK(nsm_snapshot(s));
    nsm_stream = stdout;
    CHECK(fprintf(stdout, "NBT_SLEEP_NATIVE {\"case\":%" PRId32 ",\"name\":\"%s\",\"input\":[",
                  p[0], name) >= 0);
    for (size_t i = 0; i < NSM_COLUMNS; ++i) {
        if (i && fputc(',', stdout) == EOF)
            goto cleanup;
        if (i == 1) {
            if (fprintf(stdout, "\"%s\"", name) < 0)
                goto cleanup;
        } else if (fprintf(stdout, "%" PRId32, p[i]) < 0)
            goto cleanup;
    }
    CHECK(fprintf(stdout,
                  "],\"entry\":\"%s\",\"before\":", s->attached ? "FD_ATTACH" : "SN_READ") >= 0 &&
          nsm_copy(before) && fprintf(stdout, ",\"after\":") >= 0 && nsm_copy(after) &&
          fprintf(stdout, ",\"diagnostic\":") >= 0 && nsm_copy(diagnostic) &&
          fprintf(stdout, ",\"retired\":") >= 0 && nsm_copy(retired) &&
          fprintf(stdout,
                  ",\"cleanup\":{\"refusal_attempted\":%s,\"refused\":%s,\"views_cleared\":true,"
                  "\"first_cause_preserved\":true,\"entry_counts_balanced\":true}}\n",
                  nsm_bool(owned), nsm_bool(refused)) >= 0 &&
          fflush(stdout) == 0);
    ok = true;
cleanup:
    app->registry->entered = entered;
    nsm_stream = stdout;
    if (before != NULL && fclose(before) != 0)
        ok = false;
    if (after != NULL && fclose(after) != 0)
        ok = false;
    if (retired != NULL && fclose(retired) != 0)
        ok = false;
    if (diagnostic != NULL && fclose(diagnostic) != 0)
        ok = false;
    if (s != NULL) {
        if (!nsm_retire(s))
            return false; /* Preserve refused whole owner; no dangling stack context. */
        free(s);
    }
    if (!ok)
        fprintf(stderr, "NBT sleep matrix case%" PRId32 " %s failed\n", p[0], name);
    return ok;
}

static bool nbt_sleep_matrix_cases(fixture *f, client_application *app, const char *profiles) {
    bool ok = false;
    FILE *input = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&input, profiles, "rb") != 0)
        input = NULL;
#else
    input = fopen(profiles, "rb");
#endif
    size_t rows = 0;
    char line[1024], name[80];
    CHECK(input != NULL);
    while (fgets(line, sizeof(line), input) != NULL) {
        CHECK(rows < NSM_CASES && (strchr(line, '\n') != NULL || feof(input)));
        char *words[NSM_COLUMNS + 1] = {0}, *cursor = line;
        size_t count = 0;
        while (*cursor != '\0') {
            while (*cursor == ' ' || *cursor == '\r' || *cursor == '\n')
                ++cursor;
            if (*cursor == '\0')
                break;
            CHECK(count < COUNT(words));
            words[count++] = cursor;
            while (*cursor != '\0' && *cursor != ' ' && *cursor != '\r' && *cursor != '\n')
                ++cursor;
            if (*cursor != '\0')
                *cursor++ = '\0';
        }
        CHECK(count == NSM_COLUMNS && strlen(words[1]) < sizeof(name));
        memcpy(name, words[1], strlen(words[1]) + 1);
        for (size_t i = 0; name[i] != '\0'; ++i)
            CHECK((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9') ||
                  name[i] == '_');
        int32_t values[NSM_COLUMNS] = {0};
        for (size_t i = 0; i < NSM_COLUMNS; ++i)
            if (i != 1) {
                errno = 0;
                char *end = NULL;
                const long long v = strtoll(words[i], &end, 10);
                CHECK(errno == 0 && end != words[i] && *end == '\0' && v >= INT32_MIN &&
                      v <= INT32_MAX);
                values[i] = (int32_t)v;
            }
        CHECK(values[0] == (int32_t)rows && (values[2] == 0 || values[2] == 1) && values[3] >= 0 &&
              values[3] < 4 && values[4] >= 0 && values[4] <= 3 && values[10] >= 0 &&
              values[10] <= 7 && (values[23] == 3 || values[23] == 9));
        const size_t kinds[] = {5, 7, 11, 13, 15, 17}, flags[] = {19, 20, 21, 22, 24, 25};
        for (size_t i = 0; i < COUNT(kinds); ++i)
            CHECK(values[kinds[i]] >= 0 && values[kinds[i]] <= 3);
        for (size_t i = 0; i < COUNT(flags); ++i)
            CHECK(values[flags[i]] == 0 || values[flags[i]] == 1);
        CHECK(nsm_row(f, app, values, name));
        ++rows;
    }
    CHECK(!ferror(input) && rows == NSM_CASES);
    ok = true;
cleanup:
    if (input != NULL && fclose(input) != 0)
        ok = false;
    return ok;
}
