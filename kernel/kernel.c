#include <hdc/interrupts.h>
#include <hdc/pci.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <drivers/amd_am79c973.h>
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
#include <tools/config.h>

#if CONFIG_ENABLE_TESTS
#include <tools/tests/memory_te.h>
#include <tools/tests/multitask_te.h>
#include <tools/tests/hard_driver_te.h>
#endif

#if CONFIG_DEBUG_CONSOLE
#include <tools/samples/debug_console.h>
#endif

// I/O 端口操作函数
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %1, %0" : : "dN"(port), "a"(value));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

// 串口初始化和输出
void serial_init()
{
    // COM1 端口地址: 0x3F8
    outb(0x3F8 + 1, 0x00);    // 禁用中断
    outb(0x3F8 + 3, 0x80);    // 启用 DLAB
    outb(0x3F8 + 0, 0x03);    // 设置波特率低位 (38400 baud)
    outb(0x3F8 + 1, 0x00);    // 设置波特率高位
    outb(0x3F8 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(0x3F8 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(0x3F8 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void serial_putc(char c)
{
    // 等待传输缓冲区为空
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

void serial_puts(const char* str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        serial_putc(str[i]);
    }
}

void printf_scroll_screen()
{
    static uint16_t* video_memory = (uint16_t *)0xb8000;
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 80; x++) {
            video_memory[80 * y + x] = video_memory[80 * (y + 1) + x];
        }
    }
    for (int x = 0; x < 80; x++) {
        video_memory[80 * 24 + x] = (video_memory[80 * 24 + x] & 0xFF00) | ' ';
    }
}

void printf(const char* str)
{
    serial_puts(str);
    
    static uint16_t* video_memory = (uint16_t *)0xb8000;
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
            printf_scroll_screen();
            m_y = 24;
            m_x = 0;
        }
    }
}

void printf_hex(uint8_t key)
{
    char foo[3] = "00";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0x0F];
    foo[1] = hex[key & 0x0F];
    printf(foo);
}

void printf_hex32(uint32_t value)
{
    char foo[9] = "00000000";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(value >> 28) & 0x0F];
    foo[1] = hex[(value >> 24) & 0x0F];
    foo[2] = hex[(value >> 20) & 0x0F];
    foo[3] = hex[(value >> 16) & 0x0F];
    foo[4] = hex[(value >> 12) & 0x0F];
    foo[5] = hex[(value >> 8) & 0x0F];
    foo[6] = hex[(value >> 4) & 0x0F];
    foo[7] = hex[value & 0x0F];
    printf(foo);
}

void printf_char(char c) {
    char buf[2] = {c, '\0'};
    printf(buf);
}

typedef struct {
    jlos_udp_handler_t base;
} printf_udp_handler_t;

static void printf_udp_handler_handle_udp_message(jlos_udp_handler_t* self, jlos_udp_socket_t* socket, uint8_t *m_data, uint16_t m_size)
{
    char foo[2] = " ";
    for (int i = 0; i < m_size; i++) {
        foo[0] = m_data[i];
        printf(foo);
    }
}

typedef struct {
    jlos_tcp_handler_t base;
} printf_tcp_handler_t;

static bool printf_tcp_handler_handle_tcp_message(jlos_tcp_handler_t* self, jlos_tcp_socket_t* socket, uint8_t *m_data, uint16_t m_size)
{
    char foo[2] = " ";
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
        socket->send(socket, (uint8_t *)"HTTP/1.1 200 OK\r\n_server: JLOS\r\n_content-m_type: text/html\r\n\r\n<html><head><title>john beauty</title></head><body><m_b>johnbeauty</m_b>john_love operating system</body></html>\r\n", 177);
        socket->disconnect(socket);
    }
    return true;
}

void sysprintf(char *str)
{
    __asm__ __volatile__("int $0x80" : : "a" (4), "b" (str));
}

typedef void (*constructor)();
extern constructor __init_array_start;
extern constructor __init_array_end;

void call_constructors()
{
    for (constructor* i = &__init_array_start; i != &__init_array_end; i++) {
        (*i)();
    }
}

