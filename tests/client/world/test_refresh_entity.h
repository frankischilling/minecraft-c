#ifndef MC_TEST_CLIENT_WORLD_REFRESH_ENTITY_H
#define MC_TEST_CLIENT_WORLD_REFRESH_ENTITY_H
typedef struct client_application client_application;
/* Caller supplies the genuine loaded single-player graph with a radius2
 * square of loaded/saved Chunks around its Player. It remains live/exclusive. */
int mc_test_world_refresh_entity(client_application *);
#endif
