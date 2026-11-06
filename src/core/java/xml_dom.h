#ifndef MC_CORE_JAVA_XML_DOM_H
#define MC_CORE_JAVA_XML_DOM_H
#include "mc/core/java/url.h"
typedef struct mc_java_xml_document mc_java_xml_document;
typedef struct mc_java_xml_factory mc_java_xml_factory;
typedef struct mc_java_xml_builder mc_java_xml_builder;
typedef struct mc_java_xml_node mc_java_xml_node;
typedef struct mc_java_xml_list mc_java_xml_list;

typedef enum mc_java_xml_status {
    MC_JAVA_XML_OK,
    MC_JAVA_XML_ARGUMENT,
    MC_JAVA_XML_BUSY,
    MC_JAVA_XML_LIMIT,
    MC_JAVA_XML_ALLOCATION,
    MC_JAVA_XML_INPUT,
    MC_JAVA_XML_PROVIDER
} mc_java_xml_status;

typedef struct mc_java_xml_error {
    mc_java_xml_status status;
    const char *operation;
    int parser_error;
    size_t line, column;
    bool input_source_close_reached;
    mc_java_url_error input;
} mc_java_xml_error;

typedef struct mc_java_xml_options {
    size_t max_nodes, max_depth, max_text_units, max_bytes;
} mc_java_xml_options;

mc_java_xml_options mc_java_xml_default_options(void);
/* Actual HTTP InputStream profile of DocumentBuilder.parse. Candidate owns
 * every created node even on failure; status alone acknowledges source return.
 * Input occurrence remains borrowed and live through failed-call retirement.
 * No generic provider callback or replacement filesystem is accepted. */
mc_java_xml_status mc_java_xml_factory_create(mc_java_xml_factory **, mc_java_xml_error *);
mc_java_xml_status mc_java_xml_factory_release(mc_java_xml_factory **, mc_java_xml_error *);
mc_java_xml_status mc_java_xml_builder_create(mc_java_xml_factory *, mc_java_xml_builder **,
                                              mc_java_xml_error *);
mc_java_xml_status mc_java_xml_builder_release(mc_java_xml_builder **, mc_java_xml_error *);
mc_java_xml_status mc_java_xml_builder_parse(mc_java_xml_builder *, mc_java_http_input *,
                                             const mc_java_xml_options *,
                                             mc_java_xml_document **candidate, mc_java_xml_error *);
mc_java_xml_status mc_java_xml_document_release(mc_java_xml_document **, mc_java_xml_error *);
mc_java_xml_node *mc_java_xml_root(mc_java_xml_document *); /* actual Document node */
mc_java_xml_status mc_java_xml_elements(mc_java_xml_document *, mc_java_xml_node *,
                                        const char *qualified_tag, mc_java_xml_list **,
                                        mc_java_xml_error *);
size_t mc_java_xml_list_length(const mc_java_xml_list *);
mc_java_xml_node *mc_java_xml_list_item(const mc_java_xml_list *, size_t);
void mc_java_xml_list_release(mc_java_xml_list **);
unsigned int mc_java_xml_node_type(const mc_java_xml_node *);
mc_java_xml_node *mc_java_xml_first_child(const mc_java_xml_node *);
mc_world_info_name *
mc_java_xml_node_value(const mc_java_xml_node *); /* nullable borrowed actual String */
#endif
