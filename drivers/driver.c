#include <drivers/driver.h>

static jlos_driver_manager_t s_driver_manager;

jlos_driver_manager_t *g_driver_manager_ptr = &s_driver_manager;

void jlos_driver_init(jlos_driver_t* self)
{
    self->activate = jlos_driver_activate;
}

void jlos_driver_activate(jlos_driver_t* self)
{
    (void)self;
}

void jlos_driver_manager_init(void)
{
    s_driver_manager.num_drivers = 0;
}

void jlos_driver_manager_add_driver(jlos_driver_manager_t* self, jlos_driver_t *drv)
{
    self->drivers[self->num_drivers] = drv;
    self->num_drivers++;
}

void jlos_driver_manager_activate_all(void)
{
    for (int i = 0; i < g_driver_manager_ptr->num_drivers; i++) {
        g_driver_manager_ptr->drivers[i]->activate(g_driver_manager_ptr->drivers[i]);
    }
}