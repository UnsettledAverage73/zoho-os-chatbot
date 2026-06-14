#include "tcp.h"
#include "net.h"
#include "klog.h"
#include "string.h"
#include "kmalloc.h"

#define MAX_CONNECTIONS 16
static tcp_conn_t connections[MAX_CONNECTIONS];
static uint16_t next_local_port = 49152;

static void tcp_handle_packet(const uint8_t* src_ip, uint16_t src_port, uint16_t dst_port, tcp_packet_t* tcp, void* data, uint16_t len) {
    (void)data; (void)len;
    
    /* Match an incoming segment to an existing connection. */
    tcp_conn_t* conn = NULL;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].state != TCP_STATE_CLOSED && 
            connections[i].local_port == dst_port &&
            connections[i].remote_port == src_port &&
            memcmp(connections[i].remote_ip, src_ip, 4) == 0) {
            conn = &connections[i];
            break;
        }
    }

    if (!conn) {
        if (tcp->flags & TCP_FLAG_SYN) {
            klog(LOG_INFO, "TCP", "Incoming connection request from %d.%d.%d.%d:%d",
                 src_ip[0], src_ip[1], src_ip[2], src_ip[3], src_port);
            /* No listener: reject the incoming SYN with RST. */
            net_send_tcp(src_ip, dst_port, src_port, 0, ntohl(tcp->seq) + 1, TCP_FLAG_RST | TCP_FLAG_ACK, NULL, 0);
        }
        return;
    }

    uint32_t seq = ntohl(tcp->seq);
    uint32_t ack = ntohl(tcp->ack);

    if (conn->state == TCP_STATE_SYN_SENT) {
        if ((tcp->flags & TCP_FLAG_SYN) && (tcp->flags & TCP_FLAG_ACK)) {
            klog(LOG_INFO, "TCP", "Connection established with %d.%d.%d.%d:%d",
                 src_ip[0], src_ip[1], src_ip[2], src_ip[3], src_port);
            
            conn->state = TCP_STATE_ESTABLISHED;
            conn->remote_seq = seq + 1;
            conn->ack = conn->remote_seq;
            conn->remote_ack = ack;
            
            /* Finish the three-way handshake. */
            net_send_tcp(src_ip, conn->local_port, src_port, conn->seq, conn->ack, TCP_FLAG_ACK, NULL, 0);
        }
    } else if (conn->state == TCP_STATE_ESTABLISHED) {
        if (tcp->flags & TCP_FLAG_FIN) {
            klog(LOG_INFO, "TCP", "Connection closing by remote host.");
            conn->state = TCP_STATE_CLOSE_WAIT;
            conn->ack = seq + 1;
            net_send_tcp(src_ip, conn->local_port, src_port, conn->seq, conn->ack, TCP_FLAG_ACK, NULL, 0);
            
            /* Begin connection teardown. */
            net_send_tcp(src_ip, conn->local_port, src_port, conn->seq, conn->ack, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            conn->state = TCP_STATE_LAST_ACK;
            conn->seq++;
        }
    } else if (conn->state == TCP_STATE_LAST_ACK) {
        if (tcp->flags & TCP_FLAG_ACK) {
            conn->state = TCP_STATE_CLOSED;
            klog(LOG_INFO, "TCP", "Connection closed.");
        }
    }
}

void tcp_init() {
    /* Clear all connection slots. */
    memset(connections, 0, sizeof(connections));
}

void tcp_connect(const uint8_t* ip, uint16_t port) {
    /* Pick a free connection slot and send SYN. */
    tcp_conn_t* conn = NULL;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].state == TCP_STATE_CLOSED) {
            conn = &connections[i];
            break;
        }
    }

    if (!conn) {
        klog(LOG_ERROR, "TCP", "Connect failed: max connections reached.");
        return;
    }

    memcpy(conn->remote_ip, ip, 4);
    conn->remote_port = port;
    conn->local_port = next_local_port++;
    conn->state = TCP_STATE_SYN_SENT;
    conn->seq = 0x12345678; /* Placeholder initial sequence number. */
    conn->ack = 0;

    net_tcp_bind(conn->local_port, tcp_handle_packet);

    klog(LOG_INFO, "TCP", "Connecting to %d.%d.%d.%d:%d...", ip[0], ip[1], ip[2], ip[3], port);
    net_send_tcp(ip, conn->local_port, port, conn->seq, 0, TCP_FLAG_SYN, NULL, 0);
    conn->seq++;
}
