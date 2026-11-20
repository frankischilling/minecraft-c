#include "render_global_update_sort.h"

#include "render_global_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Independently authored native representation of the observed JDK17 object
 * TimSort algorithm. Branch/call order matters: MD returns -1 even for the
 * same ordinal, so a different otherwise-valid sort changes source behavior. */
typedef struct update_sort {
    mc_client_source_dk **items, **scratch;
    int32_t length, scratch_length, starts[49], lengths[49], runs, gallop;
    void *context;
    bool (*compare)(void *, mc_client_source_dk *, mc_client_source_dk *, int *);
    mc_client_render_global_error *error;
} update_sort;

static bool failure(update_sort *s, mc_client_render_global_status status, const char *operation) {
    mc_render_global_source_fail(s->error, status, operation, "n", "a", "(Lls;Z)Z", 411, true);
    return false;
}

static bool contract(update_sort *s, bool backwards) {
    mc_render_global_source_fail(s->error, MC_CLIENT_RENDER_GLOBAL_SOURCE_ILLEGAL_ARGUMENT,
                                 "Comparison method violates its general contract!", "n", "a",
                                 "(Lls;Z)Z", 411, false);
    s->error->child = (mc_client_graphics_site){
        "java/util/TimSort", backwards ? "mergeHi" : "mergeLo", "(IIII)V", backwards ? 675 : 607};
    return false;
}

static bool cmp(update_sort *s, mc_client_source_dk *left, mc_client_source_dk *right, int *out) {
    return s->compare(s->context, left, right, out);
}

static void move(mc_client_source_dk **to, mc_client_source_dk **from, int32_t count) {
    memmove(to, from, (size_t)count * sizeof(*to));
}

static bool scratch(update_sort *s, int32_t wanted) {
    if (wanted <= s->scratch_length)
        return true;
    /* JDK17 grows to the strictly greater power of two, capped at n/2.
     * Unsigned calculation avoids introducing C signed-shift overflow. */
    uint32_t capacity = 1;
    while (capacity <= (uint32_t)wanted)
        capacity <<= 1u;
    if (capacity > (uint32_t)s->length / 2u)
        capacity = (uint32_t)s->length / 2u;
    mc_client_source_dk **next = malloc((size_t)capacity * sizeof(*next));
    if (next == NULL)
        return failure(s, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                       "native N update TimSort temporary allocation");
    /* No source array mutation occurs while growing this private workspace. */
    free(s->scratch);
    s->scratch = next;
    s->scratch_length = (int32_t)capacity;
    return true;
}

static bool insertion(update_sort *s, int32_t first, int32_t end, int32_t next) {
    if (next == first)
        ++next;
    for (; next < end; ++next) {
        mc_client_source_dk *value = s->items[next];
        int32_t low = first, high = next;
        while (low < high) {
            const int32_t middle = low + (high - low) / 2;
            int order;
            if (!cmp(s, value, s->items[middle], &order))
                return false;
            if (order < 0)
                high = middle;
            else
                low = middle + 1;
        }
        move(s->items + low + 1, s->items + low, next - low);
        s->items[low] = value;
    }
    return true;
}

static bool natural_run(update_sort *s, int32_t first, int32_t end, int32_t *length) {
    int32_t next = first + 1;
    if (next == end) {
        *length = 1;
        return true;
    }
    int order;
    if (!cmp(s, s->items[next++], s->items[first], &order))
        return false;
    const bool descending = order < 0;
    while (next < end) {
        if (!cmp(s, s->items[next], s->items[next - 1], &order))
            return false;
        if ((order < 0) != descending)
            break;
        ++next;
    }
    if (descending)
        for (int32_t low = first, high = next - 1; low < high; ++low, --high) {
            mc_client_source_dk *value = s->items[low];
            s->items[low] = s->items[high];
            s->items[high] = value;
        }
    *length = next - first;
    return true;
}

static int32_t advance(int32_t offset, int32_t maximum) {
    return offset > (INT32_MAX - 1) / 2 ? maximum : offset * 2 + 1;
}

/* upper=false finds the first >= key; upper=true finds the first > key.
 * Both search directions reproduce the JDK's initial hint comparison,
 * exponential probes and binary probes exactly, even for nonantisymmetry. */
