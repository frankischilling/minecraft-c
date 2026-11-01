#include "bitmap_font_internal.h"
#include "mc/core/java/java.h"
#include <stdlib.h>
#include <string.h>

static const char draw_descriptor[] = "(Ljava/lang/String;IIII)V";
static const char height_descriptor[] = "(Ljava/lang/String;I)I";
static const char plain_descriptor[] = "(Ljava/lang/String;III)V";

typedef struct wrap_call {
    mc_client_bitmap_font *font;
    mc_client_bitmap_font_error *error;
    uint16_t *units;
    size_t capacity, work_left, strings_left;
    const char *recursive_descriptor;
    int32_t recursive_pc;
} wrap_call;

typedef struct split_cursor {
    mc_player_string_view input;
    size_t end, next, remaining;
    uint16_t delimiter;
} split_cursor;

static bool fail(wrap_call *call, mc_client_bitmap_font_status status, const char *operation,
                 const char *descriptor, int32_t pc, bool native_policy) {
    mc_client_bitmap_font_fail(call->error, status, operation, "a", descriptor, pc, native_policy);
    return false;
}

static void caller(wrap_call *call, const char *descriptor, int32_t pc) {
    mc_client_bitmap_font_error *e = call->error;
    if (e->parent_descriptor == NULL) {
        e->parent_method = "a";
        e->parent_descriptor = descriptor;
        e->parent_pc = pc;
    } else if (e->entry_descriptor == NULL) {
        e->entry_method = "a";
        e->entry_descriptor = descriptor;
        e->entry_pc = pc;
    } else {
        /* Split children contain no newline, so the actual source graph has
         * at most this one further frame: root draw38 -> leaf -> b7 -> inner. */
        call->recursive_descriptor = descriptor;
        call->recursive_pc = pc;
    }
}

static bool work(wrap_call *call, size_t units, const char *descriptor, int32_t pc) {
    if (units >= call->work_left)
        return fail(call, MC_CLIENT_BITMAP_FONT_LIMIT, "sj native wrapped work bound", descriptor,
                    pc, true);
    call->work_left -= units + 1u;
    return true;
}

static bool materialize(wrap_call *call, size_t units, const char *descriptor, int32_t pc) {
    /* Charge one unit for the finite String occurrence as well as its content.
     * Native spans do not reproduce the JVM's temporary allocation schedule. */
    if (units >= call->strings_left)
        return fail(call, MC_CLIENT_BITMAP_FONT_LIMIT, "sj native wrapped String-work bound",
                    descriptor, pc, true);
    call->strings_left -= units + 1u;
    return true;
}

static mc_player_string_view slice(mc_player_string_view input, size_t begin, size_t length) {
    return (mc_player_string_view){begin == 0 ? input.units : input.units + begin, length};
}

static bool split(wrap_call *call, mc_player_string_view input, uint16_t delimiter,
                  const char *descriptor, int32_t pc, split_cursor *out) {
    if (!work(call, 2u * input.length, descriptor, pc))
        return false;
    size_t end = input.length;
    while (end > 0 && input.units[end - 1u] == delimiter)
        --end;
    /* Java split(regex) retains the empty input but removes every trailing
     * empty result of a positive-width delimiter, including all-delimiter text. */
    size_t count = input.length == 0 || end != 0 ? 1u : 0u;
    for (size_t i = 0; i < end; ++i)
        if (input.units[i] == delimiter)
            ++count;
    if (!materialize(call, input.length + count, descriptor, pc))
        return false;
    *out = (split_cursor){input, end, 0, count, delimiter};
    return true;
}

static bool next(wrap_call *call, split_cursor *cursor, const char *descriptor, int32_t pc,
                 mc_player_string_view *out) {
    const size_t begin = cursor->next;
    size_t end = begin;
    while (end < cursor->end && cursor->input.units[end] != cursor->delimiter)
        ++end;
    if (!work(call, end - begin, descriptor, pc))
        return false;
    *out = slice(cursor->input, begin, end - begin);
    cursor->next = end + 1u;
    --cursor->remaining;
    return true;
}

