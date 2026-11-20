#include "internal.h"

static bool model_failure(mc_client_render_manager_classes *c, mc_client_managed_renderer *r,
                          int32_t th_pc) {
    c->failure.model = r->failure;
    c->failure.th_pc = th_pc;
    c->failure.renderer_pc = r->failure.source_pc;
    manager_fail(&c->failure, MC_CLIENT_MANAGER_MODEL_FAILED,
                 "actual TH renderer/model constructor", "th", "<init>", "()V", th_pc, true);
    return false;
}

bool manager_construct(mc_client_render_manager *m) {
    mc_client_render_manager_classes *c = m->classes;
    const mc_entity_class_literal *key = NULL;
    mc_client_managed_renderer *r = NULL;
    manager_model *first = NULL, *second = NULL;
    int32_t icon = 0;
    m->active = true;
    c->failure.th_pc = 5;
    m->map_candidate = manager_allocate(c, sizeof(*m->map_candidate), "th", 5);
    if (m->map_candidate == NULL)
        goto failed;
    /* HashMap()9 creates the real initially empty identity map. */
    m->map = m->map_candidate;
    m->fields.o = m->map;
    m->last_constructor_pc = 12;

    /* Actual TH row 1: cn -> yx; put28. */
    c->failure.th_pc = 19;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_SPIDER, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 19, true);
        goto failed;
    }
    c->failure.th_pc = 21;
    r = manager_renderer_allocate(m, "yx", 21);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 25;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 25);
        goto failed;
    }
    c->failure.th_pc = 28;
    if (!manager_map_put(m, key, r, 28))
        goto failed;
    /* Actual TH row 2: wh -> me; put65. */
    c->failure.th_pc = 38;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_PIG, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 38, true);
        goto failed;
    }
    c->failure.th_pc = 40;
    r = manager_renderer_allocate(m, "me", 40);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 44;
    first = manager_renderer_model(r, "eh", "()V", "th", 44, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 48);
        goto failed;
    }
    c->failure.th_pc = 51;
    second = manager_renderer_model(r, "eh", "(F)V", "th", 51, 0x1.0000000000000p-1f, 0);
    if (second == NULL) {
        (void)model_failure(c, r, 57);
        goto failed;
    }
    c->failure.th_pc = 62;
    if (!manager_renderer_construct(r, first, second, 0x1.6666660000000p-1f, 0.0f, 0)) {
        (void)model_failure(c, r, 62);
        goto failed;
    }
    c->failure.th_pc = 65;
    if (!manager_map_put(m, key, r, 65))
        goto failed;
    /* Actual TH row 3: dl -> xy; put100. */
    c->failure.th_pc = 75;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_SHEEP, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 75, true);
        goto failed;
    }
    c->failure.th_pc = 77;
    r = manager_renderer_allocate(m, "xy", 77);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 81;
    first = manager_renderer_model(r, "mw", "()V", "th", 81, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 85);
        goto failed;
    }
    c->failure.th_pc = 88;
    second = manager_renderer_model(r, "ea", "()V", "th", 88, 0.0f, 0);
    if (second == NULL) {
        (void)model_failure(c, r, 92);
        goto failed;
    }
    c->failure.th_pc = 97;
    if (!manager_renderer_construct(r, first, second, 0x1.6666660000000p-1f, 0.0f, 0)) {
        (void)model_failure(c, r, 97);
        goto failed;
    }
    c->failure.th_pc = 100;
    if (!manager_map_put(m, key, r, 100))
        goto failed;
    /* Actual TH row 4: bx -> va; put128. */
    c->failure.th_pc = 110;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_COW, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 110, true);
        goto failed;
    }
    c->failure.th_pc = 112;
    r = manager_renderer_allocate(m, "va", 112);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 116;
    first = manager_renderer_model(r, "hh", "()V", "th", 116, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 120);
        goto failed;
    }
    c->failure.th_pc = 125;
    if (!manager_renderer_construct(r, first, NULL, 0x1.6666660000000p-1f, 0.0f, 0)) {
        (void)model_failure(c, r, 125);
        goto failed;
    }
    c->failure.th_pc = 128;
    if (!manager_map_put(m, key, r, 128))
        goto failed;
    /* Actual TH row 5: gi -> we; put156. */
    c->failure.th_pc = 138;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_WOLF, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 138, true);
        goto failed;
    }
    c->failure.th_pc = 140;
    r = manager_renderer_allocate(m, "we", 140);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 144;
    first = manager_renderer_model(r, "o", "()V", "th", 144, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 148);
        goto failed;
    }
    c->failure.th_pc = 153;
    if (!manager_renderer_construct(r, first, NULL, 0x1.0000000000000p-1f, 0.0f, 0)) {
        (void)model_failure(c, r, 153);
        goto failed;
    }
    c->failure.th_pc = 156;
    if (!manager_map_put(m, key, r, 156))
        goto failed;
    /* Actual TH row 6: ww -> ip; put184. */
    c->failure.th_pc = 166;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_CHICKEN, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 166, true);
        goto failed;
    }
    c->failure.th_pc = 168;
    r = manager_renderer_allocate(m, "ip", 168);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 172;
    first = manager_renderer_model(r, "td", "()V", "th", 172, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 176);
        goto failed;
    }
    c->failure.th_pc = 181;
    if (!manager_renderer_construct(r, first, NULL, 0x1.3333340000000p-2f, 0.0f, 0)) {
        (void)model_failure(c, r, 181);
        goto failed;
    }
    c->failure.th_pc = 184;
    if (!manager_map_put(m, key, r, 184))
        goto failed;
    /* Actual TH row 7: gb -> m; put203. */
    c->failure.th_pc = 194;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_CREEPER, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 194, true);
        goto failed;
    }
    c->failure.th_pc = 196;
    r = manager_renderer_allocate(m, "m", 196);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 200;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 200);
        goto failed;
    }
    c->failure.th_pc = 203;
    if (!manager_map_put(m, key, r, 203))
        goto failed;
    /* Actual TH row 8: fr -> v; put231. */
    c->failure.th_pc = 213;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_SKELETON, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 213, true);
        goto failed;
    }
    c->failure.th_pc = 215;
    r = manager_renderer_allocate(m, "v", 215);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 219;
    first = manager_renderer_model(r, "lc", "()V", "th", 219, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 223);
        goto failed;
    }
    c->failure.th_pc = 228;
    if (!manager_renderer_construct(r, first, NULL, 0x1.0000000000000p-1f, 0.0f, 0)) {
        (void)model_failure(c, r, 228);
        goto failed;
    }
    c->failure.th_pc = 231;
    if (!manager_map_put(m, key, r, 231))
        goto failed;
    /* Actual TH row 9: uz -> v; put259. */
    c->failure.th_pc = 241;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_ZOMBIE, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 241, true);
        goto failed;
    }
    c->failure.th_pc = 243;
    r = manager_renderer_allocate(m, "v", 243);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 247;
    first = manager_renderer_model(r, "ej", "()V", "th", 247, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 251);
        goto failed;
    }
    c->failure.th_pc = 256;
    if (!manager_renderer_construct(r, first, NULL, 0x1.0000000000000p-1f, 0.0f, 0)) {
        (void)model_failure(c, r, 256);
        goto failed;
    }
    c->failure.th_pc = 259;
    if (!manager_map_put(m, key, r, 259))
        goto failed;
    /* Actual TH row 10: uw -> mj; put297. */
    c->failure.th_pc = 269;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_SLIME, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 269, true);
        goto failed;
    }
    c->failure.th_pc = 271;
    r = manager_renderer_allocate(m, "mj", 271);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 275;
    first = manager_renderer_model(r, "no", "(I)V", "th", 275, 0.0f, 16);
    if (first == NULL) {
        (void)model_failure(c, r, 281);
        goto failed;
    }
    c->failure.th_pc = 284;
    second = manager_renderer_model(r, "no", "(I)V", "th", 284, 0.0f, 0);
    if (second == NULL) {
        (void)model_failure(c, r, 289);
        goto failed;
    }
    c->failure.th_pc = 294;
    if (!manager_renderer_construct(r, first, second, 0x1.0000000000000p-2f, 0.0f, 0)) {
        (void)model_failure(c, r, 294);
        goto failed;
    }
    c->failure.th_pc = 297;
    if (!manager_map_put(m, key, r, 297))
        goto failed;
    /* Actual TH row 11: gs -> ds; put316. */
    c->failure.th_pc = 307;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_PLAYER, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 307, true);
        goto failed;
    }
    c->failure.th_pc = 309;
    if (!manager_ds_active_use(c))
        goto failed;
    r = manager_renderer_allocate(m, "ds", 309);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 313;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 313);
        goto failed;
    }
    c->failure.th_pc = 316;
    if (!manager_map_put(m, key, r, 316))
        goto failed;
    /* Actual TH row 12: nt -> yg; put346. */
    c->failure.th_pc = 326;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_GIANT, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 326, true);
        goto failed;
    }
    c->failure.th_pc = 328;
    r = manager_renderer_allocate(m, "yg", 328);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 332;
    first = manager_renderer_model(r, "ej", "()V", "th", 332, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 336);
        goto failed;
    }
    c->failure.th_pc = 343;
    if (!manager_renderer_construct(r, first, NULL, 0x1.0000000000000p-1f, 0x1.8000000000000p+2f,
                                    0)) {
        (void)model_failure(c, r, 343);
        goto failed;
    }
    c->failure.th_pc = 346;
    if (!manager_map_put(m, key, r, 346))
        goto failed;
    /* Actual TH row 13: bp -> pq; put365. */
    c->failure.th_pc = 356;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_GHAST, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 356, true);
        goto failed;
    }
    c->failure.th_pc = 358;
    r = manager_renderer_allocate(m, "pq", 358);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 362;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 362);
        goto failed;
    }
    c->failure.th_pc = 365;
    if (!manager_map_put(m, key, r, 365))
        goto failed;
    /* Actual TH row 14: xt -> es; put393. */
    c->failure.th_pc = 375;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_SQUID, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 375, true);
        goto failed;
    }
    c->failure.th_pc = 377;
    r = manager_renderer_allocate(m, "es", 377);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 381;
    first = manager_renderer_model(r, "wn", "()V", "th", 381, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 385);
        goto failed;
    }
    c->failure.th_pc = 390;
    if (!manager_renderer_construct(r, first, NULL, 0x1.6666660000000p-1f, 0.0f, 0)) {
        (void)model_failure(c, r, 390);
        goto failed;
    }
    c->failure.th_pc = 393;
    if (!manager_map_put(m, key, r, 393))
        goto failed;
    /* Actual TH row 15: ls -> gv; put421. */
    c->failure.th_pc = 403;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
                                    &key) != MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 403, true);
        goto failed;
    }
    c->failure.th_pc = 405;
    r = manager_renderer_allocate(m, "gv", 405);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 409;
    first = manager_renderer_model(r, "fh", "()V", "th", 409, 0.0f, 0);
    if (first == NULL) {
        (void)model_failure(c, r, 413);
        goto failed;
    }
    c->failure.th_pc = 418;
    if (!manager_renderer_construct(r, first, NULL, 0x1.0000000000000p-1f, 0.0f, 0)) {
        (void)model_failure(c, r, 418);
        goto failed;
    }
    c->failure.th_pc = 421;
    if (!manager_map_put(m, key, r, 421))
        goto failed;
    /* Actual TH row 16: sn -> mb; put440. */
    c->failure.th_pc = 431;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_ENTITY, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 431, true);
        goto failed;
    }
    c->failure.th_pc = 433;
    r = manager_renderer_allocate(m, "mb", 433);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 437;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 437);
        goto failed;
    }
    c->failure.th_pc = 440;
    if (!manager_map_put(m, key, r, 440))
        goto failed;
    /* Actual TH row 17: qv -> dy; put459. */
    c->failure.th_pc = 450;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_PAINTING, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 450, true);
        goto failed;
    }
    c->failure.th_pc = 452;
    r = manager_renderer_allocate(m, "dy", 452);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 456;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 456);
        goto failed;
    }
    c->failure.th_pc = 459;
    if (!manager_map_put(m, key, r, 459))
        goto failed;
    /* Actual TH row 18: sl -> mc; put478. */
    c->failure.th_pc = 469;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_ARROW, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 469, true);
        goto failed;
    }
    c->failure.th_pc = 471;
    r = manager_renderer_allocate(m, "mc", 471);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 475;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 475);
        goto failed;
    }
    c->failure.th_pc = 478;
    if (!manager_map_put(m, key, r, 478))
        goto failed;
    /* Actual TH row 19: by -> dg; put504. */
    c->failure.th_pc = 488;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_SNOWBALL, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 488, true);
        goto failed;
    }
    c->failure.th_pc = 490;
    r = manager_renderer_allocate(m, "dg", 490);
    if (r == NULL)
        goto failed;
    {
        const mc_item *item = NULL;
        c->failure.th_pc = 494;
        if (mc_registry_item_static_get(c->registry, "aB", &item, &c->failure.registry) !=
            MC_REGISTRY_OK) {
            manager_fail(&c->failure, MC_CLIENT_MANAGER_REGISTRY_FAILED,
                         "actual GM active-use/static Item argument", "th", "<init>", "()V", 494,
                         false);
            c->failure.registry_delegated = true;
            goto failed;
        }
        if (item == NULL) {
            manager_fail(&c->failure, MC_CLIENT_MANAGER_NULL_REFERENCE, "actual GM.a(0) receiver",
                         "th", "<init>", "()V", 498, false);
            goto failed;
        }
        icon = mc_item_fields_get(item)->bh; /* Actual GM.a(I)I: getfield bh1; return4. Native
                                                argument scratch, not DG.a publication. */
    }
    c->failure.th_pc = 501;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, icon)) {
        (void)model_failure(c, r, 501);
        goto failed;
    }
    c->failure.th_pc = 504;
    if (!manager_map_put(m, key, r, 504))
        goto failed;
    /* Actual TH row 20: vv -> dg; put530. */
    c->failure.th_pc = 514;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_VV, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 514, true);
        goto failed;
    }
    c->failure.th_pc = 516;
    r = manager_renderer_allocate(m, "dg", 516);
    if (r == NULL)
        goto failed;
    {
        const mc_item *item = NULL;
        c->failure.th_pc = 520;
        if (mc_registry_item_static_get(c->registry, "aN", &item, &c->failure.registry) !=
            MC_REGISTRY_OK) {
            manager_fail(&c->failure, MC_CLIENT_MANAGER_REGISTRY_FAILED,
                         "actual GM active-use/static Item argument", "th", "<init>", "()V", 520,
                         false);
            c->failure.registry_delegated = true;
            goto failed;
        }
        if (item == NULL) {
            manager_fail(&c->failure, MC_CLIENT_MANAGER_NULL_REFERENCE, "actual GM.a(0) receiver",
                         "th", "<init>", "()V", 524, false);
            goto failed;
        }
        icon = mc_item_fields_get(item)->bh; /* Actual GM.a(I)I: getfield bh1; return4. Native
                                                argument scratch, not DG.a publication. */
    }
    c->failure.th_pc = 527;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, icon)) {
        (void)model_failure(c, r, 527);
        goto failed;
    }
    c->failure.th_pc = 530;
    if (!manager_map_put(m, key, r, 530))
        goto failed;
    /* Actual TH row 21: cf -> kl; put549. */
    c->failure.th_pc = 540;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_CF, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 540, true);
        goto failed;
    }
    c->failure.th_pc = 542;
    r = manager_renderer_allocate(m, "kl", 542);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 546;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 546);
        goto failed;
    }
    c->failure.th_pc = 549;
    if (!manager_map_put(m, key, r, 549))
        goto failed;
    /* Actual TH row 22: hl -> bb; put568. */
    c->failure.th_pc = 559;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_ITEM, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 559, true);
        goto failed;
    }
    c->failure.th_pc = 561;
    r = manager_renderer_allocate(m, "bb", 561);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 565;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 565);
        goto failed;
    }
    c->failure.th_pc = 568;
    if (!manager_map_put(m, key, r, 568))
        goto failed;
    /* Actual TH row 23: qw -> on; put587. */
    c->failure.th_pc = 578;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_PRIMED_TNT, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 578, true);
        goto failed;
    }
    c->failure.th_pc = 580;
    r = manager_renderer_allocate(m, "on", 580);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 584;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 584);
        goto failed;
    }
    c->failure.th_pc = 587;
    if (!manager_map_put(m, key, r, 587))
        goto failed;
    /* Actual TH row 24: ju -> gn; put606. */
    c->failure.th_pc = 597;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_FALLING_SAND, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 597, true);
        goto failed;
    }
    c->failure.th_pc = 599;
    r = manager_renderer_allocate(m, "gn", 599);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 603;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 603);
        goto failed;
    }
    c->failure.th_pc = 606;
    if (!manager_map_put(m, key, r, 606))
        goto failed;
    /* Actual TH row 25: yl -> tb; put625. */
    c->failure.th_pc = 616;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_MINECART, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 616, true);
        goto failed;
    }
    c->failure.th_pc = 618;
    r = manager_renderer_allocate(m, "tb", 618);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 622;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 622);
        goto failed;
    }
    c->failure.th_pc = 625;
    if (!manager_map_put(m, key, r, 625))
        goto failed;
    /* Actual TH row 26: fz -> fe; put644. */
    c->failure.th_pc = 635;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_BOAT, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 635, true);
        goto failed;
    }
    c->failure.th_pc = 637;
    r = manager_renderer_allocate(m, "fe", 637);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 641;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 641);
        goto failed;
    }
    c->failure.th_pc = 644;
    if (!manager_map_put(m, key, r, 644))
        goto failed;
    /* Actual TH row 27: lx -> pl; put663. */
    c->failure.th_pc = 654;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_LX, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 654, true);
        goto failed;
    }
    c->failure.th_pc = 656;
    r = manager_renderer_allocate(m, "pl", 656);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 660;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 660);
        goto failed;
    }
    c->failure.th_pc = 663;
    if (!manager_map_put(m, key, r, 663))
        goto failed;
    /* Actual TH row 28: c -> pi; put682. */
    c->failure.th_pc = 673;
    if (mc_entity_class_literal_get(c->models->entities, MC_ENTITY_CLASS_KIND_C, &key) !=
        MC_ENTITY_OK) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "actual passive Class literal", "th", "<init>", "()V", 673, true);
        goto failed;
    }
    c->failure.th_pc = 675;
    r = manager_renderer_allocate(m, "pi", 675);
    if (r == NULL)
        goto failed;
    c->failure.th_pc = 679;
    if (!manager_renderer_construct(r, NULL, NULL, 0.0f, 0.0f, 0)) {
        (void)model_failure(c, r, 679);
        goto failed;
    }
    c->failure.th_pc = 682;
    if (!manager_map_put(m, key, r, 682))
        goto failed;
    /* Actual values692/iterator697 and BW.a(TH)724 loop. The concrete native
     * HashMap traversal has no claimed JDK bucket order. No roster class
     * overrides the six-byte setter, which only stores this SAME TH at BW.b2. */
    for (size_t bucket = 0; bucket < m->map->capacity; ++bucket) {
        for (manager_map_entry *entry = m->map->buckets[bucket]; entry != NULL;
             entry = entry->next) {
            r = entry->owner;
            if (r->bb != NULL) {
                c->failure.model.status = mc_client_item_renderer_bind_manager(r->bb, m);
                if (c->failure.model.status != MC_CLIENT_MODEL_OK) {
                    manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_PHASE,
                                 "actual BW setter admission", "th", "<init>", "()V", 724, true);
                    goto failed;
                }
            } else
                mc_client_model_graph_bw_set_manager(&r->manager, m);
            ++m->bw_count;
            m->last_constructor_pc = 724;
        }
    }
    m->last_constructor_pc = 730;
    m->active = false;
    return true;
failed:
    m->active = false;
    return false;
}
