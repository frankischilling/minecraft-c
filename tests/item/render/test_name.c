/* Genuine App/World/LocalPlayer/Registry fixture; unused included entry point. */
#define main mc_item_name_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "item/render/name.h"
#include "item/stack/source_internal.h"
#include <limits.h>

static const struct name_case {
    int32_t id, damage;
    const char *value;
    bool exception;
} cases[] = {

#include "name_cases.inc"
};

static bool equal_ascii(const mc_registry_string *value, const char *ascii) {
    if (value == NULL || ascii == NULL)
        return value == NULL && ascii == NULL;
    mc_registry_string_view view;
    if (!mc_registry_string_read(value, &view) || view.length != strlen(ascii))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (unsigned char)ascii[i])
            return false;
    return true;
}

static const mc_item *item_at(fixture *f, int32_t id) {
    const mc_registry_item_array *array;
    const mc_item *item;
    if (mc_registry_item_c_get(f->app.registry, &array, NULL) != MC_REGISTRY_OK ||
        mc_registry_item_c_at(f->app.registry, array, id, &item, NULL) != MC_REGISTRY_OK)
        return NULL;
    return item;
}

static bool stack_at(int32_t id, int32_t damage, mc_item_stack **out) {
    CHECK(mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(*out, id, 1, damage) == MC_ITEM_STACK_OK);
    return true;
}

static bool observations(fixture *f, mc_item_name_workspace *w) {
    bool seen[MC_ITEM_KIND_COUNT] = {false};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        const struct name_case *v = &cases[i];
        mc_item_stack *stack = NULL;
        CHECK(stack_at(v->id, v->damage, &stack));
        const mc_item *item = item_at(f, v->id);
        CHECK(item != NULL);
        seen[item->kind] = true;
        const mc_registry_string *result = (const mc_registry_string *)(uintptr_t)1;
        mc_item_render_error e;
        const bool ok = mc_item_stack_render_name(f->app.registry, stack, w, &result, &e);
        if (v->exception) {
            CHECK(!ok && result == (const mc_registry_string *)(uintptr_t)1 && e.source_exception &&
                  !e.native_policy && e.site.pc == 26 && strcmp(e.site.class_name, "en") == 0 &&
                  strcmp(e.exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0);
        } else {
            CHECK(ok && e.returned && e.id_read && e.site.pc == 12 &&
                  equal_ascii(result, v->value));
        }
        CHECK(mc_item_name_workspace_reset(w));
        mc_item_stack_release(stack);
    }
    for (size_t i = 0; i < MC_ITEM_KIND_COUNT; ++i)
        CHECK(seen[i]);
    return true;
}