static bool begin_line(wrap_call *call, mc_player_string_view word, const char *descriptor,
                       int32_t builder_pc, int32_t string_pc, mc_player_string_view *line) {
    if (call->units == NULL) {
        call->units = malloc(call->capacity * sizeof(*call->units));
        if (call->units == NULL)
            return fail(call, MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED,
                        "sj native wrapped UTF16 scratch", descriptor, builder_pc, true);
    }
    if (word.length >= call->capacity)
        return fail(call, MC_CLIENT_BITMAP_FONT_LIMIT, "sj native wrapped scratch bound",
                    descriptor, string_pc, true);
    if (!work(call, word.length + 1u, descriptor, string_pc) ||
        !materialize(call, word.length + 1u, descriptor, string_pc))
        return false;
    if (word.length != 0)
        memcpy(call->units, word.units, word.length * sizeof(*word.units));
    call->units[word.length] = ' ';
    *line = (mc_player_string_view){call->units, word.length + 1u};
    return true;
}

static bool candidate(wrap_call *call, mc_player_string_view line, mc_player_string_view word,
                      const char *descriptor, int32_t pc, mc_player_string_view *out) {
    if (word.length > call->capacity - line.length)
        return fail(call, MC_CLIENT_BITMAP_FONT_LIMIT, "sj native wrapped scratch bound",
                    descriptor, pc, true);
    const size_t length = line.length + word.length;
    if (!work(call, length, descriptor, pc) || !materialize(call, length, descriptor, pc))
        return false;
    if (word.length != 0)
        memcpy(call->units + line.length, word.units, word.length * sizeof(*word.units));
    *out = (mc_player_string_view){call->units, length};
    return true;
}

static bool append_space(wrap_call *call, mc_player_string_view joined, const char *descriptor,
                         int32_t pc, mc_player_string_view *line) {
    if (joined.length >= call->capacity)
        return fail(call, MC_CLIENT_BITMAP_FONT_LIMIT, "sj native wrapped scratch bound",
                    descriptor, pc, true);
    if (!work(call, joined.length + 1u, descriptor, pc) ||
        !materialize(call, joined.length + 1u, descriptor, pc))
        return false;
    call->units[joined.length] = ' ';
    *line = (mc_player_string_view){call->units, joined.length + 1u};
    return true;
}

static bool measure(wrap_call *call, mc_player_string_view text, const char *descriptor, int32_t pc,
                    int32_t *out) {
    if (!work(call, text.length, descriptor, pc))
        return false;
    mc_client_bitmap_font_measure_entered(call->font, text, out, call->error);
    if (call->error->status != MC_CLIENT_BITMAP_FONT_OK) {
        caller(call, descriptor, pc);
        return false;
    }
    return true;
}

static bool trimmed_nonempty(wrap_call *call, mc_player_string_view text, const char *descriptor,
                             int32_t pc, bool *out) {
    if (!work(call, text.length, descriptor, pc))
        return false;
    size_t begin = 0, end = text.length;
    while (begin < end && text.units[begin] <= UINT16_C(0x0020))
        ++begin;
    while (end > begin && text.units[end - 1u] <= UINT16_C(0x0020))
        --end;
    if (!materialize(call, end - begin, descriptor, pc))
        return false;
    *out = begin != end;
    return true;
}

static bool draw_plain(wrap_call *call, mc_player_string_view text, int32_t x, int32_t y,
                       uint32_t argb, int32_t pc) {
    if (!work(call, text.length, draw_descriptor, pc))
        return false;
    if (mc_client_bitmap_font_draw_entered(call->font, text, x, y, argb, false, call->error))
        return true;
    call->error->parent_method = "b";
    call->error->parent_descriptor = plain_descriptor;
    call->error->parent_pc = 7;
    call->error->entry_method = "a";
    call->error->entry_descriptor = draw_descriptor;
    call->error->entry_pc = pc;
    return false;
}

static bool wrapped_height(wrap_call *, mc_player_string_view, int32_t, int32_t *);

