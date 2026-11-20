#include "download_thread_internal.h"
#include "mc/core/java/integer_parse.h"
#include <stdlib.h>
#include <string.h>

static mc_client_download_status source(mc_client_download_error *e, const char *method,
                                        const char *descriptor, int pc, const char *exception,
                                        const char *op) {
    mc_download_fail(e, MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION, "cz", method, descriptor, pc, op);
    e->exception_class = exception;
    return e->status;
}

static mc_client_download_status url_failure(mc_client_download_error *e, int pc,
                                             const char *descriptor) {
    bool caught = e->url.source_exception;
    mc_download_fail(e,
                     caught ? MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION
                     : e->url.status == MC_JAVA_URL_NATIVE_CANCELLED
                         ? MC_CLIENT_DOWNLOAD_CANCELLED_NATIVE
                     : e->url.status == MC_JAVA_URL_LIMIT ? MC_CLIENT_DOWNLOAD_LIMIT
                                                          : MC_CLIENT_DOWNLOAD_UNIMPLEMENTED,
                     "cz", "a", descriptor, pc, e->url.operation);
    e->exception_class = e->url.exception_class;
    return e->status;
}

static mc_client_download_status output_failure(mc_client_download_error *e, int pc) {
    bool caught = e->output.cause.source_exception;
    mc_download_fail(e,
                     caught                                    ? MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION
                     : e->output.status == MC_JAVA_OUTPUT_BUSY ? MC_CLIENT_DOWNLOAD_BUSY
                     : e->output.status == MC_JAVA_OUTPUT_LIMIT ? MC_CLIENT_DOWNLOAD_LIMIT
                                                                : MC_CLIENT_DOWNLOAD_UNIMPLEMENTED,
                     "cz", "a", "(Ljava/net/URL;Ljava/io/File;J)V", pc, e->output.cause.operation);
    e->exception_class = e->output.cause.exception_class;
    return e->status;
}

static mc_client_download_status entry_file_error(mc_client_download_error *e, mc_java_file_error f,
                                                  int pc, const char *method) {
    mc_client_download_status s = mc_download_file_error(e, f, pc, method);
    if (pc >= 0)
        e->site.descriptor = "(Ljava/net/URL;Ljava/lang/String;JI)V";
    return s;
}

static bool equal_ascii(mc_nbt_string s, const char *literal) {
    size_t n = strlen(literal);
    if (s.length != n)
        return false;
    for (size_t i = 0; i < n; ++i)
        if (s.units[i] != (unsigned char)literal[i])
            return false;
    return true;
}

