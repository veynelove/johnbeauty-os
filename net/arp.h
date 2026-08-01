#ifndef __JLOS_NET_ARP_H
#define __JLOS_NET_ARP_H

#include <net/etherframe.h>

typedef struct {
    uint16_t hardware_type;
    uint16_t protocol;
    uint8_t hardware_address_size;
    uint8_t protocol_address_size;
    uint16_t command;
    uint8_t src_mac[6];
    uint32_t src_ip;
    uint8_t dst_mac[6];
    uint32_t dst_ip;
} __attribute__((packed)) jlos_arp_message_t;

typedef struct jlos_arp jlos_arp_t;

struct jlos_arp {
    jlos_ether_frame_handler_t base_handler;
    uint32_t ip_cache[128];
    uint64_t mac_cache[128];
    int num_cache_entries;
};

void jlos_arp_init(jlos_arp_t* self, jlos_ether_frame_provider_t *backend);
void jlos_arp_destroy(jlos_arp_t* self);
bool jlos_arp_on_ether_frame_received(jlos_arp_t* self, uint8_t *etherframe_payload, uint32_t size);
void jlos_arp_request_mac_address(jlos_arp_t* self, uint32_t IP_BE);
uint64_t jlos_arp_get_mac_from_cache(jlos_arp_t* self, uint32_t IP_BE);
uint64_t jlos_arp_lookup_or_request(jlos_arp_t* self, uint32_t IP_BE);
uint64_t jlos_arp_resolve(jlos_arp_t* self, uint32_t IP_BE);
void jlos_arp_broadcast_mac_address(jlos_arp_t* self, uint32_t IP_BE);

#endif