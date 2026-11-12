#include "application_gl_diagnostics.h"

static const uint16_t text_0[] = {73,  110, 118, 97, 108, 105, 100, 32,  101,
                                  110, 117, 109, 32, 40,  103, 108, 117, 41};
static const uint16_t text_1[] = {73,  110, 118, 97, 108, 105, 100, 32,  118, 97,
                                  108, 117, 101, 32, 40,  103, 108, 117, 41};
static const uint16_t text_2[] = {79,  117, 116, 32, 111, 102, 32,  109, 101, 109,
                                  111, 114, 121, 32, 40,  103, 108, 117, 41};
static const uint16_t text_3[] = {78, 111, 32, 101, 114, 114, 111, 114};
static const uint16_t text_4[] = {73, 110, 118, 97, 108, 105, 100, 32, 101, 110, 117, 109};
static const uint16_t text_5[] = {73, 110, 118, 97, 108, 105, 100, 32, 118, 97, 108, 117, 101};
static const uint16_t text_6[] = {73,  110, 118, 97, 108, 105, 100, 32, 111,
                                  112, 101, 114, 97, 116, 105, 111, 110};
static const uint16_t text_7[] = {83, 116, 97, 99, 107, 32, 111, 118, 101, 114, 102, 108, 111, 119};
static const uint16_t text_8[] = {83,  116, 97,  99,  107, 32,  117, 110,
                                  100, 101, 114, 102, 108, 111, 119};
static const uint16_t text_9[] = {79, 117, 116, 32, 111, 102, 32, 109, 101, 109, 111, 114, 121};
static const uint16_t text_10[] = {84,  97, 98,  108, 101, 32,  116, 111,
                                   111, 32, 108, 97,  114, 103, 101};
static const uint16_t text_11[] = {73,  110, 118, 97,  108, 105, 100, 32,  102, 114,
                                   97,  109, 101, 98,  117, 102, 102, 101, 114, 32,
                                   111, 112, 101, 114, 97,  116, 105, 111, 110};

mc_client_glu_error_text mc_client_application_glu_error_text(int32_t error) {
    switch (error) {
        case 100900:
            return (mc_client_glu_error_text){
                {text_0, sizeof(text_0) / sizeof(text_0[0])}, false, "org/lwjgl/util/glu/GLU", 28};
        case 100901:
            return (mc_client_glu_error_text){
                {text_1, sizeof(text_1) / sizeof(text_1[0])}, false, "org/lwjgl/util/glu/GLU", 31};
        case 100902:
            return (mc_client_glu_error_text){
                {text_2, sizeof(text_2) / sizeof(text_2[0])}, false, "org/lwjgl/util/glu/GLU", 34};
        case 0:
            return (mc_client_glu_error_text){
                {text_3, sizeof(text_3) / sizeof(text_3[0])}, false, "org/lwjgl/opengl/Util", 84};
        case 1280:
            return (mc_client_glu_error_text){
                {text_4, sizeof(text_4) / sizeof(text_4[0])}, false, "org/lwjgl/opengl/Util", 87};
        case 1281:
            return (mc_client_glu_error_text){
                {text_5, sizeof(text_5) / sizeof(text_5[0])}, false, "org/lwjgl/opengl/Util", 90};
        case 1282:
            return (mc_client_glu_error_text){
                {text_6, sizeof(text_6) / sizeof(text_6[0])}, false, "org/lwjgl/opengl/Util", 93};
        case 1283:
            return (mc_client_glu_error_text){
                {text_7, sizeof(text_7) / sizeof(text_7[0])}, false, "org/lwjgl/opengl/Util", 96};
        case 1284:
            return (mc_client_glu_error_text){
                {text_8, sizeof(text_8) / sizeof(text_8[0])}, false, "org/lwjgl/opengl/Util", 99};
        case 1285:
            return (mc_client_glu_error_text){
                {text_9, sizeof(text_9) / sizeof(text_9[0])}, false, "org/lwjgl/opengl/Util", 102};
        case 32817:
            return (mc_client_glu_error_text){{text_10, sizeof(text_10) / sizeof(text_10[0])},
                                              false,
                                              "org/lwjgl/opengl/Util",
                                              105};
        case 1286:
            return (mc_client_glu_error_text){{text_11, sizeof(text_11) / sizeof(text_11[0])},
                                              false,
                                              "org/lwjgl/opengl/Util",
                                              108};
        default:
            return (mc_client_glu_error_text){
                .is_null = true, .source_class = "org/lwjgl/opengl/Util", .literal_pc = 111};
    }
}
