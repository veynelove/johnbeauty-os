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
#include <net/ipv4.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <filesystem/msdospath.h>
#include <filesystem/fat.h>
#include <common/multiboot.h>
#include <kernel/gdt.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/syscalls.h>

// #define GRAPHICSMODE

namespace JLOS {
namespace Kernel {
void printf(const char* str)
{
    static uint16_t*  video_memory = (uint16_t *)0xb8000;
    static uint8_t m_x = 0, m_y = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        switch (str[i]) {
            case '\n': m_y++; m_x = 0; break;
            default:
                video_memory[80 * m_y + m_x] = (video_memory[80 * m_y + m_x] & 0xFF00) | str[i];
                m_x++;
        }
        if (m_x >= 80) {
            m_y++;
            m_x = 0;
        }
        if (m_y >= 25) {
            for (m_y = 0; m_y < 25; m_y++) {
                for (m_x = 0; m_x < 80; m_x++) {
                    video_memory[80 * m_y + m_x] = (video_memory[80 * m_y + m_x] & 0xFF00) | ' ';
                }
            }
            m_x = 0;
            m_y = 0;
        }
    }
}

void printf_hex(uint8_t key)
{
    char *foo = "00";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0x0F];
    foo[1] = hex[key & 0x0F];
    printf(foo);
}

class printf_keyboard_event_handler : public Drivers::keyboard_event_handler {
public:
    void key_down(char c) {
        char *foo = " ";
        foo[0] = c;
        printf(foo);
    }
};

class mouse_console : public Drivers::mouse_event_handler {
private:
    int8_t m_x, m_y;

public:
    mouse_console() {
        uint16_t *video_memory = (uint16_t *)0xb8000;
        m_x = 40;
        m_y = 12;
        video_memory[80 * m_y + m_x] = ((video_memory[80 * m_y + m_x] & 0xF000) >> 4)
            | ((video_memory[80 * m_y + m_x] & 0x0F00) << 4)
            | ((video_memory[80 * m_y + m_x] & 0x00FF));
    }

    void mouse_move(int32_t xoffset, int32_t yoffset) {
        static uint16_t *video_memory = (uint16_t *)0xb8000;
        video_memory[80 * m_y + m_x] = ((video_memory[80 * m_y + m_x] & 0xF000) >> 4)
            | ((video_memory[80 * m_y + m_x] & 0x0F00) << 4)
            | ((video_memory[80 * m_y + m_x] & 0x00FF));

        m_x += xoffset;
        if (m_x < 0) m_x = 0;
        if (m_x >= 80) m_x = 79;
        m_y += yoffset;
        if (m_y < 0) m_y = 0;
        if (m_y >= 25) m_y = 24;

        video_memory[80 * m_y + m_x] = ((video_memory[80 * m_y + m_x] & 0xF000) >> 4)
            | ((video_memory[80 * m_y + m_x] & 0x0F00) << 4)
            | ((video_memory[80 * m_y + m_x] & 0x00FF));
    }
};

class printf_udp_handler : public Net::user_datagram_protocol_handler {
public:
    void handle_user_datagram_protocol_message(Net::user_datagram_protocol_socket *socket,
          uint8_t *m_data, uint16_t m_size) {
        char *foo = " ";
        for (int i = 0; i < m_size; i++) {
            foo[0] = m_data[i];
            printf(foo);
        }
    }
};

class printf_tcp_handler : public Net::transmission_control_protocol_handler {
public:
    bool handle_transmission_control_protocol_message(Net::transmission_control_protocol_socket *socket,
          uint8_t *m_data, uint16_t m_size) {
        char *foo = " ";
        for (int i = 0; i < m_size; i++) {
            foo[0] = m_data[i];
            printf(foo);
        }
        if (m_size > 9 
            && m_data[0] == 'G' && m_data[1] == 'E'
            && m_data[2] == 'T' && m_data[3] == ' '
            && m_data[4] == '/' && m_data[5] == ' '
            && m_data[6] == 'H' && m_data[7] == 'T'
            && m_data[8] == 'T' && m_data[9] == 'P') {
            socket->send((uint8_t *)"HTTP/1.1 200 OK\r\n_server: JLOS\r\n_content-m_type: text/html\r\n\r\n<html><head><title>john beauty</title></head><body><m_b>johnbeauty</m_b>john_love operating system</body></html>\r\n", 177);
            socket->disconnect();
        }
        return true;
    }
};

void sysprintf(char *str)
{
    asm("int $0x80" : : "a" (4), "b" (str));
}

void task_a()
{
    while(true) {
        sysprintf("A");
    }
}

void task_b()
{
    while(true) {
        sysprintf("B");
    }
}

typedef void (*constructor)();
extern "C" constructor __init_array_start;
extern "C" constructor __init_array_end;

extern "C" void call_constructors()
{
    for (constructor* i = &__init_array_start; i != &__init_array_end; i++) {
        (*i)();
    }
}

extern "C" void memory_manager_test(const void *multiboot_structure)
{
    uint32_t *memupper = (uint32_t *)((size_t)multiboot_structure + 8);
    uint8_t heap = HEAP_STACT;
    memory_manager memory_manager_(&heap, (*memupper) * 1024 - heap - 10*1024);
    printf("heap: 0x");
    printf_hex((heap >> 24) & 0xFF);
    printf_hex((heap >> 16) & 0xFF);
    printf_hex((heap >> 8) & 0xFF);
    printf_hex(heap & 0xFF);
    void *m_allocated = memory_manager_.malloc(1024);
    printf("\nallocated: 0x");
    printf_hex(((size_t)m_allocated >> 24) & 0xFF);
    printf_hex(((size_t)m_allocated >> 16) & 0xFF);
    printf_hex(((size_t)m_allocated >> 8) & 0xFF);
    printf_hex((size_t)m_allocated & 0xFF);
    printf("\n");
}

