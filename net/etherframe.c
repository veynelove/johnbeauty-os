#include <net/etherframe.h>
#include <kernel/memory_manager.h>
#include <tools/config.h>
#include <kernel/printk.h>

void jlos_ether_frame_handler_init(jlos_ether_frame_handler_t* self, jlos_ether_frame_provider_t *backend, uint16_t m_etherType_BE)
{
    self->m_etherType_BE = JLOS_SWAP_ENDIAN_16(m_etherType_BE);
    self->backend = backend;
    self->on_ether_frame_received = jlos_ether_frame_handler_on_ether_frame_received;
    jlos_hash_chain_insert(&backend->handlers, &self->m_etherType_BE, &self->hash_node);
}

void jlos_ether_frame_handler_destroy(jlos_ether_frame_handler_t* self)
{
    jlos_hash_chain_remove(&self->backend->handlers, &self->hash_node);
}

bool jlos_ether_frame_handler_on_ether_frame_received(jlos_ether_frame_handler_t* self, uint8_t *etherframe_payload, uint32_t m_size)
{
    return false;
}

void jlos_ether_frame_handler_send(jlos_ether_frame_handler_t* self, uint64_t dstMAC_BE, uint16_t m_etherType_BE, uint8_t *buffer, uint32_t m_size)
{
    jlos_ether_frame_provider_send(self->backend, dstMAC_BE, m_etherType_BE, buffer, m_size);
}

uint32_t jlos_ether_frame_handler_get_ip_address(jlos_ether_frame_handler_t* self)
{
    return jlos_ether_frame_provider_get_ip_address(self->backend);
}

static int ether_frame_cmp(const void *key, const void *node)
{
    uint16_t be = *(uint16_t *)key;
    jlos_ether_frame_handler_t *handler = container_of(node, jlos_ether_frame_handler_t, hash_node);
    return be - handler->m_etherType_BE;
}

void jlos_ether_frame_provider_init(jlos_ether_frame_provider_t* self, jlos_amd_am79c973_t *backend)
{
    jlos_rawdata_handler_init(&self->base_handler, backend);
    self->base_handler.on_raw_data_received = (bool (*)(jlos_rawdata_handler_t*, uint8_t*, uint32_t))jlos_ether_frame_provider_on_raw_data_received;

    jlos_hash_chain_init(&self->handlers, JLOS_NET_HASH_CHAIN_NUM, jlos_hash_uint16, ether_frame_cmp);
}

void jlos_ether_frame_provider_destroy(jlos_ether_frame_provider_t* self)
{
    jlos_hash_chain_destroy(&self->handlers);
    jlos_rawdata_handler_destroy(&self->base_handler);
}

static bool mac_address_eq(uint8_t *a, uint8_t *b)
{
    for (int i = 0; i < 6; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static bool mac_address_is_broadcast(uint8_t *mac)
{
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0xFF) return false;
    }
    return true;
}

static void uint64_to_mac(uint64_t mac_be, uint8_t *dest)
{
    for (int i = 0; i < 6; i++) {
        dest[i] = (mac_be >> (8 * i)) & 0xFF;
    }
}

bool jlos_ether_frame_provider_on_raw_data_received(jlos_ether_frame_provider_t* self, uint8_t *buffer, uint32_t m_size)
{    
    if (m_size < sizeof(jlos_ether_frame_header_t)) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("ETHER: Frame too small\n");
#endif
        return false;
    }
    jlos_ether_frame_header_t *frame = (jlos_ether_frame_header_t *)buffer;
    bool send_back = false;
    
    uint8_t my_mac[6];
    uint64_to_mac(jlos_amd_am79c973_get_mac_address(self->base_handler.backend), my_mac);
    if (mac_address_is_broadcast(frame->dstMAC) || mac_address_eq(frame->dstMAC, my_mac)) {
        jlos_hash_node_t *node = jlos_hash_chain_see(&self->handlers, &frame->m_etherType_BE);
        if (node) {
            jlos_ether_frame_handler_t *handler = container_of(node, jlos_ether_frame_handler_t, hash_node);
            send_back = handler->on_ether_frame_received(
                handler, buffer + sizeof(jlos_ether_frame_header_t), m_size - sizeof(jlos_ether_frame_header_t));
        }
        else {
            printf("ETHER: No handler for this type\n");
        }
    }
    else {
        printf("ETHER: Frame not for us\n");
    }
    
    if (send_back) {
        uint8_t temp[6];
        for (int i = 0; i < 6; i++) {
            temp[i] = frame->dstMAC[i];
            frame->dstMAC[i] = frame->srcMAC[i];
        }
        uint64_to_mac(jlos_amd_am79c973_get_mac_address(self->base_handler.backend), frame->srcMAC);
    }
    return send_back;
}

void jlos_ether_frame_provider_send(jlos_ether_frame_provider_t* self, uint64_t dstMAC_BE, uint16_t m_etherType_BE, uint8_t *buffer, uint32_t m_size)
{
    uint8_t *buffer2 = (uint8_t *)jlos_malloc(sizeof(jlos_ether_frame_header_t) + m_size);
    jlos_ether_frame_header_t *frame = (jlos_ether_frame_header_t *)buffer2;
    
    uint64_to_mac(dstMAC_BE, frame->dstMAC);
    uint64_to_mac(jlos_amd_am79c973_get_mac_address(self->base_handler.backend), frame->srcMAC);
    frame->m_etherType_BE = m_etherType_BE;

    uint8_t *src = buffer;
    uint8_t *dst = buffer2 + sizeof(jlos_ether_frame_header_t);
    for (uint32_t i = 0; i < m_size; i++) {
        dst[i] = src[i];
    }
    jlos_amd_am79c973_send(self->base_handler.backend, buffer2, m_size + sizeof(jlos_ether_frame_header_t));
    jlos_free(buffer2);
}

uint64_t jlos_ether_frame_provider_get_mac_address(jlos_ether_frame_provider_t* self)
{
    return jlos_amd_am79c973_get_mac_address(self->base_handler.backend);
}

uint32_t jlos_ether_frame_provider_get_ip_address(jlos_ether_frame_provider_t* self)
{
    return jlos_amd_am79c973_get_ip_address(self->base_handler.backend);
}