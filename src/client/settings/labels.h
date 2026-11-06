#ifndef MC_CLIENT_GAME_SETTINGS_LABELS_H
#define MC_CLIENT_GAME_SETTINGS_LABELS_H

#include "assets.h"
#include "mc/client/settings/game_option.h"

#define MC_GAME_SETTINGS_LABEL_MAX_UNITS (2u * MC_CLIENT_ASSETS_MAX_TEXT_UNITS + 16u)

typedef enum mc_game_settings_label_status {
    MC_GAME_SETTINGS_LABEL_OK,
    MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT,
    MC_GAME_SETTINGS_LABEL_ALLOCATION_FAILED,
    MC_GAME_SETTINGS_LABEL_LIMIT,
    MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION,
    MC_GAME_SETTINGS_LABEL_ASSETS_FAILED
} mc_game_settings_label_status;

typedef enum mc_game_settings_label_failure {
    MC_GAME_SETTINGS_LABEL_SOURCE_NONE,
    MC_GAME_SETTINGS_LABEL_NULL_OPTION,
    MC_GAME_SETTINGS_LABEL_NULL_BINDING_ARRAY,
    MC_GAME_SETTINGS_LABEL_BINDING_INDEX,
    MC_GAME_SETTINGS_LABEL_NULL_BINDING,
    MC_GAME_SETTINGS_LABEL_NULL_NAME,
    MC_GAME_SETTINGS_LABEL_KEY_INDEX,
    MC_GAME_SETTINGS_LABEL_VALUE_INDEX
} mc_game_settings_label_failure;

typedef struct mc_game_settings_label_error {
    mc_game_settings_label_status status;
    mc_game_settings_label_failure source_failure;
    /* Original kv instruction and, when applicable, nh/Keyboard instruction.
     * -1 means no source instruction is attributed to a native argument error.
     * Assets retains its actual deeper Registry/resource diagnostic separately. */
    int32_t settings_pc, dependency_pc;
    mc_client_assets_error assets;
} mc_game_settings_label_error;

/* Owned counted UTF16 result. is_null distinguishes a valid Keyboard table hole
 * from an empty String; embedded NUL and lone surrogates are unchanged. The
 * caller consumes/frees its previous result before replacing it. Outputs/error
 * are disjoint from each other and from live owners, and failure preserves out.
 * The result is independent of later Settings/Assets retirement. String-object
 * identity and Java allocation/OOME timing are not represented by this owner. */
typedef struct mc_game_settings_label {
    uint16_t *units;
    size_t length;
    bool is_null;
} mc_game_settings_label;

/* Source kv.a(I) and c(ht): actual singleton admission at entry precedes source
 * null/index failures. The real shared Assets/Registry performs all lookups;
 * performance alone reaches the distinct do wrapper. No settings mutations,
 * save, graphics/audio/key callbacks, or text normalization. Calls require
 * exclusive live owners and their resource-source lifetime contracts. */
mc_game_settings_label_status mc_game_settings_binding_label(const mc_game_settings *settings,
                                                             mc_client_assets *assets,
                                                             int32_t index,
                                                             mc_game_settings_label *out,
                                                             mc_game_settings_label_error *error);
mc_game_settings_label_status mc_game_settings_option_label(const mc_game_settings *settings,
                                                            mc_client_assets *assets,
                                                            const mc_game_option *option,
                                                            mc_game_settings_label *out,
                                                            mc_game_settings_label_error *error);

/* Source kv.b(I), with no translation initialization. Only the completed normal
 * retained LWJGL key table is represented; valid holes succeed with is_null.
 * No native keyboard creation or platform event mapping is performed. */
mc_game_settings_label_status mc_game_settings_key_label(const mc_game_settings *settings,
                                                         int32_t index, mc_game_settings_label *out,
                                                         mc_game_settings_label_error *error);
void mc_game_settings_label_free(mc_game_settings_label *label);

#endif
