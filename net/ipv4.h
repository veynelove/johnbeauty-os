#ifndef __JLOS_NET_IPV4_H
#define __JLOS_NET_IPV4_H

#include <net/etherframe.h>
#include <net/arp.h>

typedef struct {
    uint8_t header_length;
    uint8_t version;
    uint8_t m_tos;
    uint16_t m_total_length;
    uint16_t m_ident;
    uint16_t flags_and_offset;
    uint8_t m_time_to_live;
    uint8_t m_protocol;
    uint16_t m_checksum;
    uint32_t m_src_ip;
    uint32_t m_dst_ip;
} __attribute__((packed)) jlos_ipv4_message_t;

typedef struct jlos_internet_protocol_provider jlos_internet_protocol_provider_t;

typedef struct jlos_internet_protocol_handler {
    jlos_internet_protocol_provider_t *backend;
    uint8_t m_ip_protocol;
    bool (*on_internet_protocol_received)(struct jlos_internet_protocol_handler* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size);
} jlos_internet_protocol_handler_t;

struct jlos_internet_protocol_provider {
    jlos_ether_frame_handler_t base_handler;
    jlos_internet_protocol_handler_t *handlers[255];
    jlos_arp_t *arp;
    uint32_t m_gateway_ip;
    uint32_t m_subnet_mask;
};

void jlos_internet_protocol_handler_init(jlos_internet_protocol_handler_t* self, jlos_internet_protocol_provider_t *backend, uint8_t m_protocol);
void jlos_internet_protocol_handler_destroy(jlos_internet_protocol_handler_t* self);
bool jlos_internet_protocol_handler_on_internet_protocol_received(jlos_internet_protocol_handler_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size);
void jlos_internet_protocol_handler_send(jlos_internet_protocol_handler_t* self, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size);

void jlos_internet_protocol_provider_init(jlos_internet_protocol_provider_t* self, jlos_ether_frame_provider_t *backend, jlos_arp_t *arp, uint32_t m_gateway_ip, uint32_t m_subnet_mask);
void jlos_internet_protocol_provider_destroy(jlos_internet_protocol_provider_t* self);
bool jlos_internet_protocol_provider_on_ether_frame_received(jlos_internet_protocol_provider_t* self, uint8_t *etherframe_payload, uint32_t m_size);
void jlos_internet_protocol_provider_send(jlos_internet_protocol_provider_t* self, uint32_t dstIP_BE, uint8_t m_protocol, uint8_t *m_data, uint32_t m_size);
uint16_t jlos_internet_protocol_provider_check_sum(uint16_t *m_data, uint32_t length_in_bytes);
uint32_t jlos_internet_protocol_provider_get_ip_address(jlos_internet_protocol_provider_t* self);

#endif