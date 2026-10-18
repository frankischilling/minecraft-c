#include "mc/storage/region/region.h"

#include <limits.h>

_Static_assert(CHAR_BIT == 8, "Region storage requires eight-bit bytes");

mc_region_limits mc_region_default_limits(void)
{
    const mc_region_limits limits = {1024u * 1024u * 1024u};
    return limits;
}

const char *mc_region_status_string(mc_region_status status)
{
    switch (status) {
        case MC_REGION_OK:
            return "ok";
        case MC_REGION_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_REGION_TRUNCATED_HEADER:
            return "truncated region header";
        case MC_REGION_LIMIT:
            return "resource limit";
        case MC_REGION_INVALID_COORDINATE:
            return "invalid local coordinate";
        case MC_REGION_MISSING:
            return "missing region entry";
        case MC_REGION_INVALID_RECORD:
            return "invalid region record";
        case MC_REGION_UNSUPPORTED_COMPRESSION:
            return "unsupported compression selector";
    }
    return "unknown region status";
}

static uint32_t region_read_u32(const uint8_t *input)
{
    return (uint32_t)input[0] << 24u | (uint32_t)input[1] << 16u | (uint32_t)input[2] << 8u |
           input[3];
}

static void region_read_locations(mc_region_view *view)
{
    for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
        mc_region_entry *entry = &view->entries[i];
        entry->location = region_read_u32(view->input + 4u * i);
        entry->timestamp = region_read_u32(view->input + MC_REGION_SECTOR_BYTES + 4u * i);
        if (entry->location == 0) {
            continue;
        }
        ++view->present_entries;
        uint64_t start = entry->location >> 8u;
        uint64_t count = entry->location & UINT32_C(255);
        entry->byte_offset = start * MC_REGION_SECTOR_BYTES;
        entry->extent_bytes = count * MC_REGION_SECTOR_BYTES;
        if (start < 2) {
            entry->issues |= MC_REGION_ISSUE_HEADER_SECTOR;
        }
        if (count == 0) {
            entry->issues |= MC_REGION_ISSUE_EMPTY_EXTENT;
        }
        if (entry->byte_offset > (uint64_t)view->length ||
            entry->extent_bytes > (uint64_t)view->length - entry->byte_offset) {
            entry->issues |= MC_REGION_ISSUE_EXTENT_BOUNDS;
        }
    }
}

static void region_check_overlaps(mc_region_view *view)
{
    /* There are at most 1024 packed ranges. Pairwise comparisons keep storage
     * fixed, even when an entry names a sector far beyond the input. Packed
     * starts have 24 bits and counts have 8, so their byte endpoints fit u64. */
    for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
        mc_region_entry *left = &view->entries[i];
        if (left->location == 0 || left->extent_bytes == 0) {
            continue;
        }
        uint64_t left_end = left->byte_offset + left->extent_bytes;
        for (size_t j = i + 1; j < MC_REGION_ENTRY_COUNT; ++j) {
            mc_region_entry *right = &view->entries[j];
            if (right->location == 0 || right->extent_bytes == 0) {
                continue;
            }
            uint64_t right_end = right->byte_offset + right->extent_bytes;
            if (left->byte_offset < right_end && right->byte_offset < left_end) {
                left->issues |= MC_REGION_ISSUE_OVERLAP;
                right->issues |= MC_REGION_ISSUE_OVERLAP;
            }
        }
    }
}

static void region_read_records(mc_region_view *view)
{
    for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
        mc_region_entry *entry = &view->entries[i];
        if (entry->location == 0) {
            continue;
        }
        if (entry->issues == 0) {
            size_t offset = (size_t)entry->byte_offset;
            entry->record_length = region_read_u32(view->input + offset);
            if (entry->record_length == 0 || entry->record_length > INT32_MAX) {
                entry->issues |= MC_REGION_ISSUE_INVALID_LENGTH;
            } else {
                uint64_t record_bytes = UINT64_C(4) + entry->record_length;
                if (record_bytes > entry->extent_bytes ||
                    record_bytes > (uint64_t)view->length - entry->byte_offset) {
                    entry->issues |= MC_REGION_ISSUE_RECORD_BOUNDS;
                } else {
                    entry->selector = view->input[offset + 4u];
                    if (entry->selector != 1 && entry->selector != 2) {
                        entry->issues |= MC_REGION_ISSUE_UNSUPPORTED_SELECTOR;
                    }
                }
            }
        }
        if (entry->issues != 0) {
            ++view->invalid_entries;
        }
    }
}

mc_region_status mc_region_open(const uint8_t *input, size_t length,
                                const mc_region_limits *requested_limits, mc_region_view *out)
{
    if (out == NULL || (input == NULL && length != 0)) {
        return MC_REGION_INVALID_ARGUMENT;
    }
    mc_region_limits limits =
        requested_limits == NULL ? mc_region_default_limits() : *requested_limits;
    if (length > limits.max_input_bytes) {
        return MC_REGION_LIMIT;
    }
    if (length < MC_REGION_HEADER_BYTES) {
        return MC_REGION_TRUNCATED_HEADER;
    }
    mc_region_view candidate = {0};
    candidate.input = input;
    candidate.length = length;
    candidate.trailing_bytes = length % MC_REGION_SECTOR_BYTES;
    region_read_locations(&candidate);
    region_check_overlaps(&candidate);
    region_read_records(&candidate);
    *out = candidate;
    return MC_REGION_OK;
}

const mc_region_entry *mc_region_entry_at(const mc_region_view *view, int32_t x, int32_t z)
{
    if (view == NULL || x < 0 || x >= 32 || z < 0 || z >= 32) {
        return NULL;
    }
    return &view->entries[(size_t)x + 32u * (size_t)z];
}

mc_region_status mc_region_record(const mc_region_view *view, int32_t x, int32_t z,
                                  mc_region_payload *out)
{
    if (view == NULL || out == NULL) {
        return MC_REGION_INVALID_ARGUMENT;
    }
    const mc_region_entry *entry = mc_region_entry_at(view, x, z);
    if (entry == NULL) {
        return MC_REGION_INVALID_COORDINATE;
    }
    if (entry->location == 0) {
        return MC_REGION_MISSING;
    }
    if (entry->issues == MC_REGION_ISSUE_UNSUPPORTED_SELECTOR) {
        return MC_REGION_UNSUPPORTED_COMPRESSION;
    }
    if (entry->issues != 0) {
        return MC_REGION_INVALID_RECORD;
    }
    mc_region_payload payload = {view->input + (size_t)entry->byte_offset + 5u,
                                 (size_t)entry->record_length - 1u, entry->selector};
    *out = payload;
    return MC_REGION_OK;
}