extern "C" void multitask_test(global_descriptor_table gdt, task_manager task_manager_)
{
    task task1(&gdt, task_a);
    task task2(&gdt, task_b);
    task_manager_.add_task(&task1);
    task_manager_.add_task(&task2);
}

extern "C" void hard_driver_test()
{
    //m_interrupt 14
    Drivers::advanced_technolog_attachment ata0m(0x1F0, true);
    printf("ATA primary m_master: ");
    ata0m.identify();
    Drivers::advanced_technolog_attachment ata0s(0x1F0, false);
    printf("ATA primary m_master: ");
    ata0s.identify();
    printf("\n\n\n\n\n\n\n\n\n\n");
    FileSystem::msdos_partition_table::read_partitions(&ata0s);
    //char *atabuffer = "https://www.baidu.com";
    //ata0s.write28(0, (uint8_t *)atabuffer, 22);
    //ata0s.flush();

    //ata0s.read28(0, (uint8_t *)atabuffer, 22);
    //m_interrupt 15
    Drivers::advanced_technolog_attachment ata1m(0x170, true);
    Drivers::advanced_technolog_attachment ata1s(0x170, false);
    //third: 0x1E8
    //fourth: 0x168
}

extern "C" void vga_test(Gui::desktop desktop)
{
    Drivers::video_graphics_array vga;
    vga.set_mode(320, 200, 8);
    Gui::window win1(&desktop, 10, 10, 20, 20, 0xA8, 0x00, 0x00);
    desktop.add_child(&win1);
    Gui::window win2(&desktop, 40, 15, 30, 30, 0x00, 0xA8, 0x00);
    desktop.add_child(&win2);
}

extern "C" void john_beauty_main(const multiboot_info &multiboot_structure, uint32_t m_magicnumber)
{
    printf("princess yihan is safe and happy!\n");

    global_descriptor_table gdt;
    memory_manager memory_manager_((uint8_t*)(1024*(multiboot_structure.mem_upper - 1024*16)), 1024*1024*16);
    task_manager task_manager_;
	Hdc::interrupt_manager interrupts(0x20, &gdt, &task_manager_);
    syscall_handler syscalls(&interrupts, 0x80);

    printf("initializing hardware, stage 1.\n");
    Drivers::driver_manager driver_manager_;
#ifdef GRAPHICSMODE
    Gui::desktop desktop_(320, 200, 0x00, 0x00, 0xA8);    
    Drivers::keyboard_driver keyboard(&interrupts, &desktop);
    Drivers::mouse_driver mouse(&interrupts, &desktop);
#else 
    printf_keyboard_event_handler kbhandler;
    Drivers::keyboard_driver keyboard(&interrupts, &kbhandler);
    mouse_console mouse_handler_;
    Drivers::mouse_driver mouse(&interrupts, &mouse_handler_);
#endif
    driver_manager_.add_driver(&mouse);
    driver_manager_.add_driver(&keyboard);
    Hdc::peripheral_component_interconnect_controller pci_controller;
    pci_controller.select_drivers(&driver_manager_, &interrupts);

    printf("initializing hardware, stage 2.\n");
    driver_manager_.activate_all();
#ifdef GRAPHICSMODE
    vga_test(desktop);
#endif
    printf("initializing hardware, stage 3.\n");
    Drivers::amd_am79c973 *eth0 = (Drivers::amd_am79c973 *)(driver_manager_.drivers[2]);
    // IP m_address
    uint32_t ip_be = BYTES_TO_BE32(103, 0, 168, 192);
    eth0->set_ip_address(ip_be);
    Net::ether_frame_provider etherframe(eth0);
    Net::address_resolution_protocol arp(&etherframe);

    // IP m_address of the default gateway
    uint32_t gip_be = BYTES_TO_BE32(1, 0, 168, 192);

    uint32_t subnet_be = BYTES_TO_BE32(0, 255, 255, 255);
    Net::internet_protocol_provider ipv4(&etherframe, &arp, gip_be, subnet_be);
    Net::internet_control_message_protocol icmp(&ipv4);
    Net::user_datagram_protocol_provider udp(&ipv4);
    Net::transmission_control_protocol_provider tcp(&ipv4);

    interrupts.activate(); //激活中断保证在最后执行

    printf("\n\n");
    arp.broadcast_mac_address(gip_be);
    printf_tcp_handler tcphandler;
    Net::transmission_control_protocol_socket *tcpsocket = tcp.listen(1234);
    tcp.bind(tcpsocket, &tcphandler);
    //tcpsocket->send((uint8_t *)"hello TCP!", 10);

    //icmp.request_echo_reply(gip_be);
    //printf_udp_handler udphandler;

    // Net::user_datagram_protocol_socket *udpsocket = udp.connect(gip_be, 1234);
    // udp.bind(udpsocket, &udphandler);
    // udpsocket->send((uint8_t *)"hello UDP!", 10);
    //Net::user_datagram_protocol_socket *udpsocket = udp.listen(1234);
    //udp.bind(udpsocket, &udphandler);

    while (1) {
#ifdef GRAPHICSMODE
        desktop.draw(&vga);
#endif
    }
}
}
}
