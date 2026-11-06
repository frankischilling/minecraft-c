#ifndef MC_TEST_CLIENT_WORLD_LOAD_FOREIGN_LOADING_H
#define MC_TEST_CLIENT_WORLD_LOAD_FOREIGN_LOADING_H
typedef struct client_application client_application;

/* Actual completed-App native control. App f/h/i are null;
 * caller owns the real writable Stats directory and entire startup graph. */
int mc_test_world_load_foreign_loading(client_application *);
#endif
