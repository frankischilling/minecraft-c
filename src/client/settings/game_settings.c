#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "mc/client/settings/game_settings.h"

#include "mc/core/java/float_format.h"
#include "mc/core/java/float_parse.h"
#include "mc/core/java/integer_parse.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <share.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* Header and payload share an allocation, with no compiler-specific flexible
 * array. Every stored payload needs at most pointer/double alignment. */
typedef union storage_block storage_block;

union storage_block {
    struct {
        storage_block *next;
        size_t bytes;
    } header;

    long double alignment;
    void *pointer_alignment;
};

typedef struct arena {
    storage_block *blocks;
    size_t used, limit;
    mc_game_settings_status failure;
} arena;

struct mc_key_binding {
    struct mc_key_binding *next;
    mc_game_settings_text name;
    int32_t code;
};

typedef struct retained_text {
    struct retained_text *next;
    mc_game_settings_text view;
} retained_text;

struct mc_game_settings {
    arena memory;
    mc_game_settings_state state;
    mc_game_settings_profile profile;
    mc_key_binding *owned_bindings;
    retained_text *texts;
    size_t binding_owners;
    bool active;
};

typedef struct file_input {
    FILE *file;
    size_t read;
    int system_error;
    bool limited, pending_error;
} file_input;

static const uint16_t empty_string[] = {0};
static const uint16_t default_pack[] = {'D', 'e', 'f', 'a', 'u', 'l', 't'};
static const uint16_t binding_name_0[] = {'k', 'e', 'y', '.', 'f', 'o', 'r', 'w', 'a', 'r', 'd'};
static const uint16_t binding_name_1[] = {'k', 'e', 'y', '.', 'l', 'e', 'f', 't'};
static const uint16_t binding_name_2[] = {'k', 'e', 'y', '.', 'b', 'a', 'c', 'k'};
static const uint16_t binding_name_3[] = {'k', 'e', 'y', '.', 'r', 'i', 'g', 'h', 't'};
static const uint16_t binding_name_4[] = {'k', 'e', 'y', '.', 'j', 'u', 'm', 'p'};
static const uint16_t binding_name_5[] = {'k', 'e', 'y', '.', 'i', 'n', 'v',
                                          'e', 'n', 't', 'o', 'r', 'y'};
static const uint16_t binding_name_6[] = {'k', 'e', 'y', '.', 'd', 'r', 'o', 'p'};
static const uint16_t binding_name_7[] = {'k', 'e', 'y', '.', 'c', 'h', 'a', 't'};
static const uint16_t binding_name_8[] = {'k', 'e', 'y', '.', 'f', 'o', 'g'};
static const uint16_t binding_name_9[] = {'k', 'e', 'y', '.', 's', 'n', 'e', 'a', 'k'};
static const mc_game_settings_text default_binding_names[] = {
    {binding_name_0, 11u, false}, {binding_name_1, 8u, false}, {binding_name_2, 8u, false},
    {binding_name_3, 9u, false},  {binding_name_4, 8u, false}, {binding_name_5, 13u, false},
    {binding_name_6, 8u, false},  {binding_name_7, 8u, false}, {binding_name_8, 7u, false},
    {binding_name_9, 9u, false}};

static mc_game_settings_status result(mc_game_settings_error *error, mc_game_settings_status status,
                                      int system_error, const char *operation) {
    if (error != NULL)
        *error = (mc_game_settings_error){status, system_error, operation};
    return status;
}

static void *allocate(void *context, size_t size) {
    arena *memory = context;
    if (size > SIZE_MAX - sizeof(storage_block) || memory->used > memory->limit ||
        sizeof(storage_block) + size > memory->limit - memory->used) {
        memory->failure = MC_GAME_SETTINGS_LIMIT;
        return NULL;
    }
    storage_block *block = malloc(sizeof(*block) + size);
    if (block == NULL) {
        memory->failure = MC_GAME_SETTINGS_ALLOCATION_FAILED;
        return NULL;
    }
    block->header.next = memory->blocks;
    block->header.bytes = sizeof(*block) + size;
    memory->blocks = block;
    memory->used += block->header.bytes;
    return block + 1;
}

static void release_arena(arena *memory) {
    while (memory->blocks != NULL) {
        storage_block *block = memory->blocks;
        memory->blocks = block->header.next;
        free(block);
    }
    memory->used = 0;
}

static bool valid_text(mc_game_settings_text text) {
    return text.is_null ? text.units == NULL && text.length == 0
                        : text.length <= MC_GAME_SETTINGS_MAX_TEXT_UNITS &&
                              (text.length == 0 || text.units != NULL);
}

static mc_game_settings_status copy_text(mc_game_settings *settings, mc_game_settings_text text,
                                         mc_game_settings_text *out, bool preserve_reference) {
    if (!valid_text(text))
        return text.length > MC_GAME_SETTINGS_MAX_TEXT_UNITS ? MC_GAME_SETTINGS_LIMIT
                                                             : MC_GAME_SETTINGS_INVALID_ARGUMENT;
    if (text.is_null) {
        *out = text;
        return MC_GAME_SETTINGS_OK;
    }
    if (text.length == 0) {
        *out = (mc_game_settings_text){empty_string, 0, false};
        return MC_GAME_SETTINGS_OK;
    }
    /* Reassigning an existing String reference must not manufacture a new
     * identity, particularly when a caller changes only scalar fields. */
    if (preserve_reference && text.units == default_pack && text.length == 7u) {
        *out = text;
        return MC_GAME_SETTINGS_OK;
    }
    for (size_t i = 0; preserve_reference && i < MC_GAME_SETTINGS_NAMED_BINDINGS; ++i)
        if (text.units == default_binding_names[i].units &&
            text.length == default_binding_names[i].length) {
            *out = text;
            return MC_GAME_SETTINGS_OK;
        }
    for (const retained_text *item = preserve_reference ? settings->texts : NULL; item != NULL;
         item = item->next)
        if (text.units == item->view.units && text.length == item->view.length) {
            *out = item->view;
            return MC_GAME_SETTINGS_OK;
        }
    retained_text *stored =
        allocate(&settings->memory, sizeof(*stored) + (text.length + 1u) * sizeof(uint16_t));
    if (stored == NULL)
        return settings->memory.failure;
    uint16_t *units = (uint16_t *)(stored + 1);
    if (text.length != 0)
        memcpy(units, text.units, text.length * sizeof(*units));
    units[text.length] = 0;
    stored->next = settings->texts;
    stored->view = (mc_game_settings_text){units, text.length, false};
    settings->texts = stored;
    *out = stored->view;
    return MC_GAME_SETTINGS_OK;
}

