#include <tools/tests/udp_server_te.h>

extern void printf(const char *str);

typedef struct {
    jlos_udp_handler_t base;
} printf_udp_handler_t;

static void printf_udp_handler_handle_udp_message(jlos_udp_handler_t* self, jlos_udp_socket_t* socket, uint8_t *m_data, uint16_t m_size)
{
    printf("UDP received: ");
    char foo[2] = " ";
    for (int i = 0; i < m_size; i++) {
        foo[0] = m_data[i];
        printf(foo);
    }
    printf("\n");

    // Echo back the received data
    socket->send(socket, m_data, m_size);
}

void udp_server_test(jlos_udp_provider_t *udp)
{
    printf_udp_handler_t udphandler;
    jlos_udp_handler_init(&udphandler.base);
    udphandler.base.handle_udp_message = printf_udp_handler_handle_udp_message;
    jlos_udp_socket_t *udpsocket = jlos_udp_provider_listen(udp, 5678);
    jlos_udp_provider_bind(udp, udpsocket, &udphandler.base);
    printf("UDP server listening on port 5678\n");
}