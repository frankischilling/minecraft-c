#include "xml_dom.h"
#include "../../platform/files/url_path.h"
#include <expat.h>
#include <stdlib.h>
#include <string.h>

struct mc_java_xml_factory {
    size_t holds;
    bool namespace_aware, validating, ignore_whitespace, expand_entities, ignore_comments,
        coalescing, xinclude;
};

struct mc_java_xml_builder {
    size_t holds;
    mc_java_xml_factory *factory;
    XML_Parser parser;
    bool used, active;
};

struct mc_java_xml_node {
    struct mc_java_xml_node *parent, *first, *last, *next, *owned_next;
    unsigned int type;
    char *tag;
    uint16_t *units;
    size_t count, capacity;
    mc_world_info_name *value;
};

struct mc_java_xml_document {
    size_t holds, nodes, depth, text_units, bytes;
    mc_java_xml_options options;
    mc_java_xml_node *document, *current, *owned;
    XML_Parser parser;
    mc_java_xml_builder *builder;
    bool active, ready, cdata;
    mc_java_xml_error failure;
};

struct mc_java_xml_list {
    mc_java_xml_document *document;
    mc_java_xml_node *root;
    char *tag;
};

mc_java_xml_options mc_java_xml_default_options(void) {
    return (mc_java_xml_options){131072u, 128u, 8388608u, 67108864u};
}

static mc_java_xml_status error(mc_java_xml_error *e, mc_java_xml_status s, const char *op) {
    if (e != NULL)
        *e = (mc_java_xml_error){.status = s, .operation = op};
    return s;
}

mc_java_xml_status mc_java_xml_factory_create(mc_java_xml_factory **out, mc_java_xml_error *e) {
    if (out == NULL)
        return error(e, MC_JAVA_XML_ARGUMENT, "DocumentBuilderFactory output");
    mc_java_xml_factory *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return error(e, MC_JAVA_XML_ALLOCATION, "actual default DocumentBuilderFactory owner");
    f->holds = 1;
    f->expand_entities = true;
    *out = f;
    return error(e, MC_JAVA_XML_OK, NULL);
}

mc_java_xml_status mc_java_xml_factory_release(mc_java_xml_factory **slot, mc_java_xml_error *e) {
    if (slot == NULL)
        return error(e, MC_JAVA_XML_ARGUMENT, "XML factory occurrence");
    if (*slot != NULL) {
        mc_java_xml_factory *f = *slot;
        if (--f->holds == 0)
            free(f);
        *slot = NULL;
    }
    return error(e, MC_JAVA_XML_OK, NULL);
}

mc_java_xml_status mc_java_xml_builder_create(mc_java_xml_factory *f, mc_java_xml_builder **out,
                                              mc_java_xml_error *e) {
    if (f == NULL || out == NULL)
        return error(e, MC_JAVA_XML_ARGUMENT, "newDocumentBuilder native arguments");
    if (f->holds == SIZE_MAX)
        return error(e, MC_JAVA_XML_LIMIT, "XML factory holds");
    mc_java_xml_builder *b = calloc(1, sizeof(*b));
    if (b == NULL)
        return error(e, MC_JAVA_XML_ALLOCATION, "actual DocumentBuilder owner");
    b->holds = 1;
    b->factory = f;
    ++f->holds;
    *out = b;
    b->parser = XML_ParserCreate(NULL);
    if (b->parser == NULL)
        return error(e, MC_JAVA_XML_ALLOCATION, "concrete default XML decoder");
    return error(e, MC_JAVA_XML_OK, NULL);
}

mc_java_xml_status mc_java_xml_builder_release(mc_java_xml_builder **slot, mc_java_xml_error *e) {
    if (slot == NULL)
        return error(e, MC_JAVA_XML_ARGUMENT, "XML builder occurrence");
    mc_java_xml_builder *b = *slot;
    if (b == NULL)
        return error(e, MC_JAVA_XML_OK, NULL);
    if (b->holds > 1) {
        --b->holds;
        *slot = NULL;
        return error(e, MC_JAVA_XML_OK, NULL);
    }
    if (b->active)
        return error(e, MC_JAVA_XML_BUSY, "active XML builder");
    if (b->parser != NULL)
        XML_ParserFree(b->parser);
    mc_java_xml_factory_release(&b->factory, e);
    free(b);
    *slot = NULL;
    return error(e, MC_JAVA_XML_OK, NULL);
}

static void abort_parse(mc_java_xml_document *d, mc_java_xml_status s, const char *op) {
    if (d->failure.status == MC_JAVA_XML_OK)
        error(&d->failure, s, op);
    XML_StopParser(d->parser, XML_FALSE);
}