static mc_client_download_status download(mc_client_download_thread *w,
                                          mc_client_download_error *e) {
    w->call.buffer = calloc(4096u, 1u);
    if (w->call.buffer == NULL)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ALLOCATION, "cz", "a",
                                "(Ljava/net/URL;Ljava/io/File;J)V", 3,
                                "actual download array native storage");
    uint8_t *bytes = w->call.buffer;
    if (mc_java_url_data_input_allocate(65536u, &w->call.dis, &e->url) != MC_JAVA_URL_OK)
        return url_failure(e, 7, "(Ljava/net/URL;Ljava/io/File;J)V");
    if (mc_java_http_open(w->call.url, &w->options.http, &w->stop, &w->call.input, &e->url) !=
        MC_JAVA_URL_OK)
        return url_failure(e, 12, "(Ljava/net/URL;Ljava/io/File;J)V");
    if (mc_java_url_data_input_construct(w->call.dis, w->call.input, &e->url) != MC_JAVA_URL_OK)
        return url_failure(e, 15, "(Ljava/net/URL;Ljava/io/File;J)V");
    if (mc_java_data_output_allocate(NULL, &w->call.dos, &e->output) != MC_JAVA_OUTPUT_OK)
        return output_failure(e, 20);
    if (mc_java_file_output_allocate(w->classes->files, NULL, &w->call.fos, &e->output) !=
        MC_JAVA_OUTPUT_OK)
        return output_failure(e, 24);
    if (mc_java_file_output_construct(w->call.fos, w->call.file, &e->output) != MC_JAVA_OUTPUT_OK)
        return output_failure(e, 29);
    if (mc_java_data_output_construct(w->call.dos, w->call.fos, &e->output) != MC_JAVA_OUTPUT_OK)
        return output_failure(e, 32);
    for (;;) {
        int32_t count = 0;
        if (mc_java_url_data_input_read(w->call.dis, bytes, 4096, false, &count, &e->url) !=
            MC_JAVA_URL_OK)
            return url_failure(e, 44, "(Ljava/net/URL;Ljava/io/File;J)V");
        if (count < 0)
            break;
        mc_java_byte_array_view array = {bytes, 4096};
        if (mc_java_data_output_write_array(w->call.dos, &array, 0, count, &e->output) !=
            MC_JAVA_OUTPUT_OK)
            return output_failure(e, 60);
        ++w->writes;
        if (atomic_load_explicit(&w->c, memory_order_seq_cst))
            return MC_CLIENT_DOWNLOAD_OK;
    }
    if (mc_java_url_data_input_close(w->call.dis, &e->url) != MC_JAVA_URL_OK)
        return url_failure(e, 73, "(Ljava/net/URL;Ljava/io/File;J)V");
    if (mc_java_data_output_source_close(w->call.dos, &e->output) != MC_JAVA_OUTPUT_OK)
        return output_failure(e, 78);
    return MC_CLIENT_DOWNLOAD_OK;
}

static bool safe_key(mc_nbt_string key) {
    if (key.length == 0)
        return true;
    if (key.units[0] == '/' || key.units[0] == '\\')
        return false;
    size_t start = 0;
    for (size_t i = 0; i <= key.length; ++i) {
        if (i < key.length && (key.units[i] == ':' || key.units[i] == '\\' || key.units[i] == 0))
            return false;
        if (i == key.length || key.units[i] == '/') {
            if (i - start == 2u && key.units[start] == '.' && key.units[start + 1u] == '.')
                return false;
            start = i + 1u;
        }
    }
    return true;
}

