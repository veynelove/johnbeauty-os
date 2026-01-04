#include <hdc/interrupts.h>
#include <hdc/pci.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <drivers/amd_am79c973.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <net/etherframe.h>
#include <net/arp.h>
#include <kernel/gdt.h>
#include <kernel/multitasking.h>
#include <kernel/memorymanagerment.h>
#include <kernel/syscalls.h>

// #define GRAPHICSMODE

namespace JLOS {
namespace Kernel {
void printf(const char* str)
{
    static uint16_t*  VideoMemory = (uint16_t*)0xb8000;
    static uint8_t x = 0, y = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        switch (str[i]) {
            case '\n': y++; x = 0; break;
            default:
                VideoMemory[80 * y + x] = (VideoMemory[80 * y + x] & 0xFF00) | str[i];
                x++; break;
        }
        if (x >= 80) {
            y++;
            x = 0;
        }
        if (y >= 25) {
            for (y = 0; y < 25; y++) {
                for (x = 0; x < 80; x++) {
                    VideoMemory[80 * y + x] = (VideoMemory[80 * y + x] & 0xFF00) | ' ';
                }
            }
            x = 0;
            y = 0;
        }
    }
}

void printfHex(uint8_t key)
{
    char *foo = "00";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0x0F];
    foo[1] = hex[key & 0x0F];
    printf(foo);
}

class PrintfKeyboardEventHandler : public JLOS::Drivers::KeyboardEventHandler {
public:
    void OnKeyDown(char c) {
        char *foo = " ";
        foo[0] = c;
        printf(foo);
    }
};

class MouseToConsole : public JLOS::Drivers::MouseEventHandler {
private:
    int8_t x, y;

public:
    MouseToConsole() {
        uint16_t *VideoMemory = (uint16_t *)0xb8000;
        x = 40;
        y = 12;
        VideoMemory[80 * y + x] = ((VideoMemory[80 * y + x] & 0xF000) >> 4)
            | ((VideoMemory[80 * y + x] & 0x0F00) << 4)
            | ((VideoMemory[80 * y + x] & 0x00FF));
    }

    void OnMouseMove(int32_t xoffset, int32_t yoffset) {
        static uint16_t *VideoMemory = (uint16_t *)0xb8000;
        VideoMemory[80 * y + x] = ((VideoMemory[80 * y + x] & 0xF000) >> 4)
            | ((VideoMemory[80 * y + x] & 0x0F00) << 4)
            | ((VideoMemory[80 * y + x] & 0x00FF));

        x += xoffset;
        if (x < 0) x = 0;
        if (x >= 80) x = 79;
        y += yoffset;
        if (y < 0) y = 0;
        if (y >= 25) y = 24;

        VideoMemory[80 * y + x] = ((VideoMemory[80 * y + x] & 0xF000) >> 4)
            | ((VideoMemory[80 * y + x] & 0x0F00) << 4)
            | ((VideoMemory[80 * y + x] & 0x00FF));
    }
};

void sysprintf(char *str)
{
    asm("int $0x80" : : "a" (4), "b" (str));
}

void taskA()
{
    while(true) {
        sysprintf("A");
    }
}

void taskB()
{
    while(true) {
        sysprintf("B");
    }
}

typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

extern "C" void callConstructors()
{
    for (constructor* i = &start_ctors; i != &end_ctors; i++) {
        (*i)();
    }
}

extern "C" void memory_manager(const void *multiboot_structure)
{
    uint32_t *memupper = (uint32_t *)((size_t)multiboot_structure + 8);
    size_t heap = 10*1024*1024;
    MemoryManager memoryManager(heap, (*memupper) * 1024 - heap - 10*1024);
    printf("heap: 0x");
    printfHex((heap >> 24) & 0xFF);
    printfHex((heap >> 16) & 0xFF);
    printfHex((heap >> 8) & 0xFF);
    printfHex(heap & 0xFF);
    void *allocated = memoryManager.malloc(1024);
    printf("\nallocated: 0x");
    printfHex(((size_t)allocated >> 24) & 0xFF);
    printfHex(((size_t)allocated >> 16) & 0xFF);
    printfHex(((size_t)allocated >> 8) & 0xFF);
    printfHex((size_t)allocated & 0xFF);
    printf("\n");
}

extern "C" void multi_task_test(GlobalDescriptorTable gdt, TaskManager taskManager)
{
    Task task1(&gdt, taskA);
    Task task2(&gdt, taskB);
    taskManager.AddTask(&task1);
    taskManager.AddTask(&task2);
}

