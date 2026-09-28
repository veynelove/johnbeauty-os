#include <net/dhcp.h>
#include <net/network.h>
#include <kernel/memory_manager.h>
#include <kernel/timek.h>
#include <tools/config.h>

#define JLOS_KERNEL_LOG_SUBSYS "dhcp"
#include <kernel/printk.h>

static uint8_t *dhcp_write_option(uint8_t *p, uint8_t code, uint8_t len, const uint8_t *val)
{
    p[0] = code;
    p[1] = len;
    jlos_memcpy(p + 2, val, len);
    return p + 2 + len;
}

static const uint8_t *dhcp_find_option(const uint8_t *options, uint16_t max_len, uint8_t code)
{
    uint16_t i = 0;
    while (i < max_len) {
        uint8_t opt = options[i];
        if (opt == JLOS_DHCP_OPT_PAD) {
            i++;
            continue;
        }
        if (opt == JLOS_DHCP_OPT_END) {
            break;
        }
        if (i + 1 >= max_len) {
            break;
        }
        uint8_t len = options[i + 1];
        if (opt == code) {
            return &options[i + 2];
        }
        i += 2 + len;
    }
    return NULL;
}

static void jlos_dhcp_pad_to_min(uint8_t *buffer, uint16_t *total)
{
    while (*total < JLOS_DHCP_MIN_PACKET) {
        buffer[*total] = JLOS_DHCP_OPT_PAD;
        (*total)++;
    }
}

static void jlos_dhcp_fill_header(jlos_dhcp_header_t *msg, jlos_dhcp_client_t *self)
{
    msg->op = JLOS_DHCP_OP_BOOTREQUEST;
    msg->htype = JLOS_DHCP_HTYPE_ETHERNET;
    msg->hlen = JLOS_DHCP_HLEN_ETHERNET;
    msg->hops = 0;
    msg->xid = self->xid;
    msg->secs = 0;
    msg->flags = JLOS_SWAP_ENDIAN_16(JLOS_DHCP_FLAGS_BROADCAST);
    msg->ciaddr = 0;
    msg->yiaddr = 0;
    msg->siaddr = 0;
    msg->giaddr = 0;
    jlos_memset(msg->chaddr, 0, 16);
    jlos_memcpy(msg->chaddr, self->mac, 6);
    jlos_memset(msg->sname, 0, 64);
    jlos_memset(msg->file, 0, 128);
    msg->magic_cookie = JLOS_SWAP_ENDIAN_32(JLOS_DHCP_MAGIC_COOKIE);
}

static void jlos_dhcp_send_broadcast(jlos_dhcp_client_t *self, uint8_t *buffer, uint16_t total)
{
    self->socket->remote_ip = JLOS_IPV4_BROADCAST;
    self->socket->remote_port = JLOS_SWAP_ENDIAN_16(JLOS_DHCP_SERVER_PORT);
    self->socket->send(self->socket, buffer, total);
}

static void jlos_dhcp_client_request(jlos_dhcp_client_t *self)
{
    uint8_t buffer[sizeof(jlos_dhcp_header_t) + JLOS_DHCP_OPTIONS_MAX];
    
    jlos_dhcp_fill_header((jlos_dhcp_header_t *)buffer, self);
    uint8_t *opt = buffer + sizeof(jlos_dhcp_header_t);
    uint8_t msg_type = JLOS_DHCP_MSG_REQUEST;
    opt = dhcp_write_option(opt, JLOS_DHCP_OPT_MSG_TYPE, 1, &msg_type);
    opt = dhcp_write_option(opt, JLOS_DHCP_OPT_REQUESTED_IP, 4, (uint8_t *)&self->offered_ip);
    opt = dhcp_write_option(opt, JLOS_DHCP_OPT_SERVER_ID, 4, (uint8_t *)&self->server_id);
    opt[0] = JLOS_DHCP_OPT_END;
    opt++;

    uint16_t total = (uint16_t)(opt - buffer);
    jlos_dhcp_pad_to_min(buffer, &total);

    jlos_dhcp_send_broadcast(self, buffer, total);
    self->state = JLOS_DHCP_STATE_REQUESTING;
    printk_info("REQ sent, offered_ip = %x, server_id = %x\n", self->offered_ip, self->server_id);
}

static void jlos_dhcp_handle_offer(jlos_dhcp_client_t *self, jlos_dhcp_header_t *msg, const uint8_t *options, uint16_t options_len)
{
    if (self->state != JLOS_DHCP_STATE_SELECTING) {
        return;
    }
    self->offered_ip = msg->yiaddr;

    const uint8_t *server_id_opt = dhcp_find_option(options, options_len, JLOS_DHCP_OPT_SERVER_ID);
    if (server_id_opt) {
        jlos_memcpy(&self->server_id, server_id_opt, 4);
    } else {
        self->server_id = msg->siaddr;
    }

    printk_info("OFFER received, yiaddr = %x, server_id = %x\n", self->offered_ip, self->server_id);
    jlos_dhcp_client_request(self);
}

