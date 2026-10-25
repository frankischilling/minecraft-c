#ifndef MC_ENTITY_PLAYER_PLAYER_H
#define MC_ENTITY_PLAYER_PLAYER_H

#include "mc/entity/entity.h"
#include "mc/inventory/container.h"
#include "mc/world/info/info.h"

typedef struct mc_player_string mc_player_string;
typedef struct mc_player_session_classes mc_player_session_classes;
typedef struct mc_player_session mc_player_session;
struct mc_movement_input;

#define MC_PLAYER_MAX_TEXT_UNITS 1048576u

typedef enum mc_player_status {
    MC_PLAYER_OK,
    MC_PLAYER_INVALID_ARGUMENT,
    MC_PLAYER_INVALID_PHASE,
    MC_PLAYER_LIMIT,
    MC_PLAYER_ALLOCATION_FAILED,
    MC_PLAYER_ENTITY_FAILED,
    MC_PLAYER_PROFILE_FAILED,
    MC_PLAYER_WORLD_FAILED,
    MC_PLAYER_INVENTORY_FAILED,
    MC_PLAYER_REGISTRY_FAILED,
    MC_PLAYER_NULL_REFERENCE,
    MC_PLAYER_BUSY
} mc_player_status;

typedef struct mc_player_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_player_site;

typedef struct mc_player_error {
    mc_player_status status;
    const char *operation;
    mc_player_site site;
    mc_entity_error entity;
    mc_inventory_error inventory;
    mc_registry_error registry;
    mc_world_storage_error world;
    mc_world_info_status info;
} mc_player_error;

typedef struct mc_player_string_view {
    const uint16_t *units;
    size_t length;
} mc_player_string_view;

/* Explicit immutable native String identities. NULL is a source null String;
 * nonnull empty and equal-text distinct owners stay distinct. No encoding,
 * normalization, NUL/surrogate restriction, interning or network operation.
 * Each fixed owner is bounded to MAX_TEXT_UNITS; this is a native resource
 * policy separate from Entity/Inventory budgets and JVM allocation timing. */
mc_player_status mc_player_string_create(const uint16_t *units, size_t length,
                                         mc_player_string **out);
mc_player_status mc_player_string_retain(mc_player_string *string);
void mc_player_string_release(mc_player_string *string);
bool mc_player_string_read(const mc_player_string *string, mc_player_string_view *out);

typedef enum mc_player_session_phase {
    MC_PLAYER_SESSION_COLD,
    MC_PLAYER_SESSION_INITIALIZING,
    MC_PLAYER_SESSION_READY,
    MC_PLAYER_SESSION_FAILED
} mc_player_session_phase;

typedef struct mc_player_session_class_state {
    mc_player_session_phase phase;
    unsigned int pc;
    const void *creative_list;
    const mc_block *const *creative_blocks;
    size_t creative_count, live_sessions;
} mc_player_session_class_state;

typedef struct mc_player_session_state {
    const mc_player_string *username, *session_id, *password;
} mc_player_session_state;

/* Real client gr class/instance closure. Class creation retains the exact
 * client Entity/Registry/profile pair without active use. First session create
 * executes gr.<clinit>, publishing its actual List and appending the actual
 * 28 Block static identities in order. Failure latches the reached List/prefix.
 * No invented empty class, Creative list or Block table is substituted.
 * Session body stores/retains the same supplied nullable String identities;
 * password stays null. Strings may outlive the Session through other holds.
 * Retire Sessions, then this class owner, before the Registry/Entity owners. */
mc_player_status mc_player_session_classes_create(mc_entity_classes *entities,
                                                  mc_registry_classes *registry,
                                                  mc_player_session_classes **out,
                                                  mc_player_error *error);
mc_player_status mc_player_session_classes_free(mc_player_session_classes **classes,
                                                mc_player_error *error);
bool mc_player_session_classes_read(const mc_player_session_classes *classes,
                                    mc_player_session_class_state *out);
mc_player_status mc_player_session_create(mc_player_session_classes *classes,
                                          mc_player_string *username, mc_player_string *session_id,
                                          mc_player_session **out, mc_player_error *error);
void mc_player_session_retire(mc_player_session **session);
bool mc_player_session_read(const mc_player_session *session, mc_player_session_state *out);

/* All declaring-class ls instance fields; raw defaults remain represented.
 * String fields borrow immutable identities for the lifetime of the Player. */
