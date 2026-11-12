#include "download_thread_internal.h"
#include "mc/core/java/ascii_operand.h"

static mc_client_download_status source(mc_client_download_error *e, int pc, const char *exception,
                                        const char *op) {
    mc_download_fail(e, MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION, "net/minecraft/client/Minecraft", "a",
                     "(Ljava/lang/String;Ljava/io/File;)V", pc, op);
    e->exception_class = exception;
    return e->status;
}

mc_client_download_status mc_download_register(mc_client_download_thread *w,
                                               client_application *app, mc_world_info_name *name,
                                               mc_java_file *file, mc_client_download_error *e) {
    /* Null invoke receiver is rejected by the calling cz invocation, before
     * entering any MC String operation. The caller adds its exact97/133. */
    if (app == NULL) {
        mc_download_fail(e, MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION, NULL, NULL, NULL, -1,
                         "actual null Minecraft invocation receiver");
        e->exception_class = "java/lang/NullPointerException";
        return e->status;
    }
    if (name == NULL)
        return source(e, 3, "java/lang/NullPointerException", "MC resource null name.indexOf");
    mc_nbt_string text = mc_world_info_name_text(name);
    size_t slash = 0;
    while (slash < text.length && text.units[slash] != '/')
        ++slash;
    if (slash == text.length)
        return source(e, 10, "java/lang/StringIndexOutOfBoundsException",
                      "MC resource prefix substring");
    if (mc_download_name(text.units, slash, &w->call.prefix, e) != MC_CLIENT_DOWNLOAD_OK)
        return e->status;
    if (mc_download_name(text.units + slash + 1u, text.length - slash - 1u, &w->call.suffix, e) !=
        MC_CLIENT_DOWNLOAD_OK)
        return e->status;
    mc_nbt_string prefix = mc_world_info_name_text(w->call.prefix);
    mc_sound_manager_pool_kind kind;
    int field_pc, invoke_pc;
    if (mc_java_ascii_operand_equal_ignore_case(prefix.units, prefix.length, "sound")) {
        kind = MC_SOUND_MANAGER_EFFECTS;
        field_pc = 34;
        invoke_pc = 39;
    } else if (mc_java_ascii_operand_equal_ignore_case(prefix.units, prefix.length, "newsound")) {
        kind = MC_SOUND_MANAGER_EFFECTS;
        field_pc = 56;
        invoke_pc = 61;
    } else if (mc_java_ascii_operand_equal_ignore_case(prefix.units, prefix.length, "streaming")) {
        kind = MC_SOUND_MANAGER_STREAMING;
        field_pc = 78;
        invoke_pc = 83;
    } else if (mc_java_ascii_operand_equal_ignore_case(prefix.units, prefix.length, "music")) {
        kind = MC_SOUND_MANAGER_MUSIC;
        field_pc = 100;
        invoke_pc = 105;
    } else if (mc_java_ascii_operand_equal_ignore_case(prefix.units, prefix.length, "newmusic")) {
        kind = MC_SOUND_MANAGER_MUSIC;
        field_pc = 122;
        invoke_pc = 127;
    } else
        return MC_CLIENT_DOWNLOAD_OK;
    mc_sound_manager *captured = app->B;
    (void)field_pc;
    if (captured == NULL)
        return source(e, invoke_pc, "java/lang/NullPointerException",
                      "actual null MC.B yo invocation receiver");
    mc_nbt_string suffix = mc_world_info_name_text(w->call.suffix);
    const mc_sound_pool_entry *entry = NULL;
    mc_sound_status s = mc_sound_manager_add_java_file(
        captured, kind, (mc_sound_text){suffix.units, suffix.length, false}, file, &w->call.sound,
        &entry, &e->sound);
    if (s != MC_SOUND_OK) {
        if (s == MC_SOUND_SOURCE_EXCEPTION) {
            source(e, invoke_pc, "java/lang/RuntimeException",
                   "actual hr source registration failure"); /* Refine the real reached category; no
                                                                Throwable identity is fabricated. */
            if (e->sound.pool.exception == MC_SOUND_EXCEPTION_NULL_FILE ||
                e->sound.pool.exception == MC_SOUND_EXCEPTION_NULL_NAME)
                e->exception_class = "java/lang/NullPointerException";
            else if (e->sound.pool.exception == MC_SOUND_EXCEPTION_STRING_INDEX)
                e->exception_class = "java/lang/StringIndexOutOfBoundsException";
        } else
            mc_download_fail(e,
                             s == MC_SOUND_BUSY                ? MC_CLIENT_DOWNLOAD_BUSY
                             : s == MC_SOUND_LIMIT             ? MC_CLIENT_DOWNLOAD_LIMIT
                             : s == MC_SOUND_ALLOCATION_FAILED ? MC_CLIENT_DOWNLOAD_ALLOCATION
                                                               : MC_CLIENT_DOWNLOAD_UNIMPLEMENTED,
                             "net/minecraft/client/Minecraft", "a",
                             "(Ljava/lang/String;Ljava/io/File;)V", invoke_pc,
                             "actual typed yo/hr native child");
        mc_download_caller(e, "yo",
                           kind == MC_SOUND_MANAGER_EFFECTS     ? "a"
                           : kind == MC_SOUND_MANAGER_STREAMING ? "b"
                                                                : "c",
                           "(Ljava/lang/String;Ljava/io/File;)V", 6);
        return e->status;
    }
    ++w->registrations;
    return MC_CLIENT_DOWNLOAD_OK;
}
