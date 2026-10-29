#include "url_path.h"
#include <string.h>

bool mc_url_path_separator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

bool mc_url_path_absolute(const char *path, size_t length) {
#ifdef _WIN32
    if (length >= 4u && mc_url_path_separator(path[0]) && mc_url_path_separator(path[1]) &&
        (path[2] == '.' || path[2] == '?') && mc_url_path_separator(path[3]))
        return false;
    return (length >= 3u &&
            ((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) &&
            path[1] == ':' && mc_url_path_separator(path[2])) ||
           (length >= 3u && mc_url_path_separator(path[0]) && mc_url_path_separator(path[1]) &&
            !mc_url_path_separator(path[2]));
#else
    return length != 0 && path[0] == '/';
#endif
}

bool mc_url_path_next_utf8(const char *text, size_t length, size_t *offset, uint32_t *out) {
    const unsigned char *bytes = (const unsigned char *)text;
    uint32_t value = bytes[(*offset)++];
    if (value < 128u) {
        *out = value;
        return true;
    }
    size_t following;
    uint32_t minimum;
    if (value >= 194u && value <= 223u) {
        following = 1;
        minimum = 128u;
        value &= 31u;
    } else if (value >= 224u && value <= 239u) {
        following = 2;
        minimum = 2048u;
        value &= 15u;
    } else if (value >= 240u && value <= 244u) {
        following = 3;
        minimum = 65536u;
        value &= 7u;
    } else {
        return false;
    }
    if (following > length - *offset)
        return false;
    for (size_t i = 0; i < following; ++i) {
        const uint32_t next = bytes[(*offset)++];
        if ((next & 192u) != 128u)
            return false;
        value = value * 64u + (next & 63u);
    }
    if (value < minimum || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu))
        return false;
    *out = value;
    return true;
}

static bool url_quote_ascii(uint32_t unit) {
    if ((unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') ||
        (unit >= '0' && unit <= '9'))
        return false;
    const char *allowed = "-_.!~*'():@&=+$,;/";
    return unit == 0 || strchr(allowed, (int)unit) == NULL;
}

static bool url_quote_unicode(uint32_t unit) {
    return (unit >= 0x80u && unit <= 0x9fu) || unit == 0xa0u || unit == 0x1680u ||
           (unit >= 0x2000u && unit <= 0x200au) || unit == 0x2028u || unit == 0x2029u ||
           unit == 0x202fu || unit == 0x205fu || unit == 0x3000u;
}

bool mc_url_path_encode(const char *path, size_t length, bool is_directory, uint16_t *destination,
                        size_t capacity, size_t *out_length) {
    if (path == NULL || destination == NULL || out_length == NULL || length == 0 ||
        length > (SIZE_MAX - 6u) / 3u || capacity < length * 3u + 6u)
        return false;
    size_t url_at = 0;
#ifdef _WIN32
    if (mc_url_path_separator(path[0]) && mc_url_path_separator(path[1])) {
        destination[url_at++] = '/';
        destination[url_at++] = '/';
    } else {
        destination[url_at++] = '/';
    }
#endif
    static const char hex[] = "0123456789ABCDEF";
    for (size_t at = 0; at < length;) {
        const size_t first = at;
        uint32_t unit;
        (void)mc_url_path_next_utf8(path, length, &at, &unit);
        if (mc_url_path_separator(path[first])) {
            destination[url_at++] = '/';
        } else if ((unit < 128u && url_quote_ascii(unit)) || url_quote_unicode(unit)) {
            for (size_t i = first; i < at; ++i) {
                const unsigned int byte = (unsigned char)path[i];
                destination[url_at++] = '%';
                destination[url_at++] = (uint16_t)hex[byte >> 4u];
                destination[url_at++] = (uint16_t)hex[byte & 15u];
            }
        } else if (unit >= 65536u) {
            const uint32_t pair = unit - 65536u;
            destination[url_at++] = (uint16_t)(0xd800u + (pair >> 10u));
            destination[url_at++] = (uint16_t)(0xdc00u + (pair & 1023u));
        } else {
            destination[url_at++] = (uint16_t)unit;
        }
    }
    if (is_directory && destination[url_at - 1u] != '/')
        destination[url_at++] = '/';
    destination[url_at] = 0;
    *out_length = url_at;
    return true;
}