static bool equal_ascii(mc_game_settings_text text, const char *ascii) {
    if (text.is_null)
        return false;
    const size_t length = strlen(ascii);
    if (text.length != length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (text.units[i] != (uint8_t)ascii[i])
            return false;
    return true;
}

static bool owns_binding(const mc_game_settings *settings, const mc_key_binding *binding) {
    if (binding == NULL)
        return true;
    for (const mc_key_binding *item = settings->owned_bindings; item != NULL; item = item->next)
        if (item == binding)
            return true;
    return false;
}

static mc_game_settings_status admit(mc_game_settings *settings, mc_game_settings_error *error) {
    if (settings == NULL)
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0, "GameSettings receiver");
    if (settings->active)
        return result(error, MC_GAME_SETTINGS_BUSY, 0, "GameSettings active operation");
    return MC_GAME_SETTINGS_OK;
}

bool mc_game_settings_read_state(const mc_game_settings *settings, mc_game_settings_state *out) {
    if (settings == NULL || out == NULL)
        return false;
    *out = settings->state;
    return true;
}

bool mc_key_binding_read(const mc_key_binding *binding, mc_game_settings_text *name,
                         int32_t *code) {
    if (binding == NULL || name == NULL || code == NULL)
        return false;
    *name = binding->name;
    *code = binding->code;
    return true;
}

mc_game_settings_status mc_game_settings_set_values(mc_game_settings *settings,
                                                    const mc_game_settings_values *values,
                                                    mc_game_settings_error *error) {
    mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    if (values != NULL && (values->texture_pack.length > MC_GAME_SETTINGS_MAX_TEXT_UNITS ||
                           values->last_server.length > MC_GAME_SETTINGS_MAX_TEXT_UNITS))
        return result(error, MC_GAME_SETTINGS_LIMIT, 0, "GameSettings String length");
    if (values == NULL || !valid_text(values->texture_pack) || !valid_text(values->last_server))
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0, "GameSettings values");
    mc_game_settings_values copied = *values;
    status = copy_text(settings, values->texture_pack, &copied.texture_pack, true);
    if (status == MC_GAME_SETTINGS_OK) {
        if (values->last_server.units == values->texture_pack.units &&
            values->last_server.length == values->texture_pack.length &&
            values->last_server.is_null == values->texture_pack.is_null)
            copied.last_server = copied.texture_pack;
        else
            status = copy_text(settings, values->last_server, &copied.last_server, true);
    }
    if (status == MC_GAME_SETTINGS_OK)
        settings->state.values = copied;
    return result(error, status, 0, status == MC_GAME_SETTINGS_OK ? NULL : "GameSettings Strings");
}

mc_game_settings_status mc_game_settings_binding_create(mc_game_settings *settings,
                                                        mc_game_settings_text name, int32_t code,
                                                        mc_key_binding **out,
                                                        mc_game_settings_error *error) {
    mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    if (name.length > MC_GAME_SETTINGS_MAX_TEXT_UNITS)
        return result(error, MC_GAME_SETTINGS_LIMIT, 0, "KeyBinding name length");
    if (out == NULL || !valid_text(name))
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0, "KeyBinding creation");
    if (settings->binding_owners >= MC_GAME_SETTINGS_MAX_BINDING_OWNERS)
        return result(error, MC_GAME_SETTINGS_LIMIT, 0, "KeyBinding owner count");
    mc_game_settings_text copied;
    status = copy_text(settings, name, &copied, true);
    if (status != MC_GAME_SETTINGS_OK)
        return result(error, status, 0, "KeyBinding name");
    mc_key_binding *binding = allocate(&settings->memory, sizeof(*binding));
    if (binding == NULL)
        return result(error, settings->memory.failure, 0, "KeyBinding allocation");
    *binding = (mc_key_binding){settings->owned_bindings, copied, code};
    settings->owned_bindings = binding;
    ++settings->binding_owners;
    *out = binding;
    return result(error, MC_GAME_SETTINGS_OK, 0, NULL);
}

mc_game_settings_status mc_game_settings_binding_set(mc_game_settings *settings,
                                                     mc_key_binding *binding,
                                                     mc_game_settings_text name, int32_t code,
                                                     mc_game_settings_error *error) {
    mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    if (binding == NULL || !owns_binding(settings, binding))
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0, "KeyBinding ownership");
    mc_game_settings_text copied;
    status = copy_text(settings, name, &copied, true);
    if (status == MC_GAME_SETTINGS_OK) {
        binding->name = copied;
        binding->code = code;
    }
    return result(error, status, 0, status == MC_GAME_SETTINGS_OK ? NULL : "KeyBinding name");
}

mc_game_settings_status mc_game_settings_set_named_binding(mc_game_settings *settings, size_t index,
                                                           mc_key_binding *binding,
                                                           mc_game_settings_error *error) {
    mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    if (index >= MC_GAME_SETTINGS_NAMED_BINDINGS || !owns_binding(settings, binding))
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0, "KeyBinding named field");
    settings->state.named[index] = binding;
    return result(error, MC_GAME_SETTINGS_OK, 0, NULL);
}

mc_game_settings_status mc_game_settings_set_binding_array(mc_game_settings *settings,
                                                           mc_key_binding *const *bindings,
                                                           size_t count, bool is_null,
                                                           mc_game_settings_error *error) {
    mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    if ((is_null && (bindings != NULL || count != 0)) ||
        (!is_null && count != 0 && bindings == NULL))
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0, "KeyBinding array");
    if (count > MC_GAME_SETTINGS_MAX_BINDINGS)
        return result(error, MC_GAME_SETTINGS_LIMIT, 0, "KeyBinding array length");
    for (size_t i = 0; i < count; ++i)
        if (!owns_binding(settings, bindings[i]))
            return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0,
                          "KeyBinding array ownership");
    mc_key_binding **copied = NULL;
    if (!is_null) {
        copied = allocate(&settings->memory, (count == 0 ? 1u : count) * sizeof(*copied));
        if (copied == NULL)
            return result(error, settings->memory.failure, 0, "KeyBinding array allocation");
        if (count != 0)
            memcpy(copied, bindings, count * sizeof(*copied));
    }
    settings->state.bindings = copied;
    settings->state.binding_count = count;
    settings->state.bindings_null = is_null;
    return result(error, MC_GAME_SETTINGS_OK, 0, NULL);
}

