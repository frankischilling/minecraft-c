#include "controller_boundary_observer.h"

/* The public SDK declaration is already included above. Rename only this
 * exact production definition; every other Controller symbol is provided by
 * this same included TU. A future link-map admission must exclude the archive's
 * separate sdk_controllers.c object, and retain the real native provider. */
mc_client_sdk_input_status mc_test_controller_boundary_real_create(mc_client_sdk_input *,
                                                                   mc_client_sdk_input_error *);
#define mc_client_sdk_controllers_create mc_test_controller_boundary_real_create
#include "../../../src/client/display/sdk_controllers.c"
#undef mc_client_sdk_controllers_create

#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_video.h>
#include <stdio.h>
#include <string.h>

typedef void(GLAPIENTRY *mc_test_gl_enable_fn)(GLenum);
static mc_test_controller_boundary_state observation;

bool mc_test_controller_boundary_arm(void) {
    if (observation.armed)
        return false;
    observation = (mc_test_controller_boundary_state){.armed = true};
    return true;
}

bool mc_test_controller_boundary_inspect(mc_test_controller_boundary_state *out) {
    if (out == NULL)
        return false;
    *out = observation;
    return true;
}

void mc_test_controller_boundary_disarm(void) {
    observation.armed = false;
    observation.input = NULL;
    observation.display = NULL;
}

mc_client_sdk_input_status mc_client_sdk_controllers_create(mc_client_sdk_input *input,
                                                            mc_client_sdk_input_error *error) {
    const mc_client_sdk_input_status result = mc_test_controller_boundary_real_create(input, error);
    if (observation.armed) {
        ++observation.create_calls;
        observation.create_status = result;
        observation.input = input;
        observation.display = input == NULL ? NULL : input->display;
        if (result == MC_CLIENT_SDK_INPUT_OK && observation.create_calls == 1) {
            observation.injection_attempted = true;
            const mc_test_gl_enable_fn enable =
                (mc_test_gl_enable_fn)SDL_GL_GetProcAddress("glEnable");
            if (enable == NULL) {
                observation.missing_gl_function = true;
                (void)snprintf(observation.lookup_error, sizeof(observation.lookup_error), "%s",
                               SDL_GetError());
            } else {
                enable(UINT32_MAX); /* Real invalid enum on the genuine current context. */
                ++observation.invalid_enum_calls;
                observation.injected = true;
            }
        }
    }
    /* Injection failure never changes discovery, native causes, or Source
     * created publication. It remains a separate control failure. */
    return result;
}
