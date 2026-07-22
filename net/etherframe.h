#ifndef __JLOS_NET_ETHERFRAME_H
#define __JLOS_NET_ETHERFRAME_H

#include <common/types.h>
#include <drivers/amd_am79c973.h>
#include <dsa/hash_chain.h>

#define JLOS_SWAP_ENDIAN_16(m_x) ((((m_x) & 0x00FF) << 8) | (((m_x) & 0xFF00) >> 8))
#define JLOS_SWAP_ENDIAN_32(m_x) ((((m_x) & 0xFF000000) >> 24) | (((m_x) & 0x00FF0000) >> 8) | (((m_x) & 0x0000FF00) << 8) | (((m_x) & 0x000000FF) << 24))

typedef struct {
    uint8_t dstMAC[6];
    uint8_t srcMAC[6];
    uint16_t m_etherType_BE;
} __attribute__((packed)) jlos_ether_frame_header_t;

typedef uint32_t jlos_ether_frame_footer_t;

typedef struct jlos_ether_frame_provider jlos_ether_frame_provider_t;
typedef struct jlos_ether_frame_handler jlos_ether_frame_handler_t;

struct jlos_ether_frame_handler {
    jlos_ether_frame_provider_t *backend;
    uint16_t m_etherType_BE;
    jlos_hash_node_t hash_node;
    bool (*on_ether_frame_received)(jlos_ether_frame_handler_t* self, uint8_t *etherframe_payload, uint32_t m_size);
};

struct jlos_ether_frame_provider {
    jlos_rawdata_handler_t base_handler;
    jlos_hash_chain_t handlers;
};

void jlos_ether_frame_handler_init(jlos_ether_frame_handler_t* self, jlos_ether_frame_provider_t *backend, uint16_t m_etherType_BE);
void jlos_ether_frame_handler_destroy(jlos_ether_frame_handler_t* self);
bool jlos_ether_frame_handler_on_ether_frame_received(jlos_ether_frame_handler_t* self, uint8_t *etherframe_payload, uint32_t m_size);
void jlos_ether_frame_handler_send(jlos_ether_frame_handler_t* self, uint64_t dstMAC_BE, uint16_t m_etherType_BE, uint8_t *buffer, uint32_t m_size);
uint32_t jlos_ether_frame_handler_get_ip_address(jlos_ether_frame_handler_t* self);

void jlos_ether_frame_provider_init(jlos_ether_frame_provider_t* self, jlos_amd_am79c973_t *backend);
void jlos_ether_frame_provider_destroy(jlos_ether_frame_provider_t* self);
bool jlos_ether_frame_provider_on_raw_data_received(jlos_ether_frame_provider_t* self, uint8_t *buffer, uint32_t m_size);
void jlos_ether_frame_provider_send(jlos_ether_frame_provider_t* self, uint64_t dstMAC_BE, uint16_t m_etherType_BE, uint8_t *buffer, uint32_t m_size);
uint64_t jlos_ether_frame_provider_get_mac_address(jlos_ether_frame_provider_t* self);
uint32_t jlos_ether_frame_provider_get_ip_address(jlos_ether_frame_provider_t* self);

#endif