static mc_client_download_status entry_body(mc_client_download_thread *w, mc_java_url *base,
                                            mc_world_info_name *key, int64_t size,
                                            unsigned int pass, mc_client_download_error *e) {
    const char *desc = "(Ljava/net/URL;Ljava/lang/String;JI)V";
    if (key == NULL)
        return source(e, "a", desc, 3, "java/lang/NullPointerException",
                      "actual entry key.indexOf");
    mc_nbt_string text = mc_world_info_name_text(key);
    size_t slash = 0;
    while (slash < text.length && text.units[slash] != '/')
        ++slash;
    if (slash == text.length)
        return source(e, "a", desc, 12, "java/lang/StringIndexOutOfBoundsException",
                      "actual entry prefix substring");
    if (mc_download_name(text.units, slash, &w->call.entry_prefix, e) != MC_CLIENT_DOWNLOAD_OK)
        return e->status;
    mc_nbt_string prefix = mc_world_info_name_text(w->call.entry_prefix);
    bool sound = equal_ascii(prefix, "sound") || equal_ascii(prefix, "newsound");
    if ((sound && pass != 0u) || (!sound && pass != 1u))
        return MC_CLIENT_DOWNLOAD_OK;
    if (mc_world_info_name_retain(key) != MC_WORLD_INFO_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_LIMIT, NULL, NULL, NULL, -1,
                                "entry key native hold");
    w->call.key = key;
    mc_java_file_error fe = {0};
    if (mc_java_file_allocate(w->classes->files, &w->call.file, &fe) != MC_JAVA_FILE_OK)
        return entry_file_error(e, fe, 50, "a");
    if (mc_java_file_construct_child(w->call.file, w->a, key, &fe) != MC_JAVA_FILE_OK)
        return entry_file_error(e, fe, 59, "a");
    /* Native untrusted-provider admission at the first reached filesystem
     * operation. Source construction/fields above are preserved. */
    if (!safe_key(text))
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_UNIMPLEMENTED, "cz", "a", desc, 66,
                                "unsafe remote-key filesystem provider");
    bool exists = false;
    if (mc_java_file_exists(w->call.file, &exists, &fe) != MC_JAVA_FILE_OK)
        return entry_file_error(e, fe, 66, "a");
    int64_t length = 0;
    if (exists && mc_java_file_length(w->call.file, &length, &fe) != MC_JAVA_FILE_OK)
        return entry_file_error(e, fe, 74, "a");
    if (!exists || length != size) {
        if (mc_java_file_get_parent_file(w->call.file, &w->call.parent, &fe) != MC_JAVA_FILE_OK)
            return entry_file_error(e, fe, 84, "a");
        if (w->call.parent == NULL)
            return source(e, "a", desc, 87, "java/lang/NullPointerException",
                          "actual parent mkdirs receiver");
        bool ignored = false;
        if (mc_java_file_mkdirs(w->call.parent, &ignored, &fe) != MC_JAVA_FILE_OK)
            return entry_file_error(e, fe, 87, "a");
        size_t count = text.length;
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] == ' ') {
                if (count > 65536u - 2u)
                    return mc_download_fail(e, MC_CLIENT_DOWNLOAD_LIMIT, NULL, NULL, NULL, -1,
                                            "entry escaped String bound");
                count += 2u;
            }
        uint16_t *units = malloc((count == 0 ? 1u : count) * sizeof(*units));
        if (units == NULL)
            return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ALLOCATION, NULL, NULL, NULL, -1,
                                    "entry space replacement String");
        size_t at = 0;
        for (size_t i = 0; i < text.length; ++i) {
            if (text.units[i] == ' ') {
                units[at++] = '%';
                units[at++] = '2';
                units[at++] = '0';
            } else
                units[at++] = text.units[i];
        }
        mc_client_download_status status;
        if (count == text.length) {
            status = mc_world_info_name_retain(key) == MC_WORLD_INFO_OK
                         ? MC_CLIENT_DOWNLOAD_OK
                         : mc_download_fail(e, MC_CLIENT_DOWNLOAD_LIMIT, NULL, NULL, NULL, -1,
                                            "unchanged replacement String hold");
            if (status == MC_CLIENT_DOWNLOAD_OK)
                w->call.escaped = key;
        } else
            status = mc_download_name(units, count, &w->call.escaped, e);
        free(units);
        if (status != MC_CLIENT_DOWNLOAD_OK)
            return status;
        if (mc_java_url_resolve(base, w->call.escaped, &w->call.url, &e->url) != MC_JAVA_URL_OK)
            return url_failure(e, 109, desc);
        status = download(w, e);
        if (status != MC_CLIENT_DOWNLOAD_OK) {
            mc_download_caller(e, "cz", "a", desc, 115);
            return status;
        }
        if (atomic_load_explicit(&w->c, memory_order_seq_cst))
            return MC_CLIENT_DOWNLOAD_OK;
    }
    mc_client_download_status status = mc_download_register(w, w->b, key, w->call.file, e);
    if (status != MC_CLIENT_DOWNLOAD_OK)
        mc_download_caller(e, "cz", "a", desc, 133);
    return status;
}

static mc_client_download_status entry(mc_client_download_thread *w, mc_java_url *base,
                                       mc_world_info_name *key, int64_t size, unsigned int pass,
                                       mc_client_download_error *e) {
    if (mc_download_call_retire(w, e) != MC_CLIENT_DOWNLOAD_OK)
        return e->status;
    mc_client_download_status s = entry_body(w, base, key, size, pass, e);
    if (s == MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION) {
        e->print_stack_trace_pending = true;
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_UNIMPLEMENTED, "cz", "a",
                                "(Ljava/net/URL;Ljava/lang/String;JI)V", 143,
                                "actual caught Exception.printStackTrace dependency");
    }
    return s;
}

