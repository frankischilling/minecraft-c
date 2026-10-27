#include "mc/resources/files/files.h"
#include "sdk_icons.h"

#include <stdio.h>

static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct close_context {
    const mc_registry_resources *real;
    mc_client_sdk_icons **slot;
    size_t callbacks, busy, real_closes;
    size_t fail_closes;
} close_context;

static mc_registry_status open_resource(void *opaque, const char *name,
                                        mc_java_properties_input *out) {
    close_context *context = opaque;
    return context->real->open(context->real->context, name, out);
}

static mc_registry_status close_resource(void *opaque, const mc_java_properties_input *input) {
    close_context *context = opaque;
    ++context->callbacks;
    mc_client_sdk_icons *actual = *context->slot;
    mc_client_sdk_icons_error error;
    if (actual != NULL &&
        mc_client_sdk_icons_release(context->slot, &error) == MC_CLIENT_SDK_ICONS_BUSY &&
        *context->slot == actual)
        ++context->busy;
    /* The genuine filesystem close happens on every admitted callback. The
     * one-shot return fault is a native controller case, not a Java exception
     * or a successful fabricated resource. The real provider is idempotent. */
    const mc_registry_status status = context->real->close(context->real->context, input);
    ++context->real_closes;
    if (status == MC_REGISTRY_OK && context->fail_closes != 0) {
        --context->fail_closes;
        return MC_REGISTRY_IO_FAILED;
    }
    return status;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_client_sdk_icons *icons = NULL, *held = NULL;
    mc_client_sdk_icons_error error;
    mc_client_sdk_icons_state state;
    close_context context = {0};
    CHECK(argc == 2);
    CHECK(mc_resource_files_create(argv[1], &files) == MC_REGISTRY_OK);
    context.real = mc_resource_files_source(files);
    context.slot = &icons;
    mc_registry_resources wrapped = {
        .context = &context, .open = open_resource, .close = close_resource};
    CHECK(mc_client_sdk_icons_allocate(&wrapped, &icons, &error) == MC_CLIENT_SDK_ICONS_OK);
    CHECK(mc_client_sdk_icons_construct(icons, &error) == MC_CLIENT_SDK_ICONS_OK);
    CHECK(context.callbacks == 2 && context.busy == 2 && context.real_closes == 2);
    CHECK(mc_client_sdk_icons_inspect(icons, &state) && state.phase == MC_CLIENT_SDK_ICONS_READY &&
          state.holds == 1);
    /* Authored RGBA bytes are independent input fixtures, not SDK answers. */
    CHECK(state.rgba16[0] == 17 && state.rgba16[1] == 53 && state.rgba16[2] == 101 &&
          state.rgba16[3] == 255);
    CHECK(state.rgba32[0] == 211 && state.rgba32[1] == 137 && state.rgba32[2] == 29 &&
          state.rgba32[3] == 127);
    CHECK(mc_client_sdk_icons_retain(icons, &error) == MC_CLIENT_SDK_ICONS_OK);
    held = icons;
    CHECK(mc_client_sdk_icons_release(&icons, &error) == MC_CLIENT_SDK_ICONS_OK && icons == NULL);
    CHECK(mc_client_sdk_icons_inspect(held, &state) && state.holds == 1);
    CHECK(mc_client_sdk_icons_release(&held, &error) == MC_CLIENT_SDK_ICONS_OK && held == NULL);
    CHECK(mc_client_sdk_icons_allocate(&wrapped, &icons, &error) == MC_CLIENT_SDK_ICONS_OK);
    context.fail_closes = 2;
    CHECK(mc_client_sdk_icons_construct(icons, &error) == MC_CLIENT_SDK_ICONS_RESOURCE_FAILED);
    CHECK(error.close == MC_REGISTRY_IO_FAILED && error.bytes_read == 1024 && error.stream_open);
    CHECK(mc_client_sdk_icons_inspect(icons, &state) && state.phase == MC_CLIENT_SDK_ICONS_FAILED &&
          state.stream_open);
    mc_client_sdk_icons *same = icons;
    CHECK(mc_client_sdk_icons_release(&icons, &error) == MC_CLIENT_SDK_ICONS_RESOURCE_FAILED &&
          icons == same);
    CHECK(mc_client_sdk_icons_inspect(icons, &state) &&
          state.phase == MC_CLIENT_SDK_ICONS_RETIRING && state.holds == 1);
    CHECK(mc_client_sdk_icons_retain(icons, &error) == MC_CLIENT_SDK_ICONS_INVALID_PHASE);
    CHECK(mc_client_sdk_icons_inspect(icons, &state) && state.holds == 1);
    CHECK(mc_client_sdk_icons_release(&icons, &error) == MC_CLIENT_SDK_ICONS_OK && icons == NULL);
    CHECK(context.callbacks == 5 && context.busy == 5 && context.real_closes == 5);
    result = 0;
cleanup:
    if (held != NULL)
        (void)mc_client_sdk_icons_release(&held, &error);
    if (icons != NULL)
        (void)mc_client_sdk_icons_release(&icons, &error);
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    if (result == 0)
        printf("SDK_ICONS_OK|checks=%u\n", checks);
    return result;
}