mc_game_settings_status mc_game_settings_retire(mc_game_settings **settings,
                                                mc_game_settings_error *error) {
    if (settings == NULL)
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0, "GameSettings retirement");
    if (*settings != NULL) {
        if ((*settings)->active)
            return result(error, MC_GAME_SETTINGS_BUSY, 0, "GameSettings active retirement");
        release_arena(&(*settings)->memory);
        free(*settings);
        *settings = NULL;
    }
    return result(error, MC_GAME_SETTINGS_OK, 0, NULL);
}

mc_game_settings_status mc_game_settings_create_defaults(const mc_game_settings_profile *profile,
                                                         mc_game_settings **out,
                                                         mc_game_settings_error *error) {
    mc_game_settings_profile selected = {MC_JAVA_READER_DEFAULT,
#ifdef _WIN32
                                         MC_JAVA_WRITER_CRLF
#else
                                         MC_JAVA_WRITER_LF
#endif
    };
    if (profile != NULL)
        selected = *profile;
    if (out == NULL || (unsigned int)selected.charset > (unsigned int)MC_JAVA_READER_US_ASCII ||
        (selected.lines != MC_JAVA_WRITER_LF && selected.lines != MC_JAVA_WRITER_CRLF))
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0,
                      "GameSettings constructor profile");
    mc_game_settings *settings = calloc(1, sizeof(*settings));
    if (settings == NULL)
        return result(error, MC_GAME_SETTINGS_ALLOCATION_FAILED, 0, "GameSettings constructor");
    settings->memory.limit = MC_GAME_SETTINGS_OWNER_BYTES;
    settings->profile = selected;
    settings->state.values = (mc_game_settings_values){.music_volume = 1.0f,
                                                       .sound_volume = 1.0f,
                                                       .mouse_sensitivity = 0.5f,
                                                       .view_bobbing = true,
                                                       .performance_mode = 1,
                                                       .fancy_graphics = true,
                                                       .ambient_occlusion = true,
                                                       .difficulty = 2,
                                                       .source_G = 1.0f,
                                                       .source_H = 1.0f};
    settings->state.values.texture_pack = (mc_game_settings_text){default_pack, 7u, false};
    settings->state.values.last_server = (mc_game_settings_text){empty_string, 0, false};
    static const int32_t codes[] = {17, 30, 31, 32, 57, 18, 16, 20, 33, 42};
    mc_game_settings_status status = MC_GAME_SETTINGS_OK;
    for (size_t i = 0; status == MC_GAME_SETTINGS_OK && i < MC_GAME_SETTINGS_NAMED_BINDINGS; ++i) {
        mc_key_binding *binding = allocate(&settings->memory, sizeof(*binding));
        if (binding == NULL) {
            status = settings->memory.failure;
            break;
        }
        *binding = (mc_key_binding){settings->owned_bindings, default_binding_names[i], codes[i]};
        settings->owned_bindings = binding;
        ++settings->binding_owners;
        settings->state.named[i] = binding;
    }
    /* w differs from the m..v field initialization order. */
    mc_key_binding *bindings[MC_GAME_SETTINGS_NAMED_BINDINGS] = {
        settings->state.named[0], settings->state.named[1], settings->state.named[2],
        settings->state.named[3], settings->state.named[4], settings->state.named[9],
        settings->state.named[6], settings->state.named[5], settings->state.named[7],
        settings->state.named[8]};
    if (status == MC_GAME_SETTINGS_OK)
        status = mc_game_settings_set_binding_array(settings, bindings,
                                                    MC_GAME_SETTINGS_NAMED_BINDINGS, false, error);
    if (status != MC_GAME_SETTINGS_OK) {
        mc_game_settings_retire(&settings, NULL);
        return result(error, status, 0, "GameSettings default owners");
    }
    *out = settings;
    return result(error, MC_GAME_SETTINGS_OK, 0, NULL);
}

static mc_game_settings_status emit(const mc_game_settings_diagnostics *diagnostics,
                                    mc_game_settings_diagnostic event,
                                    mc_game_settings_error *error) {
    if (diagnostics == NULL || diagnostics->emit == NULL)
        return MC_GAME_SETTINGS_OK;
    mc_game_settings_error cause = {0};
    if (diagnostics->emit(diagnostics->context, &event, &cause) != MC_GAME_SETTINGS_OK)
        return result(error, MC_GAME_SETTINGS_DIAGNOSTIC_FAILED, cause.system_error,
                      cause.operation != NULL ? cause.operation
                                              : "GameSettings diagnostic delivery");
    return MC_GAME_SETTINGS_OK;
}

static mc_game_settings_status caught(const mc_game_settings_diagnostics *diagnostics,
                                      mc_game_settings_report *report, bool saving,
                                      mc_game_settings_source_exception exception, int system_error,
                                      mc_game_settings_error *error) {
    report->outer_exception_caught = true;
    report->outer_exception = exception;
    report->source_system_error = system_error;
    const mc_game_settings_diagnostic event = {
        .kind = saving ? MC_GAME_SETTINGS_SAVE_FAILED : MC_GAME_SETTINGS_LOAD_FAILED,
        .message = saving ? "Failed to save options" : "Failed to load options",
        .exception = exception,
        .line = {.is_null = true},
        .system_error = system_error};
    return emit(diagnostics, event, error);
}

/* Native filename conversion is not the FileReader content charset. */
#ifdef _WIN32
static mc_game_settings_status native_path(const char *path, wchar_t **out,
                                           mc_game_settings_error *error) {
    const size_t length = strlen(path);
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, NULL, 0);
    if (count == 0)
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0, "GameSettings UTF8 filename");
    wchar_t *native = malloc(((size_t)count + 1u) * sizeof(*native));
    if (native == NULL)
        return result(error, MC_GAME_SETTINGS_ALLOCATION_FAILED, 0, "GameSettings native filename");
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, native, count) !=
        count) {
        free(native);
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0, "GameSettings UTF8 conversion");
    }
    native[count] = 0;
    *out = native;
    return MC_GAME_SETTINGS_OK;
}
#endif

