#ifndef MC_CLIENT_RUNTIME_BOOTSTRAP_TERMINAL_INTERNAL_H
#define MC_CLIENT_RUNTIME_BOOTSTRAP_TERMINAL_INTERNAL_H

#include "bootstrap.h"

typedef struct mc_client_menu_loop mc_client_menu_loop;
typedef struct mc_client_bootstrap_terminal_scope mc_client_bootstrap_terminal_scope;
typedef struct mc_client_frame_state mc_client_frame_state;

/* Private neutral bridge: Screen adapters need no runtime linker dependency.
 * Only the actual bootstrap provider initializes this embedded token. Its
 * callback freshly proves the token's own address and complete live creator
 * association; the token is never a caller-supplied force flag. */
struct mc_client_bootstrap_terminal_scope {
    mc_client_bootstrap *owner;
    const mc_client_menu_loop *menu;
    bool (*validate)(const mc_client_bootstrap_terminal_scope *, const client_application *);
};

/* Native creator association. One loop borrows this bootstrap until its final
 * checked retirement. Failed setup never publishes an association. */
bool mc_client_bootstrap_bind_menu(mc_client_bootstrap *, mc_client_menu_loop *);
void mc_client_bootstrap_unbind_menu(mc_client_bootstrap *, const mc_client_menu_loop *);

/* End bootstrap getter admission for this exact inactive, selected menu. The
 * opaque borrowed scope belongs to the live bootstrap; it is neither a Source
 * close nor a World admission permit. A legacy close cannot mint this scope.
 * All reached menu calls must already be retired by the caller. */
const mc_client_bootstrap_terminal_scope *
mc_client_bootstrap_terminal_begin(mc_client_bootstrap *, const mc_client_menu_loop *);

static inline bool
mc_client_bootstrap_terminal_scope_matches(const mc_client_bootstrap_terminal_scope *scope,
                                           const client_application *app) {
    return scope != NULL && scope->validate != NULL && scope->validate(scope, app);
}

/* Passive terminal-root inspection while the actual Frame/App/startup owners
 * remain live and mutually associated. The menu may already have ended; never
 * revisit its expired scope pointer here. This does not reopen getters, grant
 * a Frame owner/retain or admit Source entry. Snapshot pointer members borrow
 * only those still-owned dependencies and expire when their owners end. The
 * caller keeps the bootstrap exclusive/live and output disjoint. Failure
 * preserves *out, including after startup/App association consumption. */
bool mc_client_bootstrap_terminal_frame_inspect(const mc_client_bootstrap *,
                                                mc_client_frame_state *out);

/* Implemented by the menu owner; passive check of the actual creator, App and
 * one-way inactive terminal selection, never an inspector-supplied boolean. */
bool mc_client_menu_loop_terminal_scope_matches(const mc_client_menu_loop *,
                                                const mc_client_bootstrap *,
                                                const client_application *);

#endif