static void *allocate(mc_java_xml_document *d, size_t n) {
    if (n > d->options.max_bytes - d->bytes) {
        abort_parse(d, MC_JAVA_XML_LIMIT, "XML native allocation bound");
        return NULL;
    }
    void *p = calloc(1, n);
    if (p == NULL) {
        abort_parse(d, MC_JAVA_XML_ALLOCATION, "XML native node storage");
        return NULL;
    }
    d->bytes += n;
    return p;
}

static mc_java_xml_node *node(mc_java_xml_document *d, unsigned int type, const char *tag) {
    if (d->nodes == d->options.max_nodes) {
        abort_parse(d, MC_JAVA_XML_LIMIT, "XML node bound");
        return NULL;
    }
    mc_java_xml_node *n = allocate(d, sizeof(*n));
    if (n == NULL)
        return NULL;
    n->type = type;
    n->owned_next = d->owned;
    d->owned = n;
    ++d->nodes;
    if (tag != NULL) {
        size_t length = strlen(tag);
        n->tag = allocate(d, length + 1u);
        if (n->tag == NULL)
            return NULL;
        memcpy(n->tag, tag, length + 1u);
    }
    n->parent = d->current;
    if (d->current != NULL) {
        if (d->current->last != NULL)
            d->current->last->next = n;
        else
            d->current->first = n;
        d->current->last = n;
    }
    return n;
}

static void append(mc_java_xml_document *d, mc_java_xml_node *n, const char *data, size_t length) {
    for (size_t at = 0; at < length;) {
        uint32_t point;
        if (!mc_url_path_next_utf8(data, length, &at, &point)) {
            abort_parse(d, MC_JAVA_XML_PROVIDER, "XML decoder UTF8 contract");
            return;
        }
        size_t count = point >= 65536u ? 2u : 1u;
        if (count > d->options.max_text_units - d->text_units) {
            abort_parse(d, MC_JAVA_XML_LIMIT, "XML text bound");
            return;
        }
        if (n->count + count > n->capacity) {
            size_t cap = n->capacity == 0 ? 32u : n->capacity * 2u;
            if (cap < n->count + count)
                cap = n->count + count;
            if (cap > SIZE_MAX / sizeof(uint16_t)) {
                abort_parse(d, MC_JAVA_XML_LIMIT, "XML text capacity");
                return;
            }
            size_t bytes = cap * sizeof(uint16_t);
            uint16_t *replacement = allocate(d, bytes);
            if (replacement == NULL)
                return;
            if (n->count != 0)
                memcpy(replacement, n->units, n->count * sizeof(*replacement));
            d->bytes -= n->capacity * sizeof(uint16_t);
            free(n->units);
            n->units = replacement;
            n->capacity = cap;
        }
        if (point >= 65536u) {
            point -= 65536u;
            n->units[n->count++] = (uint16_t)(0xd800u + (point >> 10u));
            n->units[n->count++] = (uint16_t)(0xdc00u + (point & 1023u));
        } else
            n->units[n->count++] = (uint16_t)point;
        d->text_units += count;
    }
}

static void start(void *context, const XML_Char *name, const XML_Char **attributes) {
    (void)attributes;
    mc_java_xml_document *d = context;
    if (d->depth == d->options.max_depth) {
        abort_parse(d, MC_JAVA_XML_LIMIT, "XML depth bound");
        return;
    }
    mc_java_xml_node *n = node(d, 1u, name);
    if (n != NULL) {
        d->current = n;
        ++d->depth;
    }
}

static void end(void *context, const XML_Char *name) {
    (void)name;
    mc_java_xml_document *d = context;
    if (d->current != NULL) {
        d->current = d->current->parent;
        --d->depth;
    }
}

static void characters(void *context, const XML_Char *text, int length) {
    mc_java_xml_document *d = context;
    if (length <= 0 || d->current == NULL)
        return;
    unsigned int type = d->cdata ? 4u : 3u;
    mc_java_xml_node *n = d->current->last;
    if (n == NULL || n->type != type)
        n = node(d, type, NULL);
    if (n != NULL)
        append(d, n, text, (size_t)length);
}

static void comment(void *context, const XML_Char *text) {
    mc_java_xml_document *d = context;
    mc_java_xml_node *n = node(d, 8u, NULL);
    if (n != NULL)
        append(d, n, text, strlen(text));
}

