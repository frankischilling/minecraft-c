#include "client/resources/download_thread_internal.h"
#include "cz_test_files.h"
/* Explicit local HTTP URL argument only; no public endpoint replacement in
 * source run(). A real owned server must be authorized/launched separately. */
static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "cz HTTP %u line%d: %s\n", checks, __LINE__, #x);                      \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    if (argc != 3)
        return 77;
    int result = 1;
    mc_java_files *files = NULL;
    mc_java_file *root = NULL;
    mc_client_download_classes *classes = NULL;
    mc_client_download_thread *w = NULL;
    client_application *app = calloc(1, sizeof(*app));
    mc_client_download_error *e = calloc(1, sizeof(*e));
    mc_world_info_name *base_text = NULL, *key = NULL, *data_name = NULL;
    mc_java_url *base = NULL, *transport_url = NULL;
    mc_java_http_input *transport = NULL;
    mc_world_info_name *transport_path = NULL;
    mc_java_http_input *listing = NULL, *zero_input = NULL;
    mc_java_url_data_input *zero_dis = NULL;
    mc_java_url *data_url = NULL;
    mc_java_xml_factory *factory = NULL;
    mc_java_xml_builder *builder = NULL;
    mc_java_xml_document *document = NULL;
    mc_java_xml_list *contents = NULL, *fields = NULL;
    mc_java_xml_error xe = {0};
    mc_java_url_error ue = {0};
    mc_java_file_error fe = {0};
    CHECK(app != NULL && e != NULL);
    CHECK(test_directory(argv[1], &files, &root));
    CHECK(test_name(argv[2], &base_text));
    CHECK(mc_java_url_create(base_text, &base, &ue) == MC_JAVA_URL_OK);
    /* Exercise multiple socket reads and buffer refill with a payload larger
     * than the native HTTP buffer. Every returned byte is independently known. */
    CHECK(test_name("ignored/large.bin", &transport_path));
    CHECK(mc_java_url_resolve(base, transport_path, &transport_url, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_http_open(transport_url, NULL, NULL, &transport, &ue) == MC_JAVA_URL_OK);
    uint8_t transport_bytes[4096];
    size_t transport_total = 0;
    for (;;) {
        int32_t count = 0;
        CHECK(mc_java_http_read(transport, transport_bytes, sizeof(transport_bytes), &count, &ue) ==
              MC_JAVA_URL_OK);
        if (count < 0)
            break;
        CHECK(count > 0 && transport_total + (size_t)count <= 65539u);
        for (size_t index = 0; index < (size_t)count; ++index)
            CHECK(transport_bytes[index] == (uint8_t)((transport_total + index) * 17u + 3u));
        transport_total += (size_t)count;
    }
    CHECK(transport_total == 65539u);
    CHECK(mc_java_http_source_close(transport, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_http_release(&transport, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_url_release(&transport_url, &ue) == MC_JAVA_URL_OK);
    mc_world_info_name_release(transport_path);
    transport_path = NULL;
    CHECK(test_name("redirect", &transport_path));
    CHECK(mc_java_url_resolve(base, transport_path, &transport_url, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_http_open(transport_url, NULL, NULL, &transport, &ue) == MC_JAVA_URL_OK);
    int32_t redirected_count = 0;
    CHECK(mc_java_http_read(transport, transport_bytes, sizeof(transport_bytes), &redirected_count,
                            &ue) == MC_JAVA_URL_OK &&
          redirected_count == 5 && transport_bytes[0] == 1 && transport_bytes[4] == 5);
    CHECK(mc_java_http_release(&transport, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_url_release(&transport_url, &ue) == MC_JAVA_URL_OK);
    mc_world_info_name_release(transport_path);
    transport_path = NULL;
    CHECK(test_name("missing", &transport_path));
    CHECK(mc_java_url_resolve(base, transport_path, &transport_url, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_http_open(transport_url, NULL, NULL, &transport, &ue) ==
              MC_JAVA_URL_SOURCE_NOT_FOUND &&
          transport != NULL && ue.source_exception && ue.response_code == 404);
    CHECK(mc_java_http_release(&transport, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_url_release(&transport_url, &ue) == MC_JAVA_URL_OK);
    mc_world_info_name_release(transport_path);
    transport_path = NULL;
    CHECK(mc_java_xml_factory_create(&factory, &xe) == MC_JAVA_XML_OK);
    CHECK(mc_java_xml_builder_create(factory, &builder, &xe) == MC_JAVA_XML_OK);
    CHECK(mc_java_http_open(base, NULL, NULL, &listing, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_xml_builder_parse(builder, listing, NULL, &document, &xe) == MC_JAVA_XML_OK &&
          xe.input_source_close_reached);
    mc_java_http_state hs;
    CHECK(mc_java_http_inspect(listing, &hs) && !hs.source_open && hs.source_closes == 1);
    CHECK(mc_java_xml_elements(document, mc_java_xml_root(document), "Contents", &contents, &xe) ==
              MC_JAVA_XML_OK &&
          mc_java_xml_list_length(contents) == 1);
    mc_java_xml_node *element = mc_java_xml_list_item(contents, 0);
    CHECK(element == mc_java_xml_list_item(contents, 0));
    CHECK(mc_java_xml_elements(document, element, "Key", &fields, &xe) == MC_JAVA_XML_OK);
    mc_java_xml_node *value = mc_java_xml_first_child(mc_java_xml_list_item(fields, 0));
    CHECK(mc_java_xml_node_type(value) == 4 &&
          mc_world_info_name_text(mc_java_xml_node_value(value)).length == 16);
    mc_java_xml_list_release(&fields);
    CHECK(mc_java_xml_elements(document, element, "Size", &fields, &xe) == MC_JAVA_XML_OK);
    value = mc_java_xml_first_child(mc_java_xml_list_item(fields, 0));
    CHECK(mc_java_xml_node_type(value) == 8 &&
          mc_world_info_name_text(mc_java_xml_node_value(value)).length == 1);
    mc_java_xml_list_release(&fields);
    CHECK(test_name("ignored/data.bin", &data_name));
    CHECK(mc_java_url_resolve(base, data_name, &data_url, &ue) == MC_JAVA_URL_OK);
    mc_world_info_name_release(data_name);
    data_name = NULL;
    CHECK(mc_java_url_data_input_allocate(65536u, &zero_dis, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_http_open(data_url, NULL, NULL, &zero_input, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_java_url_data_input_construct(zero_dis, zero_input, &ue) == MC_JAVA_URL_OK);
    mc_java_http_state before_zero, after_zero;
    CHECK(mc_java_http_inspect(zero_input, &before_zero));
    int32_t zero_count = 19;
    CHECK(mc_java_url_data_input_read(zero_dis, NULL, 0, false, &zero_count, &ue) ==
              MC_JAVA_URL_OK &&
          zero_count == 0);
    CHECK(mc_java_http_inspect(zero_input, &after_zero) &&
          after_zero.reads == before_zero.reads + 1 && after_zero.consumed == before_zero.consumed);
    CHECK(mc_java_url_data_input_close(zero_dis, &ue) == MC_JAVA_URL_OK);
    CHECK(mc_client_download_classes_create(files, NULL, &classes, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_allocate(classes, NULL, &w, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_construct(w, root, app, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(test_name("ignored/data.bin", &key));
    CHECK(mc_client_download_thread_process_entry(w, base, key, 5, 0, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(w->call.file == NULL);
    CHECK(mc_client_download_thread_process_entry(w, base, key, 5, 1, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(w->writes > 0 && w->call.file != NULL);
    mc_java_file_output_state fs;
    CHECK(mc_java_file_output_inspect(w->call.fos, &fs) && fs.bytes_committed == 5 &&
          fs.source_close_succeeded && !fs.native_open);
    CHECK(w->call.escaped == key);
    CHECK(mc_client_download_thread_process_entry(w, base, key, 5, 1, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(w->call.input == NULL); /* Existing correct size avoids another GET. */
    mc_world_info_name_release(key);
    key = NULL;
    CHECK(test_name("ignored/cancel.bin", &key));
    CHECK(mc_client_download_thread_cancel(w, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_process_entry(w, base, key, 5, 1, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_java_file_output_inspect(w->call.fos, &fs) && fs.bytes_committed == 5 &&
          fs.source_close_calls == 0 && fs.native_open);
    CHECK(mc_java_http_inspect(w->call.input, &hs) && hs.source_open && hs.source_closes == 0);
    result = 0;
cleanup:
    (void)mc_java_http_release(&transport, &ue);
    (void)mc_java_url_release(&transport_url, &ue);
    mc_world_info_name_release(transport_path);
    (void)mc_java_url_data_input_release(&zero_dis, &ue);
    (void)mc_java_http_release(&zero_input, &ue);
    (void)mc_java_url_release(&data_url, &ue);
    mc_java_xml_list_release(&fields);
    mc_java_xml_list_release(&contents);
    (void)mc_java_xml_document_release(&document, &xe);
    (void)mc_java_xml_builder_release(&builder, &xe);
    (void)mc_java_xml_factory_release(&factory, &xe);
    (void)mc_java_http_release(&listing, &ue);
    if (e != NULL) {
        (void)mc_client_download_thread_release(&w, e);
        (void)mc_client_download_classes_release(&classes, e);
    }
    mc_java_url_release(&base, &ue);
    mc_world_info_name_release(data_name);
    mc_world_info_name_release(base_text);
    mc_world_info_name_release(key);
    mc_java_file_release(&root, &fe);
    mc_java_files_release(&files, &fe);
    if (w == NULL)
        free(app);
    free(e);
    if (result == 0)
        printf("download HTTP: %u checks\n", checks);
    return result;
}
