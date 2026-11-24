#include "sdk_display_internal.h"
#include "sdk_input_internal.h"

static mc_client_sdk_input_status source_create(mc_client_sdk_input *owner, bool mouse,
                                                mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error error = {0};
    const char *name = mouse ? "org/lwjgl/input/Mouse" : "org/lwjgl/input/Keyboard";
    if (!mc_client_sdk_input_enter(owner, name, "create", "()V", &error)) {
        if (out != NULL)
            *out = error;
        return error.status;
    }
    if (!mc_client_display_sdk_created(owner->display)) {
        mc_client_sdk_input_fail(&error, MC_CLIENT_SDK_INPUT_SOURCE_ILLEGAL_STATE,
                                 "actual create Display.isCreated guard", name, "create", "()V", 21,
                                 false);
    } else if (mc_client_display_inputs(owner->display) != owner) {
        /* The native composition supplies the actual implementation returned by
         * OpenGLPackageAccess.createImplementation22, not another input cache.
         * The temporary privileged action has no escaping state in this body. */
        mc_client_sdk_input_fail(&error, MC_CLIENT_SDK_INPUT_INVALID_PHASE,
                                 "create actual shared InputImplementation identity", name,
                                 "create", "()V", 22, true);
    } else if (!(mouse ? owner->mouse_created : owner->keyboard_created)) {
        mc_client_sdk_input_fail(&error, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                                 "cold SDK device recreation is outside the completed profile",
                                 name, "create", "(Lorg/lwjgl/opengl/InputImplementation;)V", 7,
                                 true);
        mc_client_sdk_input_caller(&error, name, "create", "()V", 25);
    } else {
        /* The real overload's first created read returns at6. In particular,
         * this call does not repeat physical attachment or source stores. */
        error.site = (mc_client_sdk_input_site){name, "create", "()V", 38};
        error.child = (mc_client_sdk_input_site){name, "create",
                                                 "(Lorg/lwjgl/opengl/InputImplementation;)V", 6};
    }
    return mc_client_sdk_input_leave(owner, &error, out);
}

mc_client_sdk_input_status mc_client_sdk_keyboard_create(mc_client_sdk_input *owner,
                                                         mc_client_sdk_input_error *error) {
    return source_create(owner, false, error);
}

mc_client_sdk_input_status mc_client_sdk_mouse_create(mc_client_sdk_input *owner,
                                                      mc_client_sdk_input_error *error) {
    return source_create(owner, true, error);
}