static void pi(void *context, const XML_Char *target, const XML_Char *data) {
    mc_java_xml_document *d = context;
    mc_java_xml_node *n = node(d, 7u, target);
    if (n != NULL)
        append(d, n, data, strlen(data));
}

static void cdata_start(void *context) {
    mc_java_xml_document *d = context;
    d->cdata = true;
    (void)node(d, 4u, NULL);
}

static void cdata_end(void *context) {
    mc_java_xml_document *d = context;
    d->cdata = false;
}

static void doctype(void *context, const XML_Char *name, const XML_Char *system_id,
                    const XML_Char *public_id, int internal) {
    (void)name;
    (void)system_id;
    (void)public_id;
    (void)internal;
    abort_parse(context, MC_JAVA_XML_PROVIDER,
                "XML DTD/entity provider outside ordinary listing profile");
}

mc_java_xml_status mc_java_xml_builder_parse(mc_java_xml_builder *builder,
                                             mc_java_http_input *input,
                                             const mc_java_xml_options *options,
                                             mc_java_xml_document **out, mc_java_xml_error *e) {
    if (input == NULL || out == NULL || builder == NULL)
        return error(e, MC_JAVA_XML_ARGUMENT, "DocumentBuilder native arguments");
    if (builder->parser == NULL || builder->used || builder->active)
        return error(e, MC_JAVA_XML_BUSY, "one-attempt native XML builder profile");
    mc_java_xml_options o = options == NULL ? mc_java_xml_default_options() : *options;
    if (o.max_nodes == 0 || o.max_depth == 0 || o.max_bytes < sizeof(mc_java_xml_document))
        return error(e, MC_JAVA_XML_ARGUMENT, "XML native bounds");
    mc_java_xml_document *d = calloc(1, sizeof(*d));
    if (d == NULL)
        return error(e, MC_JAVA_XML_ALLOCATION, "XML Document owner");
    d->holds = 1;
    d->options = o;
    d->bytes = sizeof(*d);
    d->active = true;
    *out = d;
    d->parser = builder->parser;
    d->builder = builder;
    ++builder->holds;
    builder->active = true;
    builder->used = true;
    d->document = node(d, 9u, NULL);
    d->current = d->document;
    XML_SetUserData(d->parser, d);
    XML_SetElementHandler(d->parser, start, end);
    XML_SetCharacterDataHandler(d->parser, characters);
    XML_SetCommentHandler(d->parser, comment);
    XML_SetProcessingInstructionHandler(d->parser, pi);
    XML_SetCdataSectionHandler(d->parser, cdata_start, cdata_end);
    XML_SetStartDoctypeDeclHandler(d->parser, doctype);
    uint8_t bytes[4096];
    for (; d->failure.status == MC_JAVA_XML_OK;) {
        int32_t count = 0;
        mc_java_url_error ue = {0};
        if (mc_java_http_read(input, bytes, sizeof(bytes), &count, &ue) != MC_JAVA_URL_OK) {
            error(&d->failure, MC_JAVA_XML_INPUT, "actual XML InputStream.read");
            d->failure.input = ue;
            break;
        }
        bool final = count < 0;
        if (XML_Parse(d->parser, (const char *)bytes, final ? 0 : count,
                      final ? XML_TRUE : XML_FALSE) == XML_STATUS_ERROR) {
            if (d->failure.status == MC_JAVA_XML_OK) {
                error(&d->failure, MC_JAVA_XML_PROVIDER,
                      "XML malformed-input/error-handler provider frontier");
                d->failure.parser_error = (int)XML_GetErrorCode(d->parser);
                d->failure.line = (size_t)XML_GetCurrentLineNumber64(d->parser);
                d->failure.column = (size_t)XML_GetCurrentColumnNumber64(d->parser);
            }
            break;
        }
        if (final)
            break;
    }
    if (d->failure.status == MC_JAVA_XML_OK) {
        for (mc_java_xml_node *n = d->owned; n != NULL; n = n->owned_next) {
            if (n->type == 3u || n->type == 4u || n->type == 7u || n->type == 8u) {
                if (n->count == 0) {
                    n->value = mc_world_info_name_empty();
                } else if (mc_world_info_name_copy((mc_nbt_string){n->units, n->count}, SIZE_MAX,
                                                   &n->value) != MC_WORLD_INFO_OK) {
                    error(&d->failure, MC_JAVA_XML_ALLOCATION, "DOM node-value String");
                    break;
                }
            }
        }
    }
    if (d->failure.status == MC_JAVA_XML_OK) {
        d->failure.input_source_close_reached = true;
        mc_java_url_error closing = {0};
        if (mc_java_http_source_close(input, &closing) != MC_JAVA_URL_OK) {
            error(&d->failure, MC_JAVA_XML_INPUT, "XML installed-reader close");
            d->failure.input_source_close_reached = true;
            d->failure.input = closing;
        } else
            d->ready = true;
    }
    d->active = false;
    builder->active = false;
    if (e != NULL)
        *e = d->failure;
    return d->failure.status;
}