static mc_client_download_status value(mc_client_download_thread *w, mc_java_xml_node *element,
                                       const char *tag, int get_pc, int item_pc, int children_pc,
                                       int value_pc, mc_world_info_name **out,
                                       mc_client_download_error *e) {
    if (element == NULL)
        return source(e, "run", "()V", get_pc, "java/lang/NullPointerException",
                      "actual DOM Element receiver");
    if (mc_java_xml_elements(w->document, element, tag, &w->selection, &e->xml) != MC_JAVA_XML_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, "cz", "run", "()V", get_pc,
                                "actual DOM NodeList owner");
    mc_java_xml_node *selected = mc_java_xml_list_item(w->selection, 0);
    (void)item_pc;
    mc_java_xml_list_release(&w->selection);
    if (selected == NULL)
        return source(e, "run", "()V", children_pc, "java/lang/NullPointerException",
                      "actual missing DOM element child-list receiver");
    mc_java_xml_node *child = mc_java_xml_first_child(selected);
    if (child == NULL)
        return source(e, "run", "()V", value_pc, "java/lang/NullPointerException",
                      "actual first child getNodeValue receiver");
    *out = mc_java_xml_node_value(child);
    return MC_CLIENT_DOWNLOAD_OK;
}

static mc_client_download_status listing(mc_client_download_thread *w,
                                         mc_client_download_error *e) {
    if (mc_java_url_allocate(&w->listing_url, &e->url) != MC_JAVA_URL_OK) {
        url_failure(e, 0, "()V");
        e->site.method = "run";
        return e->status;
    }
    mc_world_info_name *url = NULL;
    if (mc_download_literal(w->classes, 2, &url, e) != MC_CLIENT_DOWNLOAD_OK) {
        mc_download_caller(e, "cz", "run", "()V", 4);
        return e->status;
    }
    mc_java_url_status us = mc_java_url_construct(w->listing_url, url, &e->url);
    mc_world_info_name_release(url);
    if (us != MC_JAVA_URL_OK) {
        url_failure(e, 6, "()V");
        e->site.method = "run";
        return e->status;
    }
    if (mc_java_xml_factory_create(&w->factory, &e->xml) != MC_JAVA_XML_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, "cz", "run", "()V", 10,
                                "actual default DocumentBuilderFactory.newInstance");
    if (mc_java_xml_builder_create(w->factory, &w->builder, &e->xml) != MC_JAVA_XML_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, "cz", "run", "()V", 15,
                                "actual newDocumentBuilder");
    if (mc_java_http_open(w->listing_url, &w->options.http, &w->stop, &w->listing, &e->url) !=
        MC_JAVA_URL_OK) {
        url_failure(e, 21, "()V");
        e->site.method = "run";
        return e->status;
    }
    if (mc_java_xml_builder_parse(w->builder, w->listing, &w->options.xml, &w->document, &e->xml) !=
        MC_JAVA_XML_OK) {
        if (e->xml.status == MC_JAVA_XML_INPUT && e->xml.input.source_exception) {
            e->url = e->xml.input;
            url_failure(e, 24, "()V");
            e->site.method = "run";
            return e->status;
        }
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_UNIMPLEMENTED, "cz", "run", "()V", 24,
                                e->xml.operation);
    }
    if (mc_java_xml_elements(w->document, mc_java_xml_root(w->document), "Contents", &w->contents,
                             &e->xml) != MC_JAVA_XML_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, "cz", "run", "()V", 33,
                                "actual Contents NodeList");
    for (unsigned int pass = 0; pass < 2u; ++pass) {
        for (size_t index = 0; index < mc_java_xml_list_length(w->contents); ++index) {
            if (atomic_load(&w->stop))
                return mc_download_fail(e, MC_CLIENT_DOWNLOAD_CANCELLED_NATIVE, NULL, NULL, NULL,
                                        -1, "cz native listing retirement stop");
            mc_java_xml_node *element = mc_java_xml_list_item(w->contents, index);
            if (mc_java_xml_node_type(element) != 1u)
                continue;
            mc_world_info_name *key = NULL, *size_text = NULL;
            if (value(w, element, "Key", 97, 103, 111, 122, &key, e) != MC_CLIENT_DOWNLOAD_OK ||
                value(w, element, "Size", 133, 139, 147, 158, &size_text, e) !=
                    MC_CLIENT_DOWNLOAD_OK)
                return e->status;
            mc_nbt_string string = mc_world_info_name_text(size_text);
            mc_java_integer_parse_text view = {string.units, string.length};
            int64_t size = 0;
            mc_java_integer_parse_status parsed =
                mc_java_long_parse(size_text == NULL ? NULL : &view, &size);
            if (parsed == MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT)
                return source(e, "run", "()V", 163, "java/lang/NumberFormatException",
                              "actual Long.parseLong Size");
            if (parsed != MC_JAVA_INTEGER_PARSE_OK)
                return mc_download_fail(e, MC_CLIENT_DOWNLOAD_LIMIT, "cz", "run", "()V", 163,
                                        "Long.parseLong native input bound");
            if (size <= 0)
                continue;
            w->run_pc = 183;
            if (entry(w, w->listing_url, key, size, pass, e) != MC_CLIENT_DOWNLOAD_OK) {
                mc_download_caller(e, "cz", "run", "()V", 183);
                return e->status;
            }
            w->run_pc = 187;
            if (atomic_load_explicit(&w->c, memory_order_seq_cst)) {
                e->source_returned = true;
                w->run_pc = 193;
                return MC_CLIENT_DOWNLOAD_OK;
            }
        }
    }
    w->run_pc = 224;
    e->source_returned = true;
    return MC_CLIENT_DOWNLOAD_OK;
}

