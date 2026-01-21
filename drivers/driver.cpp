#include <drivers/driver.h>

namespace JLOS {
namespace Drivers {
driver::driver(){}

driver::~driver(){}
    
void driver::activate(){}

int driver::reset()
{
    return 0;
}

void driver::deactivate(){}
 
driver_manager::driver_manager()
{
    m_num_drivers = 0;
}

void driver_manager::add_driver(driver * drv)
{
    drivers[m_num_drivers] = drv;
    m_num_drivers++;
}

void driver_manager::activate_all()
{
    for (int i = 0; i < m_num_drivers; i++) {
        drivers[i]->activate();
    }
}
}
}
