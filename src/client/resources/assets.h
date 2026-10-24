#ifndef MC_CLIENT_RESOURCES_ASSETS_H
#define MC_CLIENT_RESOURCES_ASSETS_H

#include "mc/client/gui/font_renderer.h"
#include "mc/core/java/java.h"
#include "mc/core/java/reader.h"
#include "mc/resources/translation/translation.h"

typedef struct mc_client_assets mc_client_assets;

#define MC_CLIENT_ASSETS_MAX_IMAGES 128u
#define MC_CLIENT_ASSETS_MAX_LABELS 256u
#define MC_CLIENT_ASSETS_MAX_NAME_BYTES 1024u
#define MC_CLIENT_ASSETS_MAX_TEXT_UNITS 65536u
#define MC_CLIENT_ASSETS_MAX_SPLASHES 4096u
#define MC_CLIENT_ASSETS_MAX_ENCODED_BYTES (16u * 1024u * 1024u)
#define MC_CLIENT_ASSETS_MAX_IMAGE_BYTES (128u * 1024u * 1024u)
#define MC_CLIENT_ASSETS_MAX_ARENA_BYTES (8u * 1024u * 1024u)

typedef enum mc_client_assets_status {
    MC_CLIENT_ASSETS_OK = 0,
    MC_CLIENT_ASSETS_INVALID_ARGUMENT,
    MC_CLIENT_ASSETS_LIMIT,
    MC_CLIENT_ASSETS_ALLOCATION_FAILED,
    MC_CLIENT_ASSETS_RESOURCE_FAILED,
    MC_CLIENT_ASSETS_IMAGE_FAILED,
    MC_CLIENT_ASSETS_UNSUPPORTED_IMAGE_FORMAT,
    MC_CLIENT_ASSETS_TRANSLATION_FAILED,
    MC_CLIENT_ASSETS_READER_FAILED,
    MC_CLIENT_ASSETS_RANDOM_FAILED
} mc_client_assets_status;

typedef struct mc_client_assets_error {
    mc_client_assets_status status;
    const char *operation;
    mc_registry_status resource_status, close_status;
    mc_java_properties_status read_status;
    mc_java_reader_status reader_status;
    mc_image_status image_status;
    uint8_t png_bit_depth, png_color_type;
    mc_registry_error registry_error;
} mc_client_assets_error;

/* Separate owned text; never pass a borrowed font/label view to this destructor. */
typedef struct mc_client_string {
    uint16_t *units;
    size_t length;
} mc_client_string;

const char *mc_client_assets_status_string(mc_client_assets_status status);

/* Borrow the App's live resource source and shared client Registry. The Registry
 * must have been allocated with this same source and its live material/profile
 * owners. Creation performs no I/O or class initialization. All dependencies
 * outlive this owner; free never retires them. Calls/views/free require exclusive
 * access, and callbacks must not reenter or retire these admitted owners. */
mc_client_assets_status mc_client_assets_create(const mc_registry_resources *resources,
                                                mc_registry_classes *classes,
                                                mc_client_assets **out);
void mc_client_assets_free(mc_client_assets *assets); /* NULL is harmless. */

/* Lazy case-sensitive cache of ordinary absolute resource names. Reads/close use
 * the real source callbacks; PNG decoding uses mc_image defaults. Only valid
 * 8-bit RGB/palette/RGBA (types 2/3/6) are admitted for this client increment.
 * Grayscale/16-bit ImageIO conversion and AWT missingtex remain unfinished and
 * fail explicitly; general PNG/ImageIO equivalence is not claimed. The decoded
 * cache has a separate 128 MiB RGBA ledger. No GL call or texture-pack prefix
 * interpretation occurs. Borrowed images live until assets free. */
mc_client_assets_status mc_client_assets_image(mc_client_assets *assets, const char *name,
                                               const mc_image **out, mc_client_assets_error *error);

/* First use initializes/inspects the actual shared Translation, queries the
 * UTF-16 property and borrows its real String view. Absent keys get an owned
 * copied key fallback. No alternate language loader. Keys are ASCII C strings,
 * including an empty key, bounded to MAX_NAME_BYTES. Cached views live until
 * assets free while the borrowed Registry remains live. */
mc_client_assets_status mc_client_assets_label(mc_client_assets *assets, const char *key,
                                               mc_font_text *out, mc_client_assets_error *error);

/* Lazy UTF-8 CoreReader load of /font.txt. Retain complete decoded lines with
 * LF separators and comments intact; FontRenderer performs its one fp parse.
 * Missing/open/read IOException returns the completed-line prefix, possibly
 * empty. No failed partial line is published. Native OOM/limits/unsupported
 * callbacks remain errors. Normal fp close IOException also retains the prefix.
 * Text is borrowed until assets free. */
mc_client_assets_status mc_client_assets_font_txt(mc_client_assets *assets, mc_font_text *out,
                                                  mc_client_assets_error *error);

/* The caller constructs/owns the initialized static GuiMainMenu Random before this
 * call and applies date overrides afterward. Read UTF-8, Java-trim <= U+0020,
 * preserve nonempty duplicates/order, and draw next_bounded only after complete
 * EOF. Missing/empty/source read failure returns owned "missingno" without a
 * draw. Native limits/OOM/cleanup failures are explicit. Close retires the stream
 * before selection; an allocation failure copying the selected text preserves
 * an already-consumed Random draw. Temporary reader/line/list storage is freed
 * on every path; stream contexts remain resource-source-owned after close. */
mc_client_assets_status mc_client_assets_choose_splash(mc_client_assets *assets,
                                                       mc_java_random *random,
                                                       mc_client_string *out,
                                                       mc_client_assets_error *error);
void mc_client_string_free(mc_client_string *string); /* NULL is harmless; resets the view. */

/* Persistent cache metadata/text and each temporary reader arena have separate
 * 8 MiB bounds, including arena headers. Text resources have the 16 MiB byte
 * bound. Native allocation bounds do not reproduce Java OOME timing. All out
 * values/error are mutually disjoint, disjoint from owners/dependencies and
 * unchanged on failure (except error); callers
 * release any previous owned result before replacement. error is optional and
 * receives the reached primary cause plus a separate cleanup close status. */

#endif
