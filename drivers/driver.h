#ifndef __DRIVER_H
#define __DRIVER_H

namespace JLOS {
namespace Drivers {
class Driver {
public:
    Driver();
    ~Driver();

    virtual void Activate();
    virtual int Reset();
    virtual void Deactivate();
};

class DriverManager {
private:   
    int numDrivers;

public:
    Driver *drivers[255]; // set public not a good idea;

public:
    DriverManager();
    void AddDriver(Driver *);
    void ActivateAll();
};
}
}
#endif