static bool identities_and_order(fixture *f, mc_item_name_workspace *w) {
    mc_registry_classes *r = f->app.registry;
    mc_item_render_error e;
    const mc_registry_string *a, *b, *literal;
    mc_item *sword = (mc_item *)item_at(f, 267);
    const mc_item_constructor_phase phase = sword->phase;
    const mc_registry_string *old_name = sword->fields.bl;
    CHECK(mc_registry_literal_ascii(r, "changed-name", &literal) == MC_REGISTRY_OK);
    sword->fields.bl = literal;
    sword->phase = MC_ITEM_RAW;
    CHECK(mc_item_render_name(r, sword, NULL, w, &a, &e) && a == literal && !e.damage_read);
    sword->fields.bl = NULL;
    CHECK(mc_item_render_name(r, sword, NULL, w, &a, &e) && a == NULL && e.returned);
    sword->fields.bl = old_name;
    sword->phase = phase;
    const mc_item *dirt = item_at(f, 3);
    CHECK(mc_item_render_name(r, dirt, NULL, w, &a, &e) && a == r->blocks.m[3]->fields.a &&
          !e.damage_read);
    mc_item_stack *stack = NULL;
    CHECK(stack_at(263, 1, &stack) && mc_item_render_name(r, item_at(f, 263), stack, w, &a, &e) &&
          equal_ascii(a, "item.charcoal") &&
          mc_item_render_name(r, item_at(f, 263), stack, w, &b, &e) && a == b);
    mc_item_stack_release(stack);
    CHECK(stack_at(35, 0, &stack) && mc_item_render_name(r, item_at(f, 35), stack, w, &a, &e) &&
          mc_item_render_name(r, item_at(f, 35), stack, w, &b, &e) && a != b &&
          equal_ascii(a, "tile.cloth.white") && equal_ascii(b, "tile.cloth.white") &&
          !mc_item_name_workspace_destroy(w) && mc_item_name_workspace_reset(w));
    mc_item_stack_release(stack);
    const mc_registry_string **saved = r->items.km_a;
    r->items.km_a = NULL;
    CHECK(!mc_item_render_name(r, item_at(f, 351), NULL, w, &a, &e) && e.source_exception &&
          e.site.pc == 23 && strcmp(e.exception_class, "java/lang/NullPointerException") == 0);
    CHECK(stack_at(351, -1, &stack));
    CHECK(!mc_item_render_name(r, item_at(f, 351), stack, w, &a, &e) && e.source_exception &&
          e.site.pc == 26 && strcmp(e.exception_class, "java/lang/NullPointerException") == 0);
    r->items.km_a = saved;
    CHECK(!mc_item_render_name(r, item_at(f, 351), stack, w, &a, &e) && e.source_exception &&
          e.site.pc == 26 &&
          strcmp(e.exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0);
    mc_item_stack_release(stack);
    CHECK(mc_item_name_workspace_reset(w));
    return true;
}

static bool tooltip_lifetime(fixture *f, mc_item_name_workspace *w) {
    mc_registry_classes *r = f->app.registry;
    mc_item_render_error e;
    const mc_translation *captured;
    const mc_registry_string *raw, *result;
    CHECK(mc_item_name_translation_get(r, &captured, &e) &&
          captured == r->translations.singleton_a);
    CHECK(mc_registry_literal_ascii(r, "missing-tooltip-key", &raw) == MC_REGISTRY_OK);
    CHECK(mc_item_name_tooltip_translate(r, captured, raw, w, &result, &e) &&
          equal_ascii(result, "") && e.returned && mc_item_name_workspace_reset(w));
    mc_item_stack *stack = NULL;
    CHECK(stack_at(35, 0, &stack));
    CHECK(mc_item_stack_render_name(r, stack, w, &raw, &e) &&
          mc_item_name_tooltip_translate(r, captured, raw, w, &result, &e) &&
          mc_item_name_workspace_reset(w));
    const size_t registry_bytes = r->allocation_bytes, workspace_bytes = w->bytes;
    for (size_t i = 0; i < 1000; ++i) {
        CHECK(mc_item_stack_render_name(r, stack, w, &raw, &e) &&
              mc_item_name_tooltip_translate(r, captured, raw, w, &result, &e) &&
              mc_item_name_workspace_reset(w));
    }
    CHECK(r->allocation_bytes == registry_bytes && w->bytes == workspace_bytes);
    mc_item_stack_release(stack);
    CHECK(!mc_item_name_tooltip_translate(r, NULL, raw, w, &result, &e) && e.source_exception);
    mc_item_name_workspace small = {0};
    CHECK(!mc_item_name_workspace_init(&small, 1) && mc_item_name_workspace_init(&small, 4096));
    small.limit = small.bytes;
    CHECK(stack_at(35, 0, &stack));
    result = (const mc_registry_string *)(uintptr_t)1;
    CHECK(!mc_item_stack_render_name(r, stack, &small, &result, &e) &&
          e.status == MC_REGISTRY_LIMIT && e.native_policy && !e.source_exception &&
          result == (const mc_registry_string *)(uintptr_t)1 &&
          mc_item_name_workspace_reset(&small) && mc_item_name_workspace_destroy(&small));
    mc_item_stack_release(stack);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    mc_item_name_workspace workspace = {0};
    if (f == NULL || !mc_item_name_workspace_init(&workspace, 65536) ||
        !setup(f, argv[1], "item-name", true) || !observations(f, &workspace) ||
        !identities_and_order(f, &workspace) || !tooltip_lifetime(f, &workspace) ||
        !mc_item_name_workspace_reset(&workspace) || !mc_item_name_workspace_destroy(&workspace) ||
        !teardown(f))
        return 1;
    free(f);
    printf("PASS: %zu Item name/tooltip/temporary-lifetime checks; 404 original scalar cases\n",
           checks);
    return 0;
}
