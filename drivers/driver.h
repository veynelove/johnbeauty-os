#ifndef __DRIVER_H
#define __DRIVER_H

#include <common/types.h>

typedef struct jlos_driver jlos_driver_t;
typedef struct jlos_driver_manager jlos_driver_manager_t;

struct jlos_driver {
    void (*activate)(jlos_driver_t* self);
};

struct jlos_driver_manager {
    int num_drivers;
    jlos_driver_t *drivers[255];
};

void jlos_driver_init(jlos_driver_t* self);
void jlos_driver_activate(jlos_driver_t* self);

void jlos_driver_manager_init(jlos_driver_manager_t* self);
void jlos_driver_manager_add_driver(jlos_driver_manager_t* self, jlos_driver_t *drv);
void jlos_driver_manager_activate_all(jlos_driver_manager_t* self);

#endif