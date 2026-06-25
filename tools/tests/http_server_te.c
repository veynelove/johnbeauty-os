#include <tools/tests/http_server_te.h>

extern void printf(const char *str);

typedef struct {
    jlos_tcp_handler_t base;
} printf_tcp_handler_t;

static bool printf_tcp_handler_handle_tcp_message(jlos_tcp_handler_t* self, jlos_tcp_socket_t* socket, uint8_t *m_data, uint16_t m_size)
{
    char foo[2] = " ";
    for (int i = 0; i < m_size; i++) {
        foo[0] = m_data[i];
        printf(foo);
    }
    if (m_size > 9
        && m_data[0] == 'G' && m_data[1] == 'E'
        && m_data[2] == 'T' && m_data[3] == ' '
        && m_data[4] == '/' && m_data[5] == ' '
        && m_data[6] == 'H' && m_data[7] == 'T'
        && m_data[8] == 'T' && m_data[9] == 'P') {
        socket->send(socket, (uint8_t *)"HTTP/1.1 200 OK\r\n_server: JLOS\r\n_content-m_type: text/html\r\n\r\n<html><head><title>john beauty</title></head><body><m_b>johnbeauty</m_b>john_love operating system</body></html>\r\n", 177);
        socket->disconnect(socket);
    }
    return true;
}

void http_server_test(jlos_tcp_provider_t *tcp)
{
    printf_tcp_handler_t tcphandler;
    jlos_tcp_handler_init(&tcphandler.base);
    tcphandler.base.handle_tcp_message = printf_tcp_handler_handle_tcp_message;

    jlos_tcp_socket_t *tcpsocket = jlos_tcp_provider_listen(tcp, 1234);
    jlos_tcp_provider_bind(tcp, tcpsocket, &tcphandler.base);
}
