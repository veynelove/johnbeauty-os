#ifndef __DRIVER_H
#define __DRIVER_H

namespace JLOS {
namespace Drivers {
class driver {
public:
    driver();
    ~driver();

    virtual void activate();
    virtual int reset();
    virtual void deactivate();
};

class driver_manager {
private:   
    int m_num_drivers;

public:
    driver *drivers[255]; // set public not a good idea;

public:
    driver_manager();
    void add_driver(driver *);
    void activate_all();
};
}
}
#endif
