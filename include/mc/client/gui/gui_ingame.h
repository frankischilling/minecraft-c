#ifndef MC_CLIENT_GUI_GUI_INGAME_H
#define MC_CLIENT_GUI_GUI_INGAME_H

#include "mc/client/render/model.h"
#include "mc/core/java/random_profile.h"

typedef struct client_application client_application;
typedef struct mc_player_string mc_player_string;
typedef struct mc_client_source_uq mc_client_source_uq;
typedef struct mc_client_gui_ingame_classes mc_client_gui_ingame_classes;
typedef struct mc_client_gui_ingame_list mc_client_gui_ingame_list;

typedef enum mc_client_gui_ingame_status {
    MC_CLIENT_GUI_INGAME_OK,
    MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT,
    MC_CLIENT_GUI_INGAME_INVALID_PHASE,
    MC_CLIENT_GUI_INGAME_BUSY,
    MC_CLIENT_GUI_INGAME_LIMIT,
    MC_CLIENT_GUI_INGAME_ALLOCATION_FAILED,
    MC_CLIENT_GUI_INGAME_MODEL_FAILED,
    MC_CLIENT_GUI_INGAME_RANDOM_FAILED,
    MC_CLIENT_GUI_INGAME_CLASS_ERRONEOUS,
    MC_CLIENT_GUI_INGAME_THREAD_FAILED,
    MC_CLIENT_GUI_INGAME_SOURCE_NULL_REFERENCE,
    MC_CLIENT_GUI_INGAME_UNIMPLEMENTED
} mc_client_gui_ingame_status;

typedef enum mc_client_gui_ingame_class_phase {
    MC_CLIENT_GUI_INGAME_CLASS_COLD,
    MC_CLIENT_GUI_INGAME_CLASS_INITIALIZING,
    MC_CLIENT_GUI_INGAME_CLASS_READY,
    MC_CLIENT_GUI_INGAME_CLASS_FAILED
} mc_client_gui_ingame_class_phase;

typedef enum mc_client_gui_ingame_phase {
    MC_CLIENT_GUI_INGAME_RAW,
    MC_CLIENT_GUI_INGAME_CONSTRUCTING,
    MC_CLIENT_GUI_INGAME_READY,
    MC_CLIENT_GUI_INGAME_FAILED,
    MC_CLIENT_GUI_INGAME_RETIRING
} mc_client_gui_ingame_phase;

typedef struct mc_client_gui_ingame_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_client_gui_ingame_site;

typedef struct mc_client_gui_ingame_cause {
    mc_client_gui_ingame_status status;
    const char *operation;
    mc_client_gui_ingame_site site;
    bool native_policy;
    mc_client_model_error model;
    mc_java_random_profile_status random;
    int native_thread_error;
} mc_client_gui_ingame_cause;

typedef struct mc_client_gui_ingame_error {
    mc_client_gui_ingame_status status;
    const char *operation;
    mc_client_gui_ingame_site site;
    bool native_policy, source_returned;
    mc_client_model_error model;
    mc_java_random_profile_status random;
    int native_thread_error;
    mc_client_gui_ingame_cause first_class_failure;
} mc_client_gui_ingame_error;

/* The ten actual uq instance fields plus inherited ub.k. */
typedef struct mc_client_gui_ingame_fields {
    float k; /* ub.k; the actual superclass constructor stores +0.0 at6. */
    mc_client_gui_ingame_list *e;
    mc_java_random *f;
    client_application *g;
    const mc_player_string *a;
    int32_t h;
    const mc_player_string *i;
    int32_t j;
    bool l;
    float b, c;
} mc_client_gui_ingame_fields;

typedef struct mc_client_gui_ingame_options {
    /* Independent native instance budget, including the owner. Zero means
     * literal zero. Source allocation quota counts only e/new Random, not
     * class activation or native monitor/owner setup. NULL selects1MiB/unlimited. */
    size_t maximum_bytes, maximum_allocations;
} mc_client_gui_ingame_options;

typedef struct mc_client_gui_ingame_class_state {
    mc_client_gui_ingame_class_phase phase;
    size_t holds;
    bool ub_initialized, retiring, static_item_live;
    unsigned int last_pc, last_store_pc;
    const mc_client_item_renderer *d, *pending_item;
    mc_client_gui_ingame_error failure;
} mc_client_gui_ingame_class_state;

typedef struct mc_client_gui_ingame_state {
    mc_client_gui_ingame_phase phase;
    size_t holds, allocation_bytes, allocations, allocation_attempts;
    mc_client_gui_ingame_fields fields;
    const mc_client_gui_ingame_classes *classes;
    const mc_java_random *pending_random;
    unsigned int last_pc, store_count;
    mc_client_gui_ingame_site stores[10];
    mc_client_gui_ingame_error failure;
} mc_client_gui_ingame_state;

/* One counted namespace per actual uq class. Creation is cold and retains the
 * canonical model classes, borrowing their SAME live Random profile. It creates
 * no ItemRenderer/Random. First active use initializes ub (no <clinit>) then
 * constructs a DISTINCT real bb0/4 and publishes d7 only after return. It never
 * aliases the achievement-notice or RenderManager renderer. static_item_bytes
 * is passed to the canonical ItemRenderer allocator; zero retains its1MiB default.
 * First failed activation keeps the pending child and complete immutable cause;
 * later active uses return CLASS_ERRONEOUS without replaying or consuming Random.
 * Retire all instances/borrowed views before the final class occurrence. Native
 * final cleanup invalidates static child views when consumed; inspection then
 * reports static_item_live=false/d=NULL without making a source putstatic. */
