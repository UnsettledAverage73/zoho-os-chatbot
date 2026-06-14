#ifndef NET_H
#define NET_H

#include <stdint.h>

/**
 * @file net.h
 * @brief Ethernet, IPv4, TCP, UDP, and ARP helpers.
 */

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV6 0x86DD

#define swap16(x) ((uint16_t)((((uint16_t)(x) & 0x00ff) << 8) | (((uint16_t)(x) & 0xff00) >> 8)))
#define swap32(x) ((uint32_t)((((uint32_t)(x) & 0x000000ff) << 24) | (((uint32_t)(x) & 0x0000ff00) << 8) | (((uint32_t)(x) & 0x00ff0000) >> 8) | (((uint32_t)(x) & 0xff000000) >> 24)))

#define htons(x) swap16(x)
#define ntohs(x) swap16(x)
#define htonl(x) swap32(x)
#define ntohl(x) swap32(x)

#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4     0x0800
#define ARP_OP_REQUEST     1
#define ARP_OP_REPLY       2

#define IPV4_PROTO_ICMP    1
#define IPV4_PROTO_TCP     6
#define IPV4_PROTO_UDP     17

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

#define TCP_FLAG_FIN (1 << 0)
#define TCP_FLAG_SYN (1 << 1)
#define TCP_FLAG_RST (1 << 2)
#define TCP_FLAG_PSH (1 << 3)
#define TCP_FLAG_ACK (1 << 4)
#define TCP_FLAG_URG (1 << 5)

typedef struct {
    uint8_t  dest[6];
    uint8_t  src[6];
    uint16_t type;
} __attribute__((packed)) eth_header_t;

typedef struct {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag_offset;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t checksum;
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
} __attribute__((packed)) ipv4_packet_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
    uint8_t  data[];
} __attribute__((packed)) icmp_packet_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  off_res;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
    uint8_t  options[];
} __attribute__((packed)) tcp_packet_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
    uint8_t  payload[];
} __attribute__((packed)) udp_packet_t;

typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  src_mac[6];
    uint8_t  src_ip[4];
    uint8_t  dst_mac[6];
    uint8_t  dst_ip[4];
} __attribute__((packed)) arp_packet_t;

/**
 * Initialize the network stack and NIC state.
 */
void net_init();

/**
 * Dispatch one received Ethernet frame.
 */
void net_handle_packet(void* data, uint16_t len);

/**
 * Get the number of received packets.
 */
uint64_t net_get_rx_count();

/**
 * Get the number of transmitted packets.
 */
uint64_t net_get_tx_count();

/**
 * Read the current IPv4 address.
 */
void net_get_ip(uint8_t* ip);

/**
 * Set the current IPv4 address.
 */
void net_set_ip(const uint8_t* ip);

/**
 * Send an ARP request for a target IP.
 */
void net_send_arp_request(const uint8_t* target_ip);

/**
 * Send an ICMP echo request.
 */
void net_ping(const uint8_t* target_ip);

/**
 * Send a UDP payload.
 */
void net_send_udp(const uint8_t* target_ip, uint16_t src_port, uint16_t dst_port, const void* data, uint16_t len);

/**
 * Send a TCP segment.
 */
void net_send_tcp(const uint8_t* target_ip, uint16_t src_port, uint16_t dst_port, uint32_t seq, uint32_t ack, uint8_t flags, const void* data, uint16_t len);

typedef void (*udp_handler_t)(const uint8_t* src_ip, uint16_t src_port, uint16_t dst_port, void* data, uint16_t len);

/**
 * Bind a UDP port to a handler.
 */
void net_udp_bind(uint16_t port, udp_handler_t handler);

typedef void (*tcp_handler_t)(const uint8_t* src_ip, uint16_t src_port, uint16_t dst_port, tcp_packet_t* tcp, void* data, uint16_t len);

/**
 * Bind a TCP port to a handler.
 */
void net_tcp_bind(uint16_t port, tcp_handler_t handler);

#endif