static void jlos_dhcp_handle_ack(jlos_dhcp_client_t *self, jlos_dhcp_header_t *msg, const uint8_t *options, uint16_t options_len)
{
    if (self->state != JLOS_DHCP_STATE_REQUESTING) {
        return;
    }
    g_network_stack->config.ip = msg->yiaddr;
    
    const uint8_t *opt = dhcp_find_option(options, options_len, JLOS_DHCP_OPT_SUBNET_MASK);
    if (opt) {
        jlos_memcpy(&g_network_stack->config.subnet_mask, opt, 4);
    }
    opt = dhcp_find_option(options, options_len, JLOS_DHCP_OPT_ROUTER);
    if (opt) {
        jlos_memcpy(&g_network_stack->config.gateway, opt, 4);
    }
    opt = dhcp_find_option(options, options_len, JLOS_DHCP_OPT_DNS_SERVER);
    if (opt) {
        jlos_memcpy(&g_network_stack->config.dns_server, opt, 4);
    }
    jlos_network_apply_config();
    self->state = JLOS_DHCP_STATE_BOUND;
    printk_info("BOUND: ip = %d.%d.%d.%d, mask = %d.%d.%d.%d, gw = %d.%d.%d.%d, dns = %d.%d.%d.%d\n",
    JLOS_IPV4_FMT(g_network_stack->config.ip),
    JLOS_IPV4_FMT(g_network_stack->config.subnet_mask),
    JLOS_IPV4_FMT(g_network_stack->config.gateway),
    JLOS_IPV4_FMT(g_network_stack->config.dns_server));
}

static void jlos_dhcp_on_message(jlos_udp_handler_t *handler, jlos_udp_socket_t *socket, uint8_t *data, uint16_t size)
{
    jlos_dhcp_client_t *self = container_of(handler, jlos_dhcp_client_t, handler);
    (void)socket;
    if (size < sizeof(jlos_dhcp_header_t)) {
        return;
    }
    jlos_dhcp_header_t *msg = (jlos_dhcp_header_t *)data;
    if (msg->op != JLOS_DHCP_OP_BOOTREPLY || msg->xid != self->xid || msg->magic_cookie != JLOS_SWAP_ENDIAN_32(JLOS_DHCP_MAGIC_COOKIE)) {
        return;
    }
    const uint8_t *options = data + sizeof(jlos_dhcp_header_t);
    uint16_t options_len = size - sizeof(jlos_dhcp_header_t);
    const uint8_t *msg_type_opt = dhcp_find_option(options, options_len, JLOS_DHCP_OPT_MSG_TYPE);
    if (!msg_type_opt) {
        return;
    }
    uint8_t msg_type = msg_type_opt[0];
    switch (msg_type) {
        case JLOS_DHCP_MSG_OFFER:
            jlos_dhcp_handle_offer(self, msg, options, options_len);
            break;
        case JLOS_DHCP_MSG_ACK:
            jlos_dhcp_handle_ack(self, msg, options, options_len);
            break;
        case JLOS_DHCP_MSG_NAK:
            self->state = JLOS_DHCP_STATE_FAILED;
            printk_warn("NAK received\n");
            break;
        default:
            break;
    }
}

void jlos_dhcp_client_init(jlos_dhcp_client_t *self, jlos_udp_provider_t *udp, uint8_t mac[6])
{
    self->handler.handle_udp_message = jlos_dhcp_on_message;
    self->udp = udp;
    self->state = JLOS_DHCP_STATE_INIT;
    self->xid = 0;
    self->server_id = 0;
    self->offered_ip = 0;
    jlos_memcpy(self->mac, mac, 6);

    self->socket = jlos_udp_provider_listen(udp, JLOS_DHCP_CLIENT_PORT);
    if (!self->socket) {
        printk_err("failed to listen on port %d\n", JLOS_DHCP_CLIENT_PORT);
        self->state = JLOS_DHCP_STATE_FAILED;
        return;
    }
    jlos_udp_provider_bind(udp, self->socket, &self->handler);
    printk_info("client initialized, xid = %x\n", self->xid);
}

void jlos_dhcp_client_discover(jlos_dhcp_client_t *self)
{
    if (self->state == JLOS_DHCP_STATE_BOUND) {
        return;
    }
    if (self->xid == 0) {
        self->xid = (uint32_t)jlos_timek_get_monotonic_ns();
        if (self->xid == 0) {
            self->xid = 1;
        }
    }
    uint8_t buffer[sizeof(jlos_dhcp_header_t) + JLOS_DHCP_OPTIONS_MAX];
    jlos_dhcp_fill_header((jlos_dhcp_header_t *)buffer, self);
    uint8_t *opt = buffer + sizeof(jlos_dhcp_header_t);
    uint8_t msg_type = JLOS_DHCP_MSG_DISCOVER;
    opt = dhcp_write_option(opt, JLOS_DHCP_OPT_MSG_TYPE, 1, &msg_type);
    opt[0] = JLOS_DHCP_OPT_END;
    opt++;

    uint16_t total = (uint16_t)(opt - buffer);
    jlos_dhcp_pad_to_min(buffer, &total);
    jlos_dhcp_send_broadcast(self, buffer, total);
    self->state = JLOS_DHCP_STATE_SELECTING;
    printk_info("DISC sent, xid = %x\n", self->xid);
}