static bool search(update_sort *s, mc_client_source_dk *key, mc_client_source_dk **array,
                   int32_t base, int32_t length, int32_t hint, bool upper, int32_t *out) {
    int order;
    if (!cmp(s, key, array[base + hint], &order))
        return false;
    int32_t previous = 0, offset = 1;
    if (upper ? order >= 0 : order > 0) {
        const int32_t maximum = length - hint;
        while (offset < maximum) {
            if (!cmp(s, key, array[base + hint + offset], &order))
                return false;
            if (upper ? order < 0 : order <= 0)
                break;
            previous = offset;
            offset = advance(offset, maximum);
        }
        if (offset > maximum)
            offset = maximum;
        previous += hint;
        offset += hint;
    } else {
        const int32_t maximum = hint + 1;
        while (offset < maximum) {
            if (!cmp(s, key, array[base + hint - offset], &order))
                return false;
            if (upper ? order >= 0 : order > 0)
                break;
            previous = offset;
            offset = advance(offset, maximum);
        }
        if (offset > maximum)
            offset = maximum;
        const int32_t old_previous = previous;
        previous = hint - offset;
        offset = hint - old_previous;
    }
    ++previous;
    while (previous < offset) {
        const int32_t middle = previous + (offset - previous) / 2;
        if (!cmp(s, key, array[base + middle], &order))
            return false;
        if (upper ? order >= 0 : order > 0)
            previous = middle + 1;
        else
            offset = middle;
    }
    *out = offset;
    return true;
}

static bool forward_merge(update_sort *s, int32_t first, int32_t left, int32_t second,
                          int32_t right) {
    if (!scratch(s, left))
        return false;
    move(s->scratch, s->items + first, left);
    int32_t a = 0, b = second, dest = first;
    s->items[dest++] = s->items[b++];
    if (--right == 0) {
        move(s->items + dest, s->scratch + a, left);
        return true;
    }
    if (left == 1)
        goto finish;
    int32_t threshold = s->gallop;
    for (;;) {
        int32_t wins_left = 0, wins_right = 0;
        do {
            int order;
            if (!cmp(s, s->items[b], s->scratch[a], &order))
                return false;
            if (order < 0) {
                s->items[dest++] = s->items[b++];
                ++wins_right;
                wins_left = 0;
                if (--right == 0)
                    goto merged;
            } else {
                s->items[dest++] = s->scratch[a++];
                ++wins_left;
                wins_right = 0;
                if (--left == 1)
                    goto merged;
            }
        } while ((wins_left | wins_right) < threshold);
        do {
            if (!search(s, s->items[b], s->scratch, a, left, 0, true, &wins_left))
                return false;
            if (wins_left != 0) {
                move(s->items + dest, s->scratch + a, wins_left);
                dest += wins_left;
                a += wins_left;
                left -= wins_left;
                if (left <= 1)
                    goto merged;
            }
            s->items[dest++] = s->items[b++];
            if (--right == 0)
                goto merged;
            if (!search(s, s->scratch[a], s->items, b, right, 0, false, &wins_right))
                return false;
            if (wins_right != 0) {
                move(s->items + dest, s->items + b, wins_right);
                dest += wins_right;
                b += wins_right;
                right -= wins_right;
                if (right == 0)
                    goto merged;
            }
            s->items[dest++] = s->scratch[a++];
            if (--left == 1)
                goto merged;
            --threshold;
        } while (wins_left >= 7 || wins_right >= 7);
        if (threshold < 0)
            threshold = 0;
        threshold += 2;
    }
merged:
    s->gallop = threshold < 1 ? 1 : threshold;
finish:
    if (left == 1) {
        move(s->items + dest, s->items + b, right);
        s->items[dest + right] = s->scratch[a];
    } else if (left == 0) {
        return contract(s, false);
    } else {
        move(s->items + dest, s->scratch + a, left);
    }
    return true;
}

static bool backward_merge(update_sort *s, int32_t first, int32_t left, int32_t second,
                           int32_t right) {
    if (!scratch(s, right))
        return false;
    move(s->scratch, s->items + second, right);
    int32_t a = first + left - 1, b = right - 1, dest = second + right - 1;
    s->items[dest--] = s->items[a--];
    if (--left == 0) {
        move(s->items + dest - (right - 1), s->scratch, right);
        return true;
    }
    if (right == 1)
        goto finish;
    int32_t threshold = s->gallop;
    for (;;) {
        int32_t wins_left = 0, wins_right = 0;
        do {
            int order;
            if (!cmp(s, s->scratch[b], s->items[a], &order))
                return false;
            if (order < 0) {
                s->items[dest--] = s->items[a--];
                ++wins_left;
                wins_right = 0;
                if (--left == 0)
                    goto merged;
            } else {
                s->items[dest--] = s->scratch[b--];
                ++wins_right;
                wins_left = 0;
                if (--right == 1)
                    goto merged;
            }
        } while ((wins_left | wins_right) < threshold);
        do {
            int32_t position;
            if (!search(s, s->scratch[b], s->items, first, left, left - 1, true, &position))
                return false;
            wins_left = left - position;
            if (wins_left != 0) {
                dest -= wins_left;
                a -= wins_left;
                left -= wins_left;
                move(s->items + dest + 1, s->items + a + 1, wins_left);
                if (left == 0)
                    goto merged;
            }
            s->items[dest--] = s->scratch[b--];
            if (--right == 1)
                goto merged;
            if (!search(s, s->items[a], s->scratch, 0, right, right - 1, false, &position))
                return false;
            wins_right = right - position;
            if (wins_right != 0) {
                dest -= wins_right;
                b -= wins_right;
                right -= wins_right;
                move(s->items + dest + 1, s->scratch + b + 1, wins_right);
                if (right <= 1)
                    goto merged;
            }
            s->items[dest--] = s->items[a--];
            if (--left == 0)
                goto merged;
            --threshold;
        } while (wins_left >= 7 || wins_right >= 7);
        if (threshold < 0)
            threshold = 0;
        threshold += 2;
    }
merged:
    s->gallop = threshold < 1 ? 1 : threshold;
finish:
    if (right == 1) {
        dest -= left;
        a -= left;
        move(s->items + dest + 1, s->items + a + 1, left);
        s->items[dest] = s->scratch[b];
    } else if (right == 0) {
        return contract(s, true);
    } else {
        move(s->items + dest - (right - 1), s->scratch, right);
    }
    return true;
}