mc_client_download_status mc_download_run(mc_client_download_thread *w,
                                          mc_client_download_error *e) {
    mc_client_download_status status = listing(w, e);
    if (status != MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION)
        return status;
    if (w->caught_count++ == 0)
        w->caught = *e;
    w->run_caught = *e;
    *e = (mc_client_download_error){0};
    w->run_pc = 217;
    status = mc_download_scan(w, e);
    if (status != MC_CLIENT_DOWNLOAD_OK) {
        e->uncaught = status == MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION;
        mc_download_caller(e, "cz", "run", "()V", 217);
        return status;
    }
    *e = w->run_caught;
    e->print_stack_trace_pending = true;
    e->source_returned = false;
    w->run_pc = 221;
    return mc_download_fail(e, MC_CLIENT_DOWNLOAD_UNIMPLEMENTED, "cz", "run", "()V", 221,
                            "actual caught Exception.printStackTrace after local fallback");
}

mc_client_download_status mc_client_download_thread_process_entry(mc_client_download_thread *w,
                                                                  mc_java_url *base,
                                                                  mc_world_info_name *key,
                                                                  int64_t size, unsigned int pass,
                                                                  mc_client_download_error *e) {
    if (e == NULL || w == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (mtx_trylock(&w->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz explicit entry active");
    if (!w->constructed || w->started || atomic_load(&w->retiring)) {
        mtx_unlock(&w->mutex);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_STATE, NULL, NULL, NULL, -1,
                                "cz explicit entry native admission");
    }
    mc_client_download_status s = entry(w, base, key, size, pass, e);
    if (s == MC_CLIENT_DOWNLOAD_OK)
        e->source_returned = true;
    else
        w->failure = *e;
    mtx_unlock(&w->mutex);
    return s;
}
