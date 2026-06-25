#include <net/etherframe.h>
#include <kernel/memory_manager.h>

void jlos_ether_frame_handler_init(jlos_ether_frame_handler_t* self, jlos_ether_frame_provider_t *backend, uint16_t m_etherType_BE)
{
    self->m_etherType_BE = JLOS_SWAP_ENDIAN_16(m_etherType_BE);
    self->backend = backend;
    self->on_ether_frame_received = jlos_ether_frame_handler_on_ether_frame_received;
    backend->handlers[self->m_etherType_BE] = self;
}

void jlos_ether_frame_handler_destroy(jlos_ether_frame_handler_t* self)
{
    if (self->backend->handlers[self->m_etherType_BE] == self) {
        self->backend->handlers[self->m_etherType_BE] = NULL;
    }
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

void jlos_ether_frame_provider_init(jlos_ether_frame_provider_t* self, jlos_amd_am79c973_t *backend)
{
    jlos_rawdata_handler_init(&self->base_handler, backend);
    self->base_handler.on_raw_data_received = (bool (*)(jlos_rawdata_handler_t*, uint8_t*, uint32_t))jlos_ether_frame_provider_on_raw_data_received;
    
    for (uint32_t i = 0; i < 65535; i++) {
        self->handlers[i] = NULL;
    }
}

void jlos_ether_frame_provider_destroy(jlos_ether_frame_provider_t* self)
{
    jlos_rawdata_handler_destroy(&self->base_handler);
}

bool jlos_ether_frame_provider_on_raw_data_received(jlos_ether_frame_provider_t* self, uint8_t *buffer, uint32_t m_size)
{
    if (m_size < sizeof(jlos_ether_frame_header_t)) {
        return false;
    }
    jlos_ether_frame_header_t *frame = (jlos_ether_frame_header_t *)buffer;
    bool send_back = false;
    if (frame->dstMAC_BE == 0xFFFFFFFFFFFF || frame->dstMAC_BE == jlos_amd_am79c973_get_mac_address(self->base_handler.backend)) {
        if (self->handlers[frame->m_etherType_BE]) {
            send_back = self->handlers[frame->m_etherType_BE]->on_ether_frame_received(
                self->handlers[frame->m_etherType_BE], buffer + sizeof(jlos_ether_frame_header_t), m_size - sizeof(jlos_ether_frame_header_t));
        }
    }
    if (send_back) {
        frame->dstMAC_BE = frame->srcMAC_BE;
        frame->srcMAC_BE = jlos_amd_am79c973_get_mac_address(self->base_handler.backend);
    }
    return send_back;
}

void jlos_ether_frame_provider_send(jlos_ether_frame_provider_t* self, uint64_t dstMAC_BE, uint16_t m_etherType_BE, uint8_t *buffer, uint32_t m_size)
{
    uint8_t *buffer2 = (uint8_t *)jlos_malloc(sizeof(jlos_ether_frame_header_t) + m_size);
    jlos_ether_frame_header_t *frame = (jlos_ether_frame_header_t *)buffer2;
    frame->dstMAC_BE = dstMAC_BE;
    frame->srcMAC_BE = jlos_amd_am79c973_get_mac_address(self->base_handler.backend);
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