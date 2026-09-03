#ifndef _JLOS_NET_IPV4_H
#define _JLOS_NET_IPV4_H

#include <net/etherframe.h>
#include <net/arp.h>

typedef struct {
    uint8_t     version_ihl;
    uint8_t     tos;
    uint16_t    total_length;
    uint16_t    ident;
    uint16_t    flags_and_offset;
    uint8_t     time_to_live;
    uint8_t     protocol;
    uint16_t    checksum;
    uint32_t    src_ip;
    uint32_t    dst_ip;
} __attribute__((packed)) jlos_ipv4_message_t;

#define JLOS_IPV4_GET_VERSION(msg) (((msg)->version_ihl >> 4) & 0x0F)
#define JLOS_IPV4_GET_IHL(msg)     ((msg)->version_ihl & 0x0F)
#define JLOS_IPV4_SET_VERSION_IHL(msg, ver, ihl) ((msg)->version_ihl = (((ver) & 0x0F) << 4) | ((ihl) & 0x0F))

typedef struct jlos_internet_protocol_provider jlos_internet_protocol_provider_t;

typedef struct jlos_internet_protocol_handler {
    jlos_internet_protocol_provider_t   *backend;
    uint8_t                             ip_protocol;
    bool (*on_internet_protocol_received)(struct jlos_internet_protocol_handler* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t size);
} jlos_internet_protocol_handler_t;

struct jlos_internet_protocol_provider {
    jlos_ether_frame_handler_t          base_handler;
    jlos_internet_protocol_handler_t    *handlers[255];
    jlos_arp_t                          *arp;
    uint32_t                            gateway_ip;
    uint32_t                            subnet_mask;
};

void jlos_internet_protocol_handler_init(jlos_internet_protocol_handler_t* self, jlos_internet_protocol_provider_t *backend, uint8_t protocol);
void jlos_internet_protocol_handler_destroy(jlos_internet_protocol_handler_t* self);
bool jlos_internet_protocol_handler_on_internet_protocol_received(jlos_internet_protocol_handler_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t size);
void jlos_internet_protocol_handler_send(jlos_internet_protocol_handler_t* self, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t size);

void jlos_internet_protocol_provider_init(jlos_internet_protocol_provider_t* self, jlos_ether_frame_provider_t *backend, jlos_arp_t *arp, uint32_t gateway_ip, uint32_t subnet_mask);
void jlos_internet_protocol_provider_destroy(jlos_internet_protocol_provider_t* self);
bool jlos_internet_protocol_provider_on_ether_frame_received(jlos_internet_protocol_provider_t* self, uint8_t *etherframe_payload, uint32_t size);
void jlos_internet_protocol_provider_send(jlos_internet_protocol_provider_t* self, uint32_t dstIP_BE, uint8_t protocol, uint8_t *data, uint32_t size);
uint16_t jlos_internet_protocol_provider_check_sum(uint16_t *data, uint32_t length_in_bytes);
uint32_t jlos_internet_protocol_provider_get_ip_address(jlos_internet_protocol_provider_t* self);

#endif