static mc_game_settings_status open_reader_file(const char *path, FILE **out, bool *absent,
                                                int *system_error, mc_game_settings_report *report,
                                                mc_game_settings_error *error) {
    FILE *file;
#ifdef _WIN32
    wchar_t *native = NULL;
    mc_game_settings_status status = native_path(path, &native, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    /* File.exists suppresses ordinary access/existence errors. */
    if (GetFileAttributesW(native) == INVALID_FILE_ATTRIBUTES) {
        free(native);
        *absent = true;
        return MC_GAME_SETTINGS_OK;
    }
    errno = 0;
    file = _wfsopen(native, L"rb", _SH_DENYNO);
    *system_error = errno;
    free(native);
#else
    struct stat information;
    if (stat(path, &information) != 0) {
        *absent = true;
        return MC_GAME_SETTINGS_OK;
    }
    errno = 0;
    file = fopen(path, "rb");
    *system_error = errno;
    if (file != NULL) {
        /* FileInputStream rejects directories during its native open. Inspect
         * the reached descriptor, not only the earlier File.exists pathname. */
        errno = 0;
        const int inspected = fstat(fileno(file), &information);
        const int inspection_error = errno;
        if (inspected != 0 || S_ISDIR(information.st_mode)) {
            *system_error = inspected != 0 ? inspection_error : EISDIR;
            report->native_input_admission_close = true;
            errno = 0;
            if (fclose(file) != 0) {
                report->native_cleanup_failed = true;
                report->native_cleanup_system_error = errno;
            }
            file = NULL;
        }
    }
#endif
    if (file != NULL && setvbuf(file, NULL, _IONBF, 0) != 0) {
        const int admission_error = errno;
        report->native_input_admission_close = true;
        errno = 0;
        if (fclose(file) != 0) {
            report->native_cleanup_failed = true;
            report->native_cleanup_system_error = errno;
        }
        return result(error, MC_GAME_SETTINGS_NATIVE_IO_ERROR, admission_error,
                      "GameSettings unbuffered input admission");
    }
    *out = file;
    return MC_GAME_SETTINGS_OK;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *bytes, size_t capacity,
                                            size_t *count) {
    file_input *input = context;
    *count = 0;
    if (input->pending_error)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    if (input->read >= MC_GAME_SETTINGS_MAX_INPUT_BYTES) {
        errno = 0;
        const int extra = fgetc(input->file);
        if (extra == EOF && !ferror(input->file))
            return MC_JAVA_PROPERTIES_OK;
        input->limited = extra != EOF;
        if (extra != EOF)
            ++input->read;
        input->system_error = errno;
        return MC_JAVA_PROPERTIES_IO_ERROR;
    }
    const size_t room = MC_GAME_SETTINGS_MAX_INPUT_BYTES - input->read;
    if (capacity > room)
        capacity = room;
    errno = 0;
    const size_t read = fread(bytes, 1, capacity, input->file);
    input->read += read;
    if (ferror(input->file)) {
        input->system_error = errno;
        input->pending_error = true;
        if (read == 0)
            return MC_JAVA_PROPERTIES_IO_ERROR;
    }
    *count = read;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_java_reader_status close_input(void *context) {
    file_input *input = context;
    if (input->file == NULL)
        return MC_JAVA_READER_OK;
    FILE *file = input->file;
    input->file = NULL;
    errno = 0;
    if (fclose(file) != 0) {
        input->system_error = errno;
        return MC_JAVA_READER_IO_ERROR;
    }
    return MC_JAVA_READER_OK;
}

static bool input_ready(void *context) {
    const file_input *input = context;
    if (input->file == NULL || input->pending_error)
        return false;
#ifdef _WIN32
    const int descriptor = _fileno(input->file);
    const intptr_t handle = _get_osfhandle(descriptor);
    LARGE_INTEGER length;
    const __int64 position = _telli64(descriptor);
    return handle != -1 && position >= 0 && GetFileSizeEx((HANDLE)handle, &length) != 0 &&
           length.QuadPart > position;
#else
    const int descriptor = fileno(input->file);
    struct stat information;
    if (fstat(descriptor, &information) != 0 || !S_ISREG(information.st_mode))
        return false;
    const off_t position = lseek(descriptor, 0, SEEK_CUR);
    return position >= 0 && information.st_size > position;
#endif
}

static bool binding_key(mc_game_settings_text key, mc_game_settings_text name) {
    static const uint16_t null_name[] = {'n', 'u', 'l', 'l'};
    if (name.is_null)
        name = (mc_game_settings_text){null_name, 4u, false};
    if (key.length != 4u + name.length || key.units[0] != 'k' || key.units[1] != 'e' ||
        key.units[2] != 'y' || key.units[3] != '_')
        return false;
    return name.length == 0 ||
           memcmp(key.units + 4u, name.units, name.length * sizeof(uint16_t)) == 0;
}

static mc_game_settings_status parse_integer(mc_game_settings_text text, int32_t *out,
                                             mc_game_settings_source_exception *exception) {
    const mc_java_integer_parse_text input = {text.units, text.length};
    const mc_java_integer_parse_status status = mc_java_integer_parse(&input, out);
    if (status == MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT)
        *exception = MC_GAME_SETTINGS_SOURCE_NUMBER_FORMAT;
    else if (status != MC_JAVA_INTEGER_PARSE_OK)
        return status == MC_JAVA_INTEGER_PARSE_LIMIT ? MC_GAME_SETTINGS_LIMIT
                                                     : MC_GAME_SETTINGS_INVALID_ARGUMENT;
    return MC_GAME_SETTINGS_OK;
}

static mc_game_settings_status parse_float(mc_game_settings_text text, float *out,
                                           mc_game_settings_source_exception *exception) {
    if (equal_ascii(text, "true")) {
        *out = 1.0f;
        return MC_GAME_SETTINGS_OK;
    }
    if (equal_ascii(text, "false")) {
        *out = 0.0f;
        return MC_GAME_SETTINGS_OK;
    }
    const mc_java_float_parse_text input = {text.units, text.length};
    const mc_java_float_parse_status status = mc_java_float_parse(&input, out);
    if (status == MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT || status == MC_JAVA_FLOAT_PARSE_NULL_TEXT)
        *exception = MC_GAME_SETTINGS_SOURCE_NUMBER_FORMAT;
    else if (status != MC_JAVA_FLOAT_PARSE_OK)
        return status == MC_JAVA_FLOAT_PARSE_LIMIT ? MC_GAME_SETTINGS_LIMIT
                                                   : MC_GAME_SETTINGS_INVALID_ARGUMENT;
    return MC_GAME_SETTINGS_OK;
}

/* A single-colon Java split with limit=0, retaining only the first two
 * fields actually read here. This is not String.trim or Properties parsing. */
static void split_line(mc_game_settings_text line, mc_game_settings_text *key,
                       mc_game_settings_text *value, bool *has_key, bool *has_value) {
    size_t first = 0;
    while (first < line.length && line.units[first] != ':')
        ++first;
    *has_key = true;
    *has_value = false;
    *key = (mc_game_settings_text){line.units, first, false};
    if (first == line.length)
        return; /* The original String, including empty String, is one field. */
    size_t end = line.length;
    while (end != 0 && line.units[end - 1u] == ':')
        --end;
    if (end == 0) {
        *has_key = false;
        return;
    }
    if (end <= first)
        return;
    size_t second = first + 1u;
    while (second < line.length && line.units[second] != ':')
        ++second;
    *value = (mc_game_settings_text){line.units + first + 1u, second - first - 1u, false};
    *has_value = true;
}

static mc_game_settings_status apply_line(mc_game_settings *settings, mc_game_settings_text line,
                                          mc_game_settings_source_exception *exception) {
    mc_game_settings_text key = {0}, value = {0};
    bool has_key, has_value;
    split_line(line, &key, &value, &has_key, &has_value);
    if (!has_key) {
        *exception = MC_GAME_SETTINGS_SOURCE_MISSING_SPLIT_VALUE;
        return MC_GAME_SETTINGS_OK;
    }
    mc_game_settings_values *values = &settings->state.values;
    mc_game_settings_status status = MC_GAME_SETTINGS_OK;
    /* kv.a's independent if statements precede its live w loop. A null w or
     * entry can throw after a scalar assignment has already occurred. */
#define VALUE_REQUIRED()                                                                           \
    do {                                                                                           \
        if (!has_value) {                                                                          \
            *exception = MC_GAME_SETTINGS_SOURCE_MISSING_SPLIT_VALUE;                              \
            return MC_GAME_SETTINGS_OK;                                                            \
        }                                                                                          \
    } while (0)
#define FLOAT_KEY(key_name, field)                                                                 \
    if (equal_ascii(key, key_name)) {                                                              \
        VALUE_REQUIRED();                                                                          \
        status = parse_float(value, &values->field, exception);                                    \
    }
#define INT_KEY(key_name, field)                                                                   \
    if (equal_ascii(key, key_name)) {                                                              \
        VALUE_REQUIRED();                                                                          \
        status = parse_integer(value, &values->field, exception);                                  \
    }
#define BOOL_KEY(key_name, field)                                                                  \
    if (equal_ascii(key, key_name)) {                                                              \
        VALUE_REQUIRED();                                                                          \
        values->field = equal_ascii(value, "true");                                                \
    }
    FLOAT_KEY("music", music_volume)
    FLOAT_KEY("sound", sound_volume)
    FLOAT_KEY("mouseSensitivity", mouse_sensitivity)
    BOOL_KEY("invertYMouse", invert_mouse)
    INT_KEY("viewDistance", render_distance)
    INT_KEY("guiScale", gui_scale)
    BOOL_KEY("bobView", view_bobbing)
    BOOL_KEY("anaglyph3d", anaglyph)
    BOOL_KEY("advancedOpengl", advanced_opengl)
    INT_KEY("fpsLimit", performance_mode)
    INT_KEY("difficulty", difficulty)
    BOOL_KEY("fancyGraphics", fancy_graphics)
    BOOL_KEY("ao", ambient_occlusion)
    if (status != MC_GAME_SETTINGS_OK || *exception != MC_GAME_SETTINGS_SOURCE_NONE)
        return status;
    if (equal_ascii(key, "skin")) {
        VALUE_REQUIRED();
        status = copy_text(settings, value, &values->texture_pack, false);
    }
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    if (equal_ascii(key, "lastServer") && has_value)
        status = copy_text(settings, value, &values->last_server, false);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    if (settings->state.bindings_null) {
        *exception = MC_GAME_SETTINGS_SOURCE_NULL_BINDING_ARRAY;
        return MC_GAME_SETTINGS_OK;
    }
    for (size_t i = 0; i < settings->state.binding_count; ++i) {
        mc_key_binding *binding = settings->state.bindings[i];
        if (binding == NULL) {
            *exception = MC_GAME_SETTINGS_SOURCE_NULL_BINDING;
            return MC_GAME_SETTINGS_OK;
        }
        if (binding_key(key, binding->name)) {
            VALUE_REQUIRED();
            status = parse_integer(value, &binding->code, exception);
            if (status != MC_GAME_SETTINGS_OK || *exception != MC_GAME_SETTINGS_SOURCE_NONE)
                return status;
        }
    }
#undef BOOL_KEY
#undef INT_KEY
#undef FLOAT_KEY
#undef VALUE_REQUIRED
    return MC_GAME_SETTINGS_OK;
}

mc_game_settings_status mc_game_settings_load(mc_game_settings *settings,
                                              const mc_game_settings_diagnostics *diagnostics,
                                              mc_game_settings_report *out,
                                              mc_game_settings_error *error) {
    mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    settings->active = true;
    mc_game_settings_report report = {0};
    arena temporary = {.limit = MC_GAME_SETTINGS_READER_BYTES};
    file_input input = {0};
    if (settings->state.options_path == NULL) {
        status = caught(diagnostics, &report, false, MC_GAME_SETTINGS_SOURCE_NULL_FILE, 0, error);
        goto cleanup;
    }
    status = open_reader_file(settings->state.options_path, &input.file, &report.file_absent,
                              &input.system_error, &report, error);
    if (status != MC_GAME_SETTINGS_OK || report.file_absent)
        goto cleanup;
    if (input.file == NULL) {
        status = caught(diagnostics, &report, false, MC_GAME_SETTINGS_SOURCE_FILE_IO,
                        input.system_error, error);
        if (report.native_cleanup_failed && status == MC_GAME_SETTINGS_OK)
            status =
                result(error, MC_GAME_SETTINGS_NATIVE_IO_ERROR, report.native_cleanup_system_error,
                       "GameSettings rejected input admission cleanup");
        goto cleanup;
    }
    report.source_opened = true;
    const mc_java_properties_allocator allocator = {&temporary, allocate};
    const mc_java_reader_input stream = {{&input, read_bytes}, input_ready, close_input};
    mc_java_reader *reader = NULL;
    mc_java_reader_status read_status =
        mc_java_reader_create(&allocator, &stream, settings->profile.charset, &reader);
    if (read_status != MC_JAVA_READER_OK) {
        status = result(error,
                        temporary.failure != MC_GAME_SETTINGS_OK ? temporary.failure
                        : read_status == MC_JAVA_READER_UNSUPPORTED_CHARSET
                            ? MC_GAME_SETTINGS_UNSUPPORTED_CHARSET
                        : read_status == MC_JAVA_READER_ALLOCATION_FAILED
                            ? MC_GAME_SETTINGS_ALLOCATION_FAILED
                            : MC_GAME_SETTINGS_INVALID_ARGUMENT,
                        0, "GameSettings FileReader admission");
        goto cleanup;
    }
    for (;;) {
        const mc_java_reader_line *line = NULL;
        read_status = mc_java_reader_read_line(reader, &line);
        if (read_status != MC_JAVA_READER_OK) {
            if (input.limited || temporary.failure != MC_GAME_SETTINGS_OK)
                status = result(error, input.limited ? MC_GAME_SETTINGS_LIMIT : temporary.failure,
                                0, "GameSettings input budget");
            else if (read_status == MC_JAVA_READER_IO_ERROR)
                status = caught(diagnostics, &report, false, MC_GAME_SETTINGS_SOURCE_FILE_IO,
                                input.system_error, error);
            else
                status =
                    result(error,
                           read_status == MC_JAVA_READER_LIMIT ? MC_GAME_SETTINGS_LIMIT
                                                               : MC_GAME_SETTINGS_INVALID_ARGUMENT,
                           0, "GameSettings readLine");
            break;
        }
        if (line == NULL) {
            report.source_close_reached = true;
            if (mc_java_reader_close(reader) != MC_JAVA_READER_OK)
                status = caught(diagnostics, &report, false, MC_GAME_SETTINGS_SOURCE_FILE_IO,
                                input.system_error, error);
            break;
        }
        ++report.lines_read;
        if (line->length > MC_GAME_SETTINGS_MAX_TEXT_UNITS) {
            status = result(error, MC_GAME_SETTINGS_LIMIT, 0, "GameSettings line units");
            break;
        }
        const mc_game_settings_text text = {line->units, line->length, false};
        mc_game_settings_source_exception exception = MC_GAME_SETTINGS_SOURCE_NONE;
        status = apply_line(settings, text, &exception);
        if (status != MC_GAME_SETTINGS_OK) {
            result(error, status, 0, "GameSettings option native resource");
            break;
        }
        if (exception != MC_GAME_SETTINGS_SOURCE_NONE) {
            ++report.bad_options;
            status = emit(diagnostics,
                          (mc_game_settings_diagnostic){MC_GAME_SETTINGS_BAD_OPTION,
                                                        "Skipping bad option: ", exception,
                                                        report.lines_read, text, 0},
                          error);
            if (status != MC_GAME_SETTINGS_OK)
                break;
        }
    }
cleanup:
    report.bytes_read = input.read;
    if (input.file != NULL) {
        report.native_additional_close = true;
        if (close_input(&input) != MC_JAVA_READER_OK) {
            report.native_cleanup_failed = true;
            report.native_cleanup_system_error = input.system_error;
            if (status == MC_GAME_SETTINGS_OK)
                status = result(error, MC_GAME_SETTINGS_NATIVE_IO_ERROR, input.system_error,
                                "GameSettings native input retirement");
        }
    }
    release_arena(&temporary);
    settings->active = false;
    if (out != NULL)
        *out = report;
    if (status == MC_GAME_SETTINGS_OK)
        result(error, status, 0, NULL);
    return status;
}

/* Integer has no separate public string formatter. This bounded unsigned
 * magnitude conversion handles INT32_MIN without signed negation overflow. */
static size_t decimal(int32_t value, uint16_t *out) {
    char reversed[10];
    uint32_t magnitude = value < 0 ? 0u - (uint32_t)value : (uint32_t)value;
    size_t count = 0, written = 0;
    do {
        reversed[count++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0);
    if (value < 0)
        out[written++] = '-';
    while (count != 0)
        out[written++] = (uint8_t)reversed[--count];
    return written;
}

typedef struct line_builder {
    uint16_t *units;
    size_t length, capacity;
} line_builder;

static bool append_text(line_builder *line, mc_game_settings_text text) {
    static const uint16_t null_string[] = {'n', 'u', 'l', 'l'};
    if (text.is_null)
        text = (mc_game_settings_text){null_string, 4u, false};
    if (text.length > line->capacity - line->length)
        return false;
    if (text.length != 0)
        memcpy(line->units + line->length, text.units, text.length * sizeof(uint16_t));
    line->length += text.length;
    return true;
}

static bool append_ascii(line_builder *line, const char *ascii) {
    const size_t count = strlen(ascii);
    if (count > line->capacity - line->length)
        return false;
    for (size_t i = 0; i < count; ++i)
        line->units[line->length++] = (uint8_t)ascii[i];
    return true;
}

static mc_game_settings_status writer_native(mc_java_writer_status status,
                                             mc_java_writer_error cause,
                                             mc_game_settings_error *error) {
    const mc_game_settings_status mapped =
        status == MC_JAVA_WRITER_OK                    ? MC_GAME_SETTINGS_OK
        : status == MC_JAVA_WRITER_ALLOCATION_FAILED   ? MC_GAME_SETTINGS_ALLOCATION_FAILED
        : status == MC_JAVA_WRITER_LIMIT               ? MC_GAME_SETTINGS_LIMIT
        : status == MC_JAVA_WRITER_UNSUPPORTED_CHARSET ? MC_GAME_SETTINGS_UNSUPPORTED_CHARSET
        : status == MC_JAVA_WRITER_IO_ERROR || status == MC_JAVA_WRITER_INTERRUPTED_IO
            ? MC_GAME_SETTINGS_NATIVE_IO_ERROR
            : MC_GAME_SETTINGS_INVALID_ARGUMENT;
    return result(error, mapped, cause.system_error, cause.operation);
}

/* Only an already admitted public save/action calls this body. Its owner stays
 * active through writer retirement, diagnostic callbacks and report delivery;
 * the outer entry alone clears active. No nested public admission is invented. */
static mc_game_settings_status save_admitted(mc_game_settings *settings,
                                             const mc_game_settings_diagnostics *diagnostics,
                                             mc_game_settings_report *out,
                                             mc_game_settings_error *error) {
    mc_game_settings_status status = MC_GAME_SETTINGS_OK;
    mc_game_settings_report report = {0};
    mc_java_writer *writer = NULL;
    uint16_t *scratch = NULL;
    if (settings->state.options_path == NULL) {
        status = caught(diagnostics, &report, true, MC_GAME_SETTINGS_SOURCE_NULL_FILE, 0, error);
        goto cleanup;
    }
    const mc_java_writer_profile profile = {settings->profile.charset, MC_JAVA_WRITER_PRINT,
                                            settings->profile.lines};
    mc_java_writer_error cause = {0};
    mc_java_writer_status writer_status =
        mc_java_file_writer_open(settings->state.options_path, &profile, &writer, &cause);
    if (writer_status != MC_JAVA_WRITER_OK) {
        report.native_cleanup_failed = cause.native_cleanup_failed;
        report.native_cleanup_system_error = cause.native_cleanup_system_error;
        if (writer_status == MC_JAVA_WRITER_IO_ERROR ||
            writer_status == MC_JAVA_WRITER_INTERRUPTED_IO)
            status = caught(diagnostics, &report, true, MC_GAME_SETTINGS_SOURCE_FILE_IO,
                            cause.system_error, error);
        else
            status = writer_native(writer_status, cause, error);
        if (cause.native_cleanup_failed && status == MC_GAME_SETTINGS_OK)
            status =
                result(error, MC_GAME_SETTINGS_NATIVE_IO_ERROR, cause.native_cleanup_system_error,
                       "GameSettings failed FileWriter admission cleanup");
        goto cleanup;
    }
    report.source_opened = true;
    /* FileWriter already created/truncated the actual file before any line
     * workspace/formatting. Each bounded line has room for a maximal name. */
    const size_t capacity = MC_GAME_SETTINGS_MAX_TEXT_UNITS + 64u;
    scratch = malloc(capacity * sizeof(*scratch));
    if (scratch == NULL) {
        status = result(error, MC_GAME_SETTINGS_ALLOCATION_FAILED, 0, "GameSettings save line");
        goto cleanup;
    }
    static const char *const keys[] = {"music:",
                                       "sound:",
                                       "invertYMouse:",
                                       "mouseSensitivity:",
                                       "viewDistance:",
                                       "guiScale:",
                                       "bobView:",
                                       "anaglyph3d:",
                                       "advancedOpengl:",
                                       "fpsLimit:",
                                       "difficulty:",
                                       "fancyGraphics:",
                                       "ao:",
                                       "skin:",
                                       "lastServer:"};
    const mc_game_settings_values *values = &settings->state.values;
    size_t index = 0;
    for (;;) {
        line_builder line = {scratch, 0, capacity};
        bool appended = true;
        if (index < 15u) {
            appended = append_ascii(&line, keys[index]);
            if (index == 0u || index == 1u || index == 3u) {
                const float value = index == 0u   ? values->music_volume
                                    : index == 1u ? values->sound_volume
                                                  : values->mouse_sensitivity;
                mc_java_float_string formatted;
                if (mc_java_float_to_string(value, &formatted) != MC_JAVA_FLOAT_FORMAT_OK) {
                    status =
                        result(error, MC_GAME_SETTINGS_LIMIT, 0, "GameSettings Float.toString");
                    break;
                }
                appended = appended && append_ascii(&line, formatted.text);
            } else if (index == 4u || index == 5u || index == 9u || index == 10u) {
                const int32_t value = index == 4u   ? values->render_distance
                                      : index == 5u ? values->gui_scale
                                      : index == 9u ? values->performance_mode
                                                    : values->difficulty;
                line.length += decimal(value, line.units + line.length);
            } else if (index == 13u || index == 14u) {
                appended = appended && append_text(&line, index == 13u ? values->texture_pack
                                                                       : values->last_server);
            } else {
                const bool value = index == 2u    ? values->invert_mouse
                                   : index == 6u  ? values->view_bobbing
                                   : index == 7u  ? values->anaglyph
                                   : index == 8u  ? values->advanced_opengl
                                   : index == 11u ? values->fancy_graphics
                                                  : values->ambient_occlusion;
                appended = appended && append_ascii(&line, value ? "true" : "false");
            }
        } else {
            if (settings->state.bindings_null) {
                status = caught(diagnostics, &report, true,
                                MC_GAME_SETTINGS_SOURCE_NULL_BINDING_ARRAY, 0, error);
                break;
            }
            const size_t binding_index = index - 15u;
            if (binding_index == settings->state.binding_count) {
                report.source_close_reached = true;
                writer_status = mc_java_writer_close(writer, &cause);
                if (writer_status != MC_JAVA_WRITER_OK)
                    status = writer_native(writer_status, cause, error);
                break;
            }
            const mc_key_binding *binding = settings->state.bindings[binding_index];
            if (binding == NULL) {
                status = caught(diagnostics, &report, true, MC_GAME_SETTINGS_SOURCE_NULL_BINDING, 0,
                                error);
                break;
            }
            appended = append_ascii(&line, "key_") && append_text(&line, binding->name) &&
                       append_ascii(&line, ":");
            if (appended)
                line.length += decimal(binding->code, line.units + line.length);
        }
        if (!appended) {
            status = result(error, MC_GAME_SETTINGS_LIMIT, 0, "GameSettings output line units");
            break;
        }
        const mc_java_properties_string_view text = {line.units, line.length};
        ++report.println_attempts;
        writer_status = mc_java_writer_println(writer, &text, &cause);
        if (writer_status != MC_JAVA_WRITER_OK) {
            status = writer_native(writer_status, cause, error);
            break;
        }
        ++index;
    }
cleanup:
    if (writer != NULL) {
        report.writer_state_present = mc_java_writer_read_state(writer, &report.writer_state);
        mc_java_writer_retirement retired = {0};
        mc_java_writer_error retirement_error = {0};
        const mc_java_writer_status retired_status =
            mc_java_writer_retire(&writer, &retired, &retirement_error);
        report.native_additional_close = retired.additional_sink_close;
        if (retired_status != MC_JAVA_WRITER_OK) {
            report.native_cleanup_failed = true;
            report.native_cleanup_system_error = retirement_error.system_error;
            if (status == MC_GAME_SETTINGS_OK)
                status = writer_native(retired_status, retirement_error, error);
        }
    }
    free(scratch);
    if (out != NULL)
        *out = report;
    if (status == MC_GAME_SETTINGS_OK)
        result(error, status, 0, NULL);
    return status;
}

mc_game_settings_status mc_game_settings_save(mc_game_settings *settings,
                                              const mc_game_settings_diagnostics *diagnostics,
                                              mc_game_settings_report *out,
                                              mc_game_settings_error *error) {
    mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    settings->active = true;
    status = save_admitted(settings, diagnostics, out, error);
    settings->active = false;
    return status;
}

mc_game_settings_status
mc_game_settings_set_binding_code(mc_game_settings *settings, int32_t index, int32_t code,
                                  const mc_game_settings_diagnostics *diagnostics,
                                  mc_game_settings_binding_action_report *out,
                                  mc_game_settings_error *error) {
    mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    settings->active = true;
    mc_game_settings_binding_action_report report = {.source_pc = 5};
    if (settings->state.bindings_null) {
        report.uncaught_exception = MC_GAME_SETTINGS_BINDING_ACTION_NULL_ARRAY;
        status = result(error, MC_GAME_SETTINGS_SOURCE_EXCEPTION, 0,
                        "GameSettings binding action null array");
        goto done;
    }
    if (index < 0 || (size_t)index >= settings->state.binding_count) {
        report.uncaught_exception = MC_GAME_SETTINGS_BINDING_ACTION_INDEX;
        status = result(error, MC_GAME_SETTINGS_SOURCE_EXCEPTION, 0,
                        "GameSettings binding action index");
        goto done;
    }
    mc_key_binding *binding = settings->state.bindings[(size_t)index];
    report.source_pc = 7;
    if (binding == NULL) {
        report.uncaught_exception = MC_GAME_SETTINGS_BINDING_ACTION_NULL_BINDING;
        status = result(error, MC_GAME_SETTINGS_SOURCE_EXCEPTION, 0,
                        "GameSettings binding action null binding");
        goto done;
    }
    if (!owns_binding(settings, binding)) {
        status = result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0,
                        "GameSettings binding action ownership");
        goto done;
    }
    binding->code = code;
    report.binding_assigned = true;
    report.source_pc = 11;
    report.save_reached = true;
    status = save_admitted(settings, diagnostics, &report.save, error);
    if (status == MC_GAME_SETTINGS_OK)
        report.source_pc = 14;
done:
    settings->active = false;
    if (out != NULL)
        *out = report;
    return status;
}

mc_game_settings_status mc_game_settings_create_file(
    void *minecraft, const char *directory, const mc_game_settings_profile *profile,
    const mc_game_settings_diagnostics *diagnostics, mc_game_settings **out,
    mc_game_settings_report *report, mc_game_settings_error *error) {
    if (out == NULL)
        return result(error, MC_GAME_SETTINGS_INVALID_ARGUMENT, 0,
                      "GameSettings file constructor output");
    size_t length = 0;
    if (directory != NULL)
        while (length <= MC_GAME_SETTINGS_MAX_PATH_BYTES && directory[length] != 0)
            ++length;
    if (length > MC_GAME_SETTINGS_MAX_PATH_BYTES - sizeof("/options.txt"))
        return result(error, MC_GAME_SETTINGS_LIMIT, 0, "GameSettings directory length");
    mc_game_settings *settings = NULL;
    mc_game_settings_status status = mc_game_settings_create_defaults(profile, &settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    /* File(File(""),child) resolves against the filesystem default parent,
     * unlike the null-parent constructor. No mkdir or canonicalization. */
    if (directory != NULL && length == 0) {
#ifdef _WIN32
        directory = "\\";
#else
        directory = "/";
#endif
        length = 1;
    }
    bool separator = length != 0 && directory[length - 1u] != '/';
#ifdef _WIN32
    separator = separator && directory[length - 1u] != '\\';
    if (length == 2u && directory[1] == ':' &&
        ((directory[0] >= 'A' && directory[0] <= 'Z') ||
         (directory[0] >= 'a' && directory[0] <= 'z')))
        separator = false; /* Drive-relative File("C:"), not C:\\ root. */
#endif
    const size_t path_size = length + (separator ? 1u : 0u) + sizeof("options.txt");
    char *path = allocate(&settings->memory, path_size);
    if (path == NULL) {
        status = settings->memory.failure;
        mc_game_settings_retire(&settings, NULL);
        return result(error, status, 0, "GameSettings File owner");
    }
    if (length != 0)
        memcpy(path, directory, length);
    size_t offset = length;
    if (separator)
        path[offset++] = '/';
    memcpy(path + offset, "options.txt", sizeof("options.txt"));
    settings->state.minecraft = minecraft;
    settings->state.options_path = path;
    settings->state.file_identity = path;
    status = mc_game_settings_load(settings, diagnostics, report, error);
    if (status != MC_GAME_SETTINGS_OK) {
        mc_game_settings_retire(&settings, NULL);
        return status;
    }
    *out = settings;
    return result(error, MC_GAME_SETTINGS_OK, 0, NULL);
}