mc_java_xml_node *mc_java_xml_root(mc_java_xml_document *d) {
    return d != NULL && d->ready ? d->document : NULL;
}

static mc_java_xml_node *following(mc_java_xml_node *root, mc_java_xml_node *n) {
    if (n->first != NULL)
        return n->first;
    while (n != root && n->next == NULL)
        n = n->parent;
    return n == root ? NULL : n->next;
}

mc_java_xml_status mc_java_xml_elements(mc_java_xml_document *d, mc_java_xml_node *root,
                                        const char *tag, mc_java_xml_list **out,
                                        mc_java_xml_error *e) {
    if (d == NULL || root == NULL || tag == NULL || out == NULL || !d->ready)
        return error(e, MC_JAVA_XML_ARGUMENT, "DOM getElementsByTagName native arguments");
    if (d->holds == SIZE_MAX)
        return error(e, MC_JAVA_XML_LIMIT, "DOM holds");
    mc_java_xml_list *list = calloc(1, sizeof(*list));
    if (list == NULL)
        return error(e, MC_JAVA_XML_ALLOCATION, "DOM NodeList owner");
    size_t length = strlen(tag);
    list->tag = malloc(length + 1u);
    if (list->tag == NULL) {
        free(list);
        return error(e, MC_JAVA_XML_ALLOCATION, "DOM tag String");
    }
    memcpy(list->tag, tag, length + 1u);
    list->root = root;
    list->document = d;
    ++d->holds;
    *out = list;
    return error(e, MC_JAVA_XML_OK, NULL);
}

mc_java_xml_node *mc_java_xml_list_item(const mc_java_xml_list *list, size_t index) {
    if (list == NULL)
        return NULL;
    for (mc_java_xml_node *n = list->root->first; n != NULL; n = following(list->root, n))
        if (n->type == 1u && n->tag != NULL && strcmp(n->tag, list->tag) == 0) {
            if (index == 0)
                return n;
            --index;
        }
    return NULL;
}

size_t mc_java_xml_list_length(const mc_java_xml_list *list) {
    size_t count = 0;
    if (list != NULL)
        for (mc_java_xml_node *n = list->root->first; n != NULL; n = following(list->root, n))
            if (n->type == 1u && n->tag != NULL && strcmp(n->tag, list->tag) == 0)
                ++count;
    return count;
}

void mc_java_xml_list_release(mc_java_xml_list **slot) {
    if (slot != NULL && *slot != NULL) {
        mc_java_xml_list *list = *slot;
        mc_java_xml_error ignored;
        mc_java_xml_document_release(&list->document, &ignored);
        free(list->tag);
        free(list);
        *slot = NULL;
    }
}

mc_java_xml_status mc_java_xml_document_release(mc_java_xml_document **slot, mc_java_xml_error *e) {
    if (slot == NULL)
        return error(e, MC_JAVA_XML_ARGUMENT, "DOM occurrence");
    mc_java_xml_document *d = *slot;
    if (d == NULL)
        return error(e, MC_JAVA_XML_OK, NULL);
    if (d->holds > 1) {
        --d->holds;
        *slot = NULL;
        return error(e, MC_JAVA_XML_OK, NULL);
    }
    if (d->active)
        return error(e, MC_JAVA_XML_BUSY, "active DOM final occurrence");
    mc_java_xml_builder_release(&d->builder, e);
    mc_java_xml_node *n = d->owned;
    while (n != NULL) {
        mc_java_xml_node *next = n->owned_next;
        mc_world_info_name_release(n->value);
        free(n->units);
        free(n->tag);
        free(n);
        n = next;
    }
    free(d);
    *slot = NULL;
    return error(e, MC_JAVA_XML_OK, NULL);
}

unsigned int mc_java_xml_node_type(const mc_java_xml_node *n) {
    return n == NULL ? 0u : n->type;
}

mc_java_xml_node *mc_java_xml_first_child(const mc_java_xml_node *n) {
    return n == NULL ? NULL : n->first;
}

mc_world_info_name *mc_java_xml_node_value(const mc_java_xml_node *n) {
    return n == NULL ? NULL : n->value;
}
