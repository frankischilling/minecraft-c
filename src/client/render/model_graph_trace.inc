/* Native diagnostics only. The actual scalar geometry body is unchanged. */
static void current_part(mc_client_model_graph *o, char part_name) {
    if (o->detail != NULL) {
        o->detail->part = part_name;
        o->detail->face = o->detail->vertex = o->detail->uv_copy = -1;
    }
}

static void root_store(mc_client_model_graph *o, const char *source, int32_t pc) {
    if (o->progress != NULL)
        o->progress->last_root_store = (mc_client_model_site){
            source, "<init>", strcmp(source, "ko") == 0 ? "()V" : "(FF)V", pc};
}

static void frame(mc_client_biped_error *e, const char *source, const char *method,
                  const char *descriptor, int32_t pc) {
    if (e->frame_count < sizeof(e->frames) / sizeof(e->frames[0]))
        e->frames[e->frame_count++] = (mc_client_model_site){source, method, descriptor, pc};
}

static void trace_failure(mc_client_model_graph *o, const char *source, int32_t pc) {
    if (o->detail == NULL)
        return;
    mc_client_biped_error *e = o->detail;
    e->status = o->failure->status;
    e->operation = "normal Biped genuine child";
    e->native_policy = true; /* allocation/Vec3 policies, not Java OOME timing */
    e->frame_count = 0;
    const char *parts = "ihabcdefg";
    const int32_t box_call[9] = {47, 80, 113, 162, 207, 252, 306, 350, 403};
    const int32_t vertex_call[8] = {101, 117, 135, 151, 166, 183, 202, 219};
    const int32_t quad_call[6] = {363, 430, 497, 570, 641, 724};
    const int32_t uv_call[4] = {35, 60, 86, 113};
    size_t part_index = 0;
    while (part_index < 9 && parts[part_index] != e->part)
        ++part_index;
    if (strcmp(source, "fh") == 0) {
        frame(e, "fh", "<init>", "(FF)V", pc);
    } else if (strcmp(source, "ps") == 0) {
        frame(e, "ps", "a", "(FFFIIIF)V", pc);
    } else if (strcmp(source, "tz") == 0) {
        frame(e, "tz", "a", "()V", pc);
        frame(e, "ps", "a", "(FFFIIIF)V", 755);
    } else if (strcmp(source, "ib") == 0 && pc == 0) {
        frame(e, "ib", "a", "(FF)Lib;", 0);
        if (e->uv_copy >= 0 && e->uv_copy < 4)
            frame(e, "tz", "<init>", "([Lib;IIII)V", uv_call[e->uv_copy]);
        if (e->face >= 0 && e->face < 6)
            frame(e, "ps", "a", "(FFFIIIF)V", quad_call[e->face]);
    } else if (strcmp(source, "ib") == 0) {
        frame(e, "ib", "<init>", "(FFFFF)V", pc);
        if (e->vertex >= 0 && e->vertex < 8)
            frame(e, "ps", "a", "(FFFIIIF)V", vertex_call[e->vertex]);
    }
    if (strcmp(source, "fh") != 0 && part_index < 9)
        frame(e, "fh", "<init>", "(FF)V", box_call[part_index]);
    frame(e, "fh", "<init>", "(F)V", 3);
}
