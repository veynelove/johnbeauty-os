#ifndef _JLOS_DRIVERS_AMD_AM79C973_H
#define _JLOS_DRIVERS_AMD_AM79C973_H

#define NUM_RECV_BUFFERS 32
#define NUM_SEND_BUFFERS 8

#include <drivers/driver.h>
#include <hal/irq.h>
#include <hal/pci.h>
#include <hal/io.h>

typedef struct jlos_amd_am79c973    jlos_amd_am79c973_t;
typedef struct jlos_rawdata_handler jlos_rawdata_handler_t;

struct jlos_rawdata_handler {
    jlos_amd_am79c973_t *backend;
    bool                (*on_raw_data_received)(jlos_rawdata_handler_t* self, uint8_t *buffer, uint32_t size);
    void                (*send)(jlos_rawdata_handler_t* self, uint8_t *buffer, uint32_t size);
};

typedef struct {
    uint16_t mode;
    uint8_t recv_len;
    uint8_t send_len;
    uint8_t physical_address[6];
    uint16_t reserved;
    uint8_t logical_address[8];
    uint32_t recv_buffer_descr_address;
    uint32_t send_buffer_descr_address;
} __attribute__((packed)) jlos_amd_init_block_t;

typedef struct {
    uint32_t address;
    uint32_t flags;
    uint16_t flags2;
    uint16_t avail;
    uint32_t reserved;
} __attribute__((packed)) jlos_amd_buffer_descriptor_t;

struct jlos_amd_am79c973 {
    jlos_driver_t                   base_driver;
    jlos_irq_handler_t              base_handler;
    jlos_io16_t                     mac_address0_port;
    jlos_io16_t                     mac_address2_port;
    jlos_io16_t                     mac_address4_port;
    jlos_io16_t                     register_data_port;
    jlos_io16_t                     register_address_port;
    jlos_io16_t                     reset_port;
    jlos_io16_t                     bus_control_register_data_port;
    jlos_amd_init_block_t           *init_block;
    uint8_t                         init_block_memory[64];
    jlos_amd_buffer_descriptor_t    *send_buffer_descr;
    uint8_t                         send_buffer_desc_memory[NUM_SEND_BUFFERS * sizeof(jlos_amd_buffer_descriptor_t) + 15];
    uint8_t                         send_buffers[NUM_SEND_BUFFERS][2048] __attribute__((aligned(2048)));
    uint8_t                         current_send_buffer;
    jlos_amd_buffer_descriptor_t    *recv_buffer_descr;
    uint8_t                         recv_buffer_desc_memory[NUM_RECV_BUFFERS * sizeof(jlos_amd_buffer_descriptor_t) + 15];
    uint8_t                         recv_buffers[NUM_RECV_BUFFERS][2048] __attribute__((aligned(2048)));
    uint8_t                         current_recv_buffer;
    jlos_rawdata_handler_t          *handler;
};

void jlos_rawdata_handler_init(jlos_rawdata_handler_t* self, jlos_amd_am79c973_t *backend);
void jlos_rawdata_handler_destroy(jlos_rawdata_handler_t* self);
bool jlos_rawdata_handler_on_raw_data_received(jlos_rawdata_handler_t* self, uint8_t *buffer, uint32_t size);
void jlos_rawdata_handler_send(jlos_rawdata_handler_t* self, uint8_t *buffer, uint32_t size);

void jlos_amd_am79c973_init(jlos_amd_am79c973_t* self, jlos_hal_pci_device_t *dev, jlos_irq_manager_t *interrupts);
void jlos_amd_am79c973_destroy(jlos_amd_am79c973_t* self);

void jlos_amd_am79c973_activate(jlos_amd_am79c973_t* self);
uint32_t jlos_amd_am79c973_handle_interrupt(jlos_irq_handler_t* handler, uint32_t esp);

void jlos_amd_am79c973_send(jlos_amd_am79c973_t* self, uint8_t *buffer, int size);
void jlos_amd_am79c973_receive(jlos_amd_am79c973_t* self);
void jlos_amd_am79c973_set_handler(jlos_amd_am79c973_t* self, jlos_rawdata_handler_t *handler);

uint64_t jlos_amd_am79c973_get_mac_address(jlos_amd_am79c973_t* self);
void jlos_amd_am79c973_set_ip_address(jlos_amd_am79c973_t* self, uint32_t ip);
uint32_t jlos_amd_am79c973_get_ip_address(jlos_amd_am79c973_t* self);

#endif