static bool draw_wrapped(wrap_call *call, mc_player_string_view input, int32_t x, int32_t y,
                         int32_t width, uint32_t argb) {
    split_cursor lines;
    if (!split(call, input, '\n', draw_descriptor, 3, &lines))
        return false;
    if (lines.remaining > 1u) {
        while (lines.remaining != 0) {
            mc_player_string_view line;
            if (!next(call, &lines, draw_descriptor, 31, &line))
                return false;
            if (!draw_wrapped(call, line, x, y, width, argb)) {
                caller(call, draw_descriptor, 38);
                return false;
            }
            int32_t height;
            if (!wrapped_height(call, line, width, &height)) {
                caller(call, draw_descriptor, 50);
                return false;
            }
            y = mc_java_i32_add(y, height);
        }
        return true;
    }
    /* Source uses the ORIGINAL String at62, even if split3 removed its final
     * newlines and left zero or one result. Do not wrap lines.input[0]. */
    split_cursor words;
    if (!split(call, input, ' ', draw_descriptor, 65, &words))
        return false;
    while (words.remaining != 0) {
        mc_player_string_view word, line;
        if (!next(call, &words, draw_descriptor, 95, &word) ||
            !begin_line(call, word, draw_descriptor, 81, 104, &line))
            return false;
        while (words.remaining != 0) {
            split_cursor peek = words;
            mc_player_string_view following, joined;
            int32_t measured;
            if (!next(call, &peek, draw_descriptor, 134, &following) ||
                !candidate(call, line, following, draw_descriptor, 138, &joined) ||
                !measure(call, joined, draw_descriptor, 141, &measured))
                return false;
            if (measured >= width)
                break;
            words = peek;
            if (!append_space(call, joined, draw_descriptor, 177, &line))
                return false;
        }
        for (;;) {
            int32_t measured;
            if (!measure(call, line, draw_descriptor, 188, &measured))
                return false;
            if (measured <= width)
                break;
            size_t prefix = 0;
            for (;;) {
                if (prefix >= line.length) {
                    call->error->actual_index = (int32_t)(prefix + 1u);
                    return fail(call, MC_CLIENT_BITMAP_FONT_STRING_INDEX,
                                "sj actual String.substring end index", draw_descriptor, 207,
                                false);
                }
                const mc_player_string_view test = slice(line, 0, prefix + 1u);
                if (!materialize(call, test.length, draw_descriptor, 207) ||
                    !measure(call, test, draw_descriptor, 210, &measured))
                    return false;
                if (measured > width)
                    break;
                ++prefix;
            }
            const mc_player_string_view part = slice(line, 0, prefix);
            bool nonempty;
            if (!materialize(call, part.length, draw_descriptor, 229) ||
                !trimmed_nonempty(call, part, draw_descriptor, 232, &nonempty))
                return false;
            if (nonempty) {
                if (!materialize(call, part.length, draw_descriptor, 247) ||
                    !draw_plain(call, part, x, y, argb, 254))
                    return false;
                y = mc_java_i32_add(y, 8);
            }
            if (!materialize(call, line.length - prefix, draw_descriptor, 264))
                return false;
            line = slice(line, prefix, line.length - prefix);
            if (prefix == 0)
                return fail(call, MC_CLIENT_BITMAP_FONT_LIMIT,
                            "sj native nonprogress after actual substring", draw_descriptor, 264,
                            true);
        }
        bool nonempty;
        if (!trimmed_nonempty(call, line, draw_descriptor, 274, &nonempty))
            return false;
        if (nonempty) {
            if (!draw_plain(call, line, x, y, argb, 290))
                return false;
            y = mc_java_i32_add(y, 8);
        }
    }
    return true;
}

static bool wrapped_height(wrap_call *call, mc_player_string_view input, int32_t width,
                           int32_t *out) {
    split_cursor lines;
    if (!split(call, input, '\n', height_descriptor, 3, &lines))
        return false;
    if (lines.remaining > 1u) {
        int32_t height = 0;
        while (lines.remaining != 0) {
            mc_player_string_view line;
            if (!next(call, &lines, height_descriptor, 32, &line))
                return false;
            int32_t child;
            if (!wrapped_height(call, line, width, &child)) {
                caller(call, height_descriptor, 34);
                return false;
            }
            height = mc_java_i32_add(height, child);
        }
        *out = height;
        return true;
    }
    split_cursor words;
    if (!split(call, input, ' ', height_descriptor, 52, &words))
        return false;
    int32_t height = 0;
    while (words.remaining != 0) {
        mc_player_string_view word, line;
        if (!next(call, &words, height_descriptor, 85, &word) ||
            !begin_line(call, word, height_descriptor, 71, 94, &line))
            return false;
        while (words.remaining != 0) {
            split_cursor peek = words;
            mc_player_string_view following, joined;
            int32_t measured;
            if (!next(call, &peek, height_descriptor, 124, &following) ||
                !candidate(call, line, following, height_descriptor, 128, &joined) ||
                !measure(call, joined, height_descriptor, 131, &measured))
                return false;
            if (measured >= width)
                break;
            words = peek;
            if (!append_space(call, joined, height_descriptor, 166, &line))
                return false;
        }
        for (;;) {
            int32_t measured;
            if (!measure(call, line, height_descriptor, 177, &measured))
                return false;
            if (measured <= width)
                break;
            size_t prefix = 0;
            for (;;) {
                if (prefix >= line.length) {
                    call->error->actual_index = (int32_t)(prefix + 1u);
                    return fail(call, MC_CLIENT_BITMAP_FONT_STRING_INDEX,
                                "sj actual String.substring end index", height_descriptor, 195,
                                false);
                }
                const mc_player_string_view test = slice(line, 0, prefix + 1u);
                if (!materialize(call, test.length, height_descriptor, 195) ||
                    !measure(call, test, height_descriptor, 198, &measured))
                    return false;
                if (measured > width)
                    break;
                ++prefix;
            }
            const mc_player_string_view part = slice(line, 0, prefix);
            bool nonempty;
            if (!materialize(call, part.length, height_descriptor, 216) ||
                !trimmed_nonempty(call, part, height_descriptor, 219, &nonempty))
                return false;
            if (nonempty)
                height = mc_java_i32_add(height, 8);
            if (!materialize(call, line.length - prefix, height_descriptor, 235))
                return false;
            line = slice(line, prefix, line.length - prefix);
            if (prefix == 0)
                return fail(call, MC_CLIENT_BITMAP_FONT_LIMIT,
                            "sj native nonprogress after actual substring", height_descriptor, 235,
                            true);
        }
        bool nonempty;
        if (!trimmed_nonempty(call, line, height_descriptor, 245, &nonempty))
            return false;
        if (nonempty)
            height = mc_java_i32_add(height, 8);
    }
    if (height < 8)
        height = mc_java_i32_add(height, 8); /* Actual iinc267, not a maximum. */
    *out = height;
    return true;
}