void john_beauty_main(const multiboot_info_t *multiboot_structure, uint32_t m_magicnumber)
{
    serial_init();  // 初始化串口输出
    printf("princess yihan is safe and happy!\n");

    jlos_gdt_t gdt;
    jlos_gdt_init(&gdt);
    
    uint8_t* low_memory_heap = (uint8_t*)(0x50000);
    jlos_memory_manager_t low_memory_manager_;
    jlos_memory_manager_init(&low_memory_manager_, low_memory_heap, 0x50000);
    
    uint8_t* heap_start = (uint8_t*)(1024 * (multiboot_structure->mem_upper - 1024 * 16));
    jlos_memory_manager_t memory_manager_;
    jlos_memory_manager_init(&memory_manager_, heap_start, 1024 * 1024 * 16);
    
    jlos_task_manager_t task_manager_;
    jlos_task_manager_init(&task_manager_);
    
    jlos_interrupt_manager_t interrupts;
    jlos_interrupt_manager_init(&interrupts, 0x20, &gdt, &task_manager_);
    
    jlos_syscall_handler_t syscalls;
    jlos_syscall_handler_init(&syscalls, &interrupts, 0x80);

    printf("initializing hardware, stage 1.\n");
    jlos_driver_manager_t driver_manager_;
    jlos_driver_manager_init(&driver_manager_);

#if CONFIG_DEBUG_CONSOLE
    debug_console_init(&interrupts, &driver_manager_);
#endif

    jlos_pci_controller_t pci_controller;
    jlos_pci_controller_init(&pci_controller);
    
    jlos_memory_manager_t *old_manager = jlos_active_memory_manager;
    jlos_active_memory_manager = &low_memory_manager_;
    printf("switched to low memory manager for PCI driver allocation\n");
    jlos_pci_controller_select_drivers(&pci_controller, &driver_manager_, &interrupts);
    jlos_active_memory_manager = old_manager;
    printf("switched back to main memory manager\n");

    printf("initializing hardware, stage 2.\n");
    jlos_driver_manager_activate_all(&driver_manager_);

    printf("initializing hardware, stage 3.\n");
    jlos_amd_am79c973_t *eth0 = (jlos_amd_am79c973_t *)(driver_manager_.drivers[2]);

    uint32_t ip_be = BYTES_TO_BE32(103, 0, 168, 192);
    jlos_amd_am79c973_set_ip_address(eth0, ip_be);

    jlos_ether_frame_provider_t etherframe;
    jlos_ether_frame_provider_init(&etherframe, eth0);

    jlos_arp_t arp;
    jlos_arp_init(&arp, &etherframe);

    uint32_t gip_be = BYTES_TO_BE32(1, 0, 168, 192);
    uint32_t subnet_be = BYTES_TO_BE32(0, 255, 255, 255);

    jlos_internet_protocol_provider_t ipv4;
    jlos_internet_protocol_provider_init(&ipv4, &etherframe, &arp, gip_be, subnet_be);

    jlos_icmp_t icmp;
    jlos_icmp_init(&icmp, &ipv4);

    jlos_udp_provider_t udp;
    jlos_udp_provider_init(&udp, &ipv4);

    jlos_tcp_provider_t tcp;
    jlos_tcp_provider_init(&tcp, &ipv4);

    jlos_interrupt_manager_activate(&interrupts);
    printf("\n");
#if CONFIG_ENABLE_TESTS
    memory_manager_test(multiboot_structure);
    multitask_test(&gdt, &task_manager_);
    hard_driver_test();
#endif
    jlos_arp_broadcast_mac_address(&arp, gip_be);

    printf_tcp_handler_t tcphandler;
    jlos_tcp_handler_init(&tcphandler.base);
    tcphandler.base.handle_tcp_message = printf_tcp_handler_handle_tcp_message;

    jlos_tcp_socket_t *tcpsocket = jlos_tcp_provider_listen(&tcp, 1234);
    jlos_tcp_provider_bind(&tcp, tcpsocket, &tcphandler.base);

    while (1) {
    }
}