#ifndef MC_CLIENT_GUI_ACHIEVEMENT_NOTICE_H
#define MC_CLIENT_GUI_ACHIEVEMENT_NOTICE_H
#include "mc/client/render/model.h"
#include "mc/statistics/achievements/achievements.h"

typedef struct client_application client_application;
typedef struct mc_achievement_description_callback mc_achievement_description_callback;
typedef struct mc_client_achievement_notice mc_client_achievement_notice;
typedef struct mc_client_description_owner mc_client_description_owner;

/* Stored l is borrowed from Registry. Formatted kh text has a distinct runtime
 * owner; transfer this noncopyable owning result or free it after its final native
 * use. Consume/free a previous result before successful replacement. Pattern/
 * Registry owners remain live throughout calls; a borrowed l additionally needs
 * Registry lifetime. Runtime text admits 1Mi UTF16 units and an 8MiB per-call
 * allocation ledger. Native bounds are distinct from Java OOME timing. */
typedef struct mc_client_description_result {
    const mc_registry_string *string;
    mc_client_description_owner *owner;
} mc_client_description_result;

typedef enum mc_client_notice_status {
    MC_CLIENT_NOTICE_OK,
    MC_CLIENT_NOTICE_INVALID_ARGUMENT,
    MC_CLIENT_NOTICE_INVALID_PHASE,
    MC_CLIENT_NOTICE_BUSY,
    MC_CLIENT_NOTICE_ALLOCATION_FAILED,
    MC_CLIENT_NOTICE_REGISTRY_FAILED,
    MC_CLIENT_NOTICE_MODEL_FAILED,
    MC_CLIENT_NOTICE_SOURCE_EXCEPTION,
    MC_CLIENT_NOTICE_CLOCK_FAILED,
    MC_CLIENT_NOTICE_UNIMPLEMENTED_DEPENDENCY
} mc_client_notice_status;

typedef struct mc_client_notice_error {
    mc_client_notice_status status;
    const char *source_class;
    int32_t source_pc, notice_pc;
    mc_registry_error registry;
    mc_client_model_error model;
    mc_java_format_error format;
} mc_client_notice_error;

typedef struct mc_client_achievement_notice_fields {
    float k; /* ub.k */
    client_application *a;
    int32_t b, c;
    const mc_registry_string *d, *e;
    const mc_statistic *f;
    int64_t g;
    mc_client_item_renderer *h;
    bool i;
} mc_client_achievement_notice_fields;

/* Concrete kh objects are Registry-arena owned. Client is the actual application
 * identity (possibly NULL), borrowed until that Registry is retired. It must
 * outlive every dispatch. No callable/generic callback is accepted. Assignment
 * does no initialization; NULL callback restores the actual ny.e stored l path. */
mc_registry_status mc_client_inventory_callback_create(mc_registry_classes *, client_application *,
                                                       const mc_achievement_description_callback **,
                                                       mc_registry_error *);
mc_registry_status mc_client_achievement_callback_set(mc_registry_classes *, const mc_statistic *,
                                                      const mc_achievement_description_callback *,
                                                      mc_registry_error *);
mc_client_notice_status mc_client_achievement_description(mc_registry_classes *,
                                                          const mc_statistic *,
                                                          mc_client_description_result *,
                                                          mc_client_notice_error *);
void mc_client_description_result_free(mc_client_description_result *);

/* Native Registry retention uses the same explicit profile as its owner.
 * Allocation alone publishes zeroed RAW storage; construct runs genuine ub,
 * bb/bw/fh/ps/ib/tz/cv and the ordinary default Random before publishing h.
 * Registry/Entities/profile/resources remain exclusive and live. Failed source
 * constructor prefixes cannot be retried; free retires both published and
 * unpublished native children. Outputs/errors must be disjoint from owners. */
mc_client_notice_status mc_client_achievement_notice_allocate(mc_client_render_classes *,
                                                              mc_registry_classes *,
                                                              mc_java_random_profile *,
                                                              mc_client_achievement_notice **,
                                                              mc_client_notice_error *);
mc_client_notice_status mc_client_achievement_notice_construct(mc_client_achievement_notice *,
                                                               client_application *,
                                                               mc_client_notice_error *);
const mc_client_achievement_notice_fields *
mc_client_achievement_notice_fields_get(const mc_client_achievement_notice *);
mc_client_model_phase mc_client_achievement_notice_phase(const mc_client_achievement_notice *);
/* Completion and tutorial remain distinct operations. Every title/description,
 * clock and later-field store follows source order; failure retains the prefix.
 * Borrowed dynamic description views survive until the next successful e store
 * or retirement; a failed description call keeps the previous owner alive.
 * Nullable achievement reaches its source dereference after preceding work. */
mc_client_notice_status mc_client_achievement_notice_complete(mc_client_achievement_notice *,
                                                              const mc_statistic *,
                                                              mc_client_notice_error *);
mc_client_notice_status mc_client_achievement_notice_remind(mc_client_achievement_notice *,
                                                            const mc_statistic *,
                                                            mc_client_notice_error *);
mc_client_notice_status mc_client_achievement_notice_free(mc_client_achievement_notice **,
                                                          mc_client_notice_error *);
/* Reached vu.a() gate reads actual H from the constructed registered App's
 * live bound class namespace. With H<=0, null f or zero g returns107 without
 * clock/GL/Font work. Unbound/null/RAW App namespaces remain an explicit native
 * dependency at0; no missing namespace is inferred as H=0. H>0 retains the
 * watermark GL dependency11, and active notices retain clock/drawing108.
 * App/class/Registry/model owners stay live and exclusively borrowed. */
mc_client_notice_status mc_client_achievement_notice_draw(mc_client_achievement_notice *,
                                                          mc_client_notice_error *);
#endif