extern "C" void hard_driver_test()
{
    //interrupt 14
    Drivers::AdvancedTechnologAttachment ata0m(0x1F0, true);
    printf("ATA Primary Master: ");
    ata0m.Identify();
    Drivers::AdvancedTechnologAttachment ata0s(0x1F0, false);
    printf("ATA Primary Master: ");
    ata0s.Identify();
    char *atabuffer = "https://www.baidu.com";
    ata0s.Write28(0, (uint8_t *)atabuffer, 22);
    ata0s.Flush();

    ata0s.Read28(0, (uint8_t *)atabuffer, 22);
    //interrupt 15
    Drivers::AdvancedTechnologAttachment ata1m(0x170, true);
    Drivers::AdvancedTechnologAttachment ata1s(0x170, false);
    //third: 0x1E8
    //fourth: 0x168
}

extern "C" Net::AddressResolutionProtocol ethnet_test(Drivers::DriverManager drvManager)
{
    uint8_t ip1 = 10, ip2 = 0, ip3 = 2, ip4 = 15;
    uint32_t ip_be = (((uint32_t)ip4 << 24) | ((uint32_t)ip3 << 16)
                     | ((uint32_t)ip2 << 8) | (uint32_t)ip1);

    Drivers::amd_am79c973 *eth0 = (Drivers::amd_am79c973 *)(drvManager.drivers[2]);
    eth0->SetIPAddress(ip_be);
    Net::EtherFrameProvider etherframe(eth0);
    Net::AddressResolutionProtocol arp(&etherframe);
    return arp;
    //etherframe.Send(0xFFFFFFFFFFFF, 0x0608, (uint8_t *)"F00", 3);
    //eth0->Send((uint8_t *)"Hello NetWork", 13);
}

extern "C" void vga_test(Gui::Desktop desktop)
{
    Drivers::VideoGraphicsArray vga;
    vga.SetMode(320, 200, 8);
    Gui::Window win1(&desktop, 10, 10, 20, 20, 0xA8, 0x00, 0x00);
    desktop.AddChild(&win1);
    Gui::Window win2(&desktop, 40, 15, 30, 30, 0x00, 0xA8, 0x00);
    desktop.AddChild(&win2);
}

extern "C" void johnbeautyMain(void *multiboot_structure, uint32_t magicnumber)
{
    printf("Princess Yihan is safe and happy!\n");

    GlobalDescriptorTable gdt;
    memory_manager(multiboot_structure);
    TaskManager taskManager;
	Hdc::InterruptManager interrupts(0x20, &gdt, &taskManager);
    SyscallHandler syscalls(&interrupts, 0x80);

    printf("initializing Hardware, Stage 1.\n");
    Drivers::DriverManager drvManager;
#ifdef GRAPHICSMODE
    Gui::Desktop desktop(320, 200, 0x00, 0x00, 0xA8);    
    Drivers::KeyboardDriver keyboard(&interrupts, &desktop);
    Drivers::MouseDriver mouse(&interrupts, &desktop);
#else 
    PrintfKeyboardEventHandler kbhandler;
    Drivers::KeyboardDriver keyboard(&interrupts, &kbhandler);
    MouseToConsole mouseHandler;
    Drivers::MouseDriver mouse(&interrupts, &mouseHandler);
#endif
    drvManager.AddDriver(&mouse);
    drvManager.AddDriver(&keyboard);
    Hdc::PeripheralComponentInterconnectController PCIController;
    PCIController.SelectDrivers(&drvManager, &interrupts);

    printf("initializing Hardware, Stage 2.\n");
    drvManager.ActivateAll();
#ifdef GRAPHICSMODE
    vga_test(desktop);
#endif
    printf("initializing Hardware, Stage 3.\n");
    /** arp test
    Net::AddressResolutionProtocol arp = ethnet_test(drvManager);
    uint8_t gip1 = 10, gip2 = 0, gip3 = 2, gip4 = 2;
    uint32_t gip_be = (((uint32_t)gip4 << 24) | ((uint32_t)gip3 << 16)
                     | ((uint32_t)gip2 << 8) | (uint32_t)gip1);
    */
    interrupts.Activate(); //激活中断保证在最后执行
    printf("\n\n");
    //arp.Resolve(gip_be);
    while (1) {
#ifdef GRAPHICSMODE
        desktop.Draw(&vga);
#endif
    }
}
}
}
