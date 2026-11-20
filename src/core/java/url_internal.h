#ifndef MC_CORE_JAVA_URL_INTERNAL_H
#define MC_CORE_JAVA_URL_INTERNAL_H
#include "mc/core/java/url.h"

struct mc_java_uri {
    size_t holds;
    mc_java_file *absolute;
    char *path;
    uint16_t *url_path;
    size_t url_length;
};

struct mc_java_url {
    unsigned int phase;
    size_t holds;
    mc_world_info_name *text;
    char *request, *file_path;
    uint16_t *url_path;
    size_t url_length;
    bool http, file;
};

mc_java_url_status mc_url_fail(mc_java_url_error *, mc_java_url_status, const char *, int);
#endif
