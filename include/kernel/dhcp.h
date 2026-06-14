#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>

/**
 * @file dhcp.h
 * @brief DHCP client packet definitions.
 */

#define DHCP_BOOTREQUEST 1
#define DHCP_BOOTREPLY   2

#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET  6

#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCP_MSG_DISCOVER 1
#define DHCP_MSG_OFFER    2
#define DHCP_MSG_REQUEST  3
#define DHCP_MSG_ACK      5

#define DHCP_OPT_MSG_TYPE 53
#define DHCP_OPT_SUBNET   1
#define DHCP_OPT_ROUTER   3
#define DHCP_OPT_DNS      6
#define DHCP_OPT_REQ_IP   50
#define DHCP_OPT_SERVER   54
#define DHCP_OPT_END      255

typedef struct {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t  ciaddr[4];
    uint8_t  yiaddr[4];
    uint8_t  siaddr[4];
    uint8_t  giaddr[4];
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
    uint8_t  options[];
} __attribute__((packed)) dhcp_packet_t;

/**
 * Register the DHCP client handler.
 */
void dhcp_init();

/**
 * Broadcast a DHCPDISCOVER message.
 */
void dhcp_discover();

#endif
