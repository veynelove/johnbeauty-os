#include <drivers/driver.h>

void jlos_driver_init(jlos_driver_t* self)
{
    self->activate = jlos_driver_activate;
    self->reset = jlos_driver_reset;
    self->deactivate = jlos_driver_deactivate;
}

void jlos_driver_destroy(jlos_driver_t* self)
{
}

void jlos_driver_activate(jlos_driver_t* self)
{
}

int jlos_driver_reset(jlos_driver_t* self)
{
    return 0;
}

void jlos_driver_deactivate(jlos_driver_t* self)
{
}

void jlos_driver_manager_init(jlos_driver_manager_t* self)
{
    self->num_drivers = 0;
}

void jlos_driver_manager_add_driver(jlos_driver_manager_t* self, jlos_driver_t *drv)
{
    self->drivers[self->num_drivers] = drv;
    self->num_drivers++;
}

void jlos_driver_manager_activate_all(jlos_driver_manager_t* self)
{
    for (int i = 0; i < self->num_drivers; i++) {
        self->drivers[i]->activate(self->drivers[i]);
    }
}