mc_client_gui_ingame_status mc_client_gui_ingame_classes_create(mc_client_render_classes *,
                                                                mc_java_random_profile *,
                                                                size_t static_item_bytes,
                                                                mc_client_gui_ingame_classes **,
                                                                mc_client_gui_ingame_error *);
mc_client_gui_ingame_status mc_client_gui_ingame_classes_active_use(mc_client_gui_ingame_classes *,
                                                                    mc_client_gui_ingame_error *);
mc_client_gui_ingame_status mc_client_gui_ingame_classes_retain(mc_client_gui_ingame_classes *,
                                                                mc_client_gui_ingame_error *);
mc_client_gui_ingame_status mc_client_gui_ingame_classes_release(mc_client_gui_ingame_classes **,
                                                                 mc_client_gui_ingame_error *);
mc_client_gui_ingame_status mc_client_gui_ingame_classes_inspect(mc_client_gui_ingame_classes *,
                                                                 mc_client_gui_ingame_class_state *,
                                                                 mc_client_gui_ingame_error *);

/* Allocate performs real new/active use before RAW instance allocation. Output
 * stays unchanged on failure. Construct performs ub1/k6 then the complete uq
 * constructor. Failed List/Random prefixes remain owned; construction is one-shot.
 * NULL and actual RAW App fixtures are supported. A nonnull RETIRING/retired App
 * is rejected as native policy at the reached g59 publication, without atomic
 * reads, dummy App fields or a fabricated source failure.
 *
 * g is a stable BORROW, not self-owning. The caller must keep that actual App
 * allocated through the entire instance lifetime and every exported g view,
 * until strict instance retirement consumes its slot. The future startup owner
 * must retire GuiIngame before App/resources and preserve them on BUSY. No App
 * hold, source field publication, registered counter or ownership cycle is added.
 *
 * Operations require exclusive serialized use of the shared model/profile graph;
 * this namespace's native monitor also rejects contention/reentry as BUSY.
 * Inspection is passive. Views expire on owner mutation/retirement, and no source
 * method beyond construction, b()V clear_messages and the reached a()V tick
 * is implemented here. */
mc_client_gui_ingame_status mc_client_gui_ingame_allocate(mc_client_gui_ingame_classes *,
                                                          const mc_client_gui_ingame_options *,
                                                          mc_client_source_uq **,
                                                          mc_client_gui_ingame_error *);
mc_client_gui_ingame_status mc_client_gui_ingame_construct(mc_client_source_uq *,
                                                           client_application *nullable_app,
                                                           mc_client_gui_ingame_error *);
mc_client_gui_ingame_status mc_client_gui_ingame_retain(mc_client_source_uq *,
                                                        mc_client_gui_ingame_error *);
/* Ordinary nonfinal release consumes exactly its caller occurrence. Strict
 * retire instead refuses extra instance holds BEFORE consuming or revoking it.
 * Final cleanup failures retain the same slot/prefix for retry. No source field
 * is reset; successful consumption invalidates the whole instance/view. */
mc_client_gui_ingame_status mc_client_gui_ingame_release(mc_client_source_uq **,
                                                         mc_client_gui_ingame_error *);
mc_client_gui_ingame_status mc_client_gui_ingame_retire(mc_client_source_uq **,
                                                        mc_client_gui_ingame_error *);
mc_client_gui_ingame_status mc_client_gui_ingame_inspect(mc_client_source_uq *,
                                                         mc_client_gui_ingame_state *,
                                                         mc_client_gui_ingame_error *);
/* Actual uq.b()V: load SAME e1, List.clear4, return9. RAW and failed native
 * instances may reach their real null e fault4; constructing/retiring/class
 * contention are separate native admissions. Clear increments the selected
 * JDK modCount with Java wrap, captures array/old size, stores size0 before
 * nulling occupied slots. It retains List/array identity and capacity and
 * never source-closes/frees referents. Constructor phase/diagnostics/stores,
 * Random/App/Font and GL are untouched. A borrowed array view must stay live
 * and exclusive; native terminal retirement, separately, frees its storage. */
mc_client_gui_ingame_status mc_client_gui_ingame_clear_messages(mc_client_source_uq *,
                                                                mc_client_gui_ingame_error *);
/* Actual uq.a()V: decrement positive j at14, increment h with Java wrap at24,
 * then inspect the current e List at34. The ordinary empty list returns70.
 * A null List faults at34 after those prior stores; a null entry faults at56.
 * Nonnull message entries stop at the unreached canonical sw owner/type
 * dependency52, preserving earlier source stores and the List. No arbitrary
 * native void pointer is dereferenced as a fabricated chat-line instance.
 * Constructor phase/history and Random/App/Font/GL state remain unchanged. */
mc_client_gui_ingame_status mc_client_gui_ingame_tick(mc_client_source_uq *,
                                                      mc_client_gui_ingame_error *);
bool mc_client_gui_ingame_list_size(const mc_client_gui_ingame_list *, size_t *);

#endif