static bool merge(update_sort *s, int32_t index) {
    int32_t first = s->starts[index], left = s->lengths[index];
    const int32_t second = s->starts[index + 1];
    int32_t right = s->lengths[index + 1];
    s->lengths[index] = left + right;
    if (index == s->runs - 3) {
        s->starts[index + 1] = s->starts[index + 2];
        s->lengths[index + 1] = s->lengths[index + 2];
    }
    --s->runs;
    int32_t skip;
    if (!search(s, s->items[second], s->items, first, left, 0, true, &skip))
        return false;
    first += skip;
    left -= skip;
    if (left == 0)
        return true;
    if (!search(s, s->items[first + left - 1], s->items, second, right, right - 1, false, &right))
        return false;
    if (right == 0)
        return true;
    return left <= right ? forward_merge(s, first, left, second, right)
                         : backward_merge(s, first, left, second, right);
}

static bool collapse(update_sort *s, bool final) {
    while (s->runs > 1) {
        int32_t index = s->runs - 2;
        if (final) {
            if (index > 0 && s->lengths[index - 1] < s->lengths[index + 1])
                --index;
        } else if ((index > 0 &&
                    s->lengths[index - 1] <= s->lengths[index] + s->lengths[index + 1]) ||
                   (index > 1 &&
                    s->lengths[index - 2] <= s->lengths[index] + s->lengths[index - 1])) {
            if (s->lengths[index - 1] < s->lengths[index + 1])
                --index;
        } else if (s->lengths[index] > s->lengths[index + 1]) {
            break;
        }
        if (!merge(s, index))
            return false;
    }
    return true;
}

bool mc_render_global_update_sort(mc_client_source_dk **array, size_t count, void *context,
                                  bool (*compare)(void *, mc_client_source_dk *,
                                                  mc_client_source_dk *, int *),
                                  mc_client_render_global_error *error) {
    if (error == NULL)
        return false;
    *error = (mc_client_render_global_error){0};
    update_sort sort = {
        .items = array, .context = context, .compare = compare, .error = error, .gallop = 7};
    if ((array == NULL && count != 0) || compare == NULL)
        return failure(&sort, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                       "native N update TimSort array/comparator");
    if (count > INT32_MAX || count > SIZE_MAX / sizeof(*array))
        return failure(&sort, MC_CLIENT_RENDER_GLOBAL_LIMIT,
                       "native N update TimSort array extent");
    sort.length = (int32_t)count;
    if (count < 2)
        return true;
    int32_t run;
    if (count < 32)
        return natural_run(&sort, 0, sort.length, &run) && insertion(&sort, 0, sort.length, run);
    sort.scratch_length = count < 512 ? sort.length / 2 : 256;
    sort.scratch = malloc((size_t)sort.scratch_length * sizeof(*sort.scratch));
    if (sort.scratch == NULL)
        return failure(&sort, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                       "native N update TimSort initial temporary allocation");
    int32_t minimum = sort.length, remainder = 0;
    while (minimum >= 32) {
        remainder |= minimum & 1;
        minimum /= 2;
    }
    minimum += remainder;
    bool ok = true;
    int32_t first = 0;
    while (first < sort.length) {
        if (!natural_run(&sort, first, sort.length, &run)) {
            ok = false;
            break;
        }
        if (run < minimum) {
            const int32_t available = sort.length - first;
            const int32_t forced = available <= minimum ? available : minimum;
            if (!insertion(&sort, first, first + forced, first + run)) {
                ok = false;
                break;
            }
            run = forced;
        }
        if (sort.runs == 49) {
            ok = failure(&sort, MC_CLIENT_RENDER_GLOBAL_LIMIT,
                         "native N update TimSort run stack extent");
            break;
        }
        sort.starts[sort.runs] = first;
        sort.lengths[sort.runs++] = run;
        if (!collapse(&sort, false)) {
            ok = false;
            break;
        }
        first += run;
    }
    if (ok)
        ok = collapse(&sort, true);
    free(sort.scratch);
    return ok;
}