static void initialize_error(mc_client_bitmap_font_wrap_error *error) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->recursive_pc = -1;
    }
}

static mc_client_bitmap_font_status null_text(const char *descriptor,
                                              mc_client_bitmap_font_wrap_error *error) {
    if (error != NULL)
        mc_client_bitmap_font_fail(&error->font, MC_CLIENT_BITMAP_FONT_NULL_REFERENCE,
                                   "sj actual String.split receiver", "a", descriptor, 3, false);
    return MC_CLIENT_BITMAP_FONT_NULL_REFERENCE;
}

static mc_client_bitmap_font_status leave(wrap_call *call,
                                          mc_client_bitmap_font_wrap_error *error) {
    free(call->units);
    if (error != NULL) {
        error->recursive_descriptor = call->recursive_descriptor;
        error->recursive_pc = call->recursive_pc;
    }
    return mc_client_bitmap_font_view_leave(call->font, call->error,
                                            error == NULL ? NULL : &error->font);
}

mc_client_bitmap_font_status mc_client_bitmap_font_draw_wrapped_view(
    mc_client_bitmap_font *font, const mc_player_string_view *text, int32_t x, int32_t y,
    int32_t width, uint32_t argb, mc_client_bitmap_font_wrap_error *error) {
    initialize_error(error);
    mc_client_bitmap_font_status status = mc_client_bitmap_font_admit(
        font, "a", draw_descriptor, error == NULL ? NULL : &error->font);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    if (text == NULL)
        return null_text(draw_descriptor, error);
    mc_client_bitmap_font_error *e;
    status = mc_client_bitmap_font_view_enter(font, text, "a", draw_descriptor, &e,
                                              error == NULL ? NULL : &error->font);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    wrap_call call = {font, e, NULL, text->length + 1u, 16777216u, 16777216u, NULL, -1};
    (void)draw_wrapped(&call, *text, x, y, width, argb);
    return leave(&call, error);
}

mc_client_bitmap_font_status
mc_client_bitmap_font_wrapped_height_view(mc_client_bitmap_font *font,
                                          const mc_player_string_view *text, int32_t width,
                                          int32_t *out, mc_client_bitmap_font_wrap_error *error) {
    initialize_error(error);
    mc_client_bitmap_font_status status = mc_client_bitmap_font_admit(
        font, "a", height_descriptor, error == NULL ? NULL : &error->font);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    if (out == NULL) {
        if (error != NULL)
            mc_client_bitmap_font_fail(&error->font, MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT,
                                       "sj native wrapped height output", "a", height_descriptor,
                                       -1, true);
        return MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT;
    }
    if (text == NULL)
        return null_text(height_descriptor, error);
    mc_client_bitmap_font_error *e;
    status = mc_client_bitmap_font_view_enter(font, text, "a", height_descriptor, &e,
                                              error == NULL ? NULL : &error->font);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    wrap_call call = {font, e, NULL, text->length + 1u, 16777216u, 16777216u, NULL, -1};
    int32_t height;
    if (wrapped_height(&call, *text, width, &height))
        *out = height;
    return leave(&call, error);
}
