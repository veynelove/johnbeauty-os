#include <tools/tests/http_server_te.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>
#include <net/network.h>

#define JLOS_KERNEL_LOG_SUBSYS "test"

typedef struct {
    jlos_tcp_handler_t base;
} http_tcp_handler_t;

static bool http_tcp_handler_handle_tcp_message(jlos_tcp_handler_t* self, jlos_tcp_socket_t* socket, uint8_t *data, uint16_t size)
{
    (void)self;
    printk_debug("tcp received: %u bytes\n", size);
    if (size > 9
        && data[0] == 'G' && data[1] == 'E'
        && data[2] == 'T' && data[3] == ' '
        && data[4] == '/' && data[5] == ' '
        && data[6] == 'H' && data[7] == 'T'
        && data[8] == 'T' && data[9] == 'P') {
        const char *body = "<html><head><title>john beauty</title></head><body><b>johnbeauty</b> - john_love operating system</body></html>\r\n";
        uint16_t body_len = 0;
        while (body[body_len] != '\0') body_len++;
        const char *hdr = "HTTP/1.1 200 OK\r\nServer: JLOS\r\nContent-Type: text/html\r\nContent-Length: ";
        uint16_t hdr_len = 0;
        while (hdr[hdr_len] != '\0') hdr_len++;
        char clen[16];
        uint8_t ci = 0;
        uint16_t n = body_len;
        if (n == 0) { clen[ci++] = '0'; }
        else {
            char tmp[16]; int ti = 0;
            while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
            while (ti > 0) clen[ci++] = tmp[--ti];
        }
        clen[ci] = '\0';
        const char *crlfcrlf = "\r\n\r\n";
        uint16_t clen_len = ci;
        uint16_t total = hdr_len + clen_len + 4 + body_len;
        uint8_t *resp = (uint8_t *)jlos_kalloc(total);
        uint16_t p = 0;
        for (uint16_t i = 0; i < hdr_len; i++) resp[p++] = hdr[i];
        for (uint16_t i = 0; i < clen_len; i++) resp[p++] = clen[i];
        for (uint16_t i = 0; i < 4; i++) resp[p++] = crlfcrlf[i];
        for (uint16_t i = 0; i < body_len; i++) resp[p++] = body[i];
        socket->send(socket, resp, total);
        socket->disconnect(socket);
    }
    return true;
}

void http_server_test(void)
{
    jlos_tcp_provider_t *tcp = &g_network_stack->tcp;
    http_tcp_handler_t *tcphandler = jlos_kalloc(sizeof(http_tcp_handler_t));
    jlos_tcp_handler_init(&tcphandler->base);
    tcphandler->base.handle_tcp_message = http_tcp_handler_handle_tcp_message;

    jlos_tcp_socket_t *tcpsocket = jlos_tcp_provider_listen(tcp, 1234);
    jlos_tcp_provider_bind(tcp, tcpsocket, &tcphandler->base);
    printk_info("tcp server listening on port 1234\n");
}