typedef struct mc_living_entity_fields {
    int32_t E;
    float F, G, H, I, J, K, L, M;
    bool N;
    const mc_player_string *O;
    bool P;
    float Q;
    const mc_player_string *R;
    float S;
    int32_t T;
    float U;
    bool V;
    float W, X;
    int32_t Y, Z, private_a, aa, ab;
    float ac;
    int32_t ad, ae;
    float af, ag;
    bool ah;
    int32_t ai;
    float aj, ak, al, am;
    int32_t an;
    double ao, ap, aq, ar, as;
    float at;
    int32_t au, av;
    float aw, ax, ay;
    bool az;
    float aA, aB;
    mc_entity *private_b;
    int32_t aC;
} mc_living_entity_fields;

typedef struct mc_player_coordinates {
    int32_t x, y, z;
} mc_player_coordinates;

typedef struct mc_player_fields {
    mc_inventory_player *c;
    mc_container *d, *e;
    int8_t f;
    int32_t g;
    float h, i;
    bool j;
    int32_t k;
    const mc_player_string *l;
    int32_t m;
    const mc_player_string *n;
    double o, p, q, r, s, t;
    bool u;
    const mc_player_coordinates *v;
    int32_t private_a;
    float w, x, y;
    const mc_player_coordinates *private_b, *private_bN;
    int32_t z;
    bool A;
    float B, C;
    int32_t private_bO;
    mc_entity *D;
} mc_player_fields;

typedef struct mc_player_mouse_filter_fields {
    float a, b, c;
} mc_player_mouse_filter_fields;

typedef struct mc_local_player_snapshot {
    mc_living_entity_fields living;
    mc_player_fields player;
    /* Borrowed view of actual dc.a; constructor leaves NULL. An attachment
     * owns one native input hold. Retain before independent use/retirement. */
    struct mc_movement_input *input;
    void *minecraft; /* dc.b, borrowed actual client; NULL supported. */
    const mc_player_mouse_filter_fields *filters[3]; /* dc.bN/bO/bP, distinct objects. */
    const mc_player_string *skin_url;                /* same identity as base.client_only.bA */
    unsigned int living_pc, player_pc, local_pc, watcher_hook_pc;
} mc_local_player_snapshot;

/* Allocate a genuine RAW concrete dc owner with dc->gs->ls->sn Class ancestry.
 * Existing literal values remain stable; dc is a distinct client-only literal.
 * Construction invokes the actual watcher16 hook DURING the base constructor,
 * before Living fields, consumes all four shared Math returns in source order,
 * creates the actual InventoryPlayer/45-Slot container, reads the actual World
 * spawn through its Info, positions the same bounds, constructs three filters,
 * stores client/dimension and retains username/builds the skin String.
 *
 * world may be NULL until gs PC68, preserving the failed real base/Living/
 * inventory prefix. NULL session fails at dc PC103 after completed fields and
 * filters. A nonnull session may contain a null/empty username. No fake Player,
 * World/client, empty recipe callback or implicit gameplay bootstrap is used.
 * Nonnull minecraft must be the actual live client identity, borrowed for the
 * Player lifetime. Session stays live only through this synchronous call;
 * the Player retains its username String, not the Session owner.
 *
 * Keep actual World/Registry/class/profile dependencies live and exclusive.
 * A World must use this exact Entity namespace and native application profile.
 * Its existing counted binding protects the World/Registry during Player life.
 * Inventory/container limits use the concrete module's independent budget.
 * Failures retain every published constructor field/child; no retry. Retire
 * via ordinary Entity release after ending all borrowed views/entered calls.
 * The snapshot's child pointers are borrowed; never retire its Inventory,
 * Container, filters or Strings independently of their owning Player.
 * Native RAW/failed holds and snapshots are supported; no tick/NBT/publication,
 * input attachment, spawn collision search, controller action or Player render
 * method is implied by reaching READY here. Runtime remains C17/JVM-free. */
mc_entity_status mc_local_player_allocate(mc_entity_classes *classes, mc_entity **out,
                                          mc_entity_error *error);
mc_player_status mc_local_player_construct(mc_entity *raw, mc_world_runtime *world, void *minecraft,
                                           const mc_player_session *session, int32_t dimension,
                                           const mc_inventory_limits *inventory_limits,
                                           mc_player_error *error);
mc_entity_status mc_local_player_get_snapshot(const mc_entity *entity,
                                              mc_local_player_snapshot *out);

/* Actual gs.N()Z: read only u on a real concrete LocalPlayer, including RAW
 * and failed prefixes. No health, World, watcher, callback or sleep transition.
 * Native admission holds the Entity; a null receiver reports source getfield1. */
mc_player_status mc_player_is_sleeping(mc_entity *entity, bool *out, mc_player_error *error);

#endif
