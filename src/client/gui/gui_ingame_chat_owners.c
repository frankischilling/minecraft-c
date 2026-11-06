#include "gui_ingame_chat_internal.h"
#include "mc/block/registry/runtime.h"
#include <stdlib.h>

bool mc_gui_chat_text_read(mc_gui_chat_text text, mc_player_string_view *out) {
    if (text.registry != NULL && text.player == NULL && text.registry_owner != NULL) {
        mc_registry_string_view view;
        if (!mc_registry_string_read(text.registry, &view))
            return false;
        *out = (mc_player_string_view){view.units, view.length};
        return true;
    }
    return text.registry == NULL && text.registry_owner == NULL &&
           mc_player_string_read(text.player, out);
}

mc_gui_chat_message *mc_gui_chat_message_find(const mc_client_source_uq *owner, const void *value) {
    for (mc_gui_chat_message *m = owner->messages; m != NULL; m = m->next)
        if (m == value)
            return m;
    return NULL;
}

bool mc_gui_chat_message_release(mc_gui_chat_message *m, mc_registry_error *error) {
    if (m->registry_held) {
        if (mc_registry_classes_release(m->a.registry_owner, error) != MC_REGISTRY_OK)
            return false;
        m->registry_held = false;
    }
    mc_player_string_release((mc_player_string *)m->a.player);
    free(m);
    return true;
}

static bool collect(mc_client_source_uq *owner, bool terminal, mc_client_gui_ingame_error *error) {
    mc_gui_chat_message **slot = &owner->messages;
    while (*slot != NULL) {
        mc_gui_chat_message *m = *slot;
        bool reachable = false;
        if (!terminal && owner->fields.e != NULL)
            for (size_t i = 0; i < owner->fields.e->size; ++i)
                if (owner->fields.e->entries[i] == m) {
                    reachable = true;
                    break;
                }
        if (reachable) {
            slot = &m->next;
            continue;
        }
        mc_gui_chat_message *next = m->next;
        mc_registry_error registry;
        if (!mc_gui_chat_message_release(m, &registry)) {
            error->status = MC_CLIENT_GUI_INGAME_BUSY;
            error->operation = "GuiIngame native chat String namespace retirement";
            error->native_policy = true;
            error->registry = registry;
            return false;
        }
        *slot = next;
        owner->bytes -= sizeof(*m);
    }
    return true;
}

bool mc_gui_chat_terminal(mc_client_source_uq *owner, mc_client_gui_ingame_error *error) {
    return collect(owner, true, error);
}

bool mc_gui_chat_collect(mc_client_source_uq *owner, mc_client_gui_ingame_error *error) {
    return collect(owner, false, error);
}
