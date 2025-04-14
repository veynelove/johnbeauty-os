#ifndef __DRIVER_H
#define __DRIVER_H

class Driver {
public:
    Driver();
    ~Driver();

    virtual void Activate();
    virtual int Reset();
    virtual void Deactivate();
};

class DriverManager {
public:
    DriverManager();
    void AddDriver(Driver *);
    void ActivateAll();
private:   
    Driver *drivers[255];
    int numDrivers;
};
#endif // __DRIVER_H
