#ifndef MC_ENTITY_MINECART_SITES_H
#define MC_ENTITY_MINECART_SITES_H

#include <stdbool.h>
#include <stdint.h>

/* A concrete Minecart method selection. selected and entered are explicit:
 * PC0 alone identifies neither a neutral record nor an entered method. */
typedef struct mc_minecart_site {
    bool selected, entered;
    const char *declaring_class, *method;
    uint16_t bytecode;
} mc_minecart_site;

#endif
