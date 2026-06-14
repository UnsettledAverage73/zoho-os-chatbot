#include "dhcp.h"
#include "net.h"
#include "e1000.h"
#include "klog.h"
#include "string.h"
#include "kmalloc.h"

static uint32_t dhcp_xid = 0x12345678;
static uint8_t dhcp_offered_ip[4];
static uint8_t dhcp_server_ip[4];

static void dhcp_handle(const uint8_t* src_ip, uint16_t src_port, uint16_t dst_port, void* data, uint16_t len) {
    (void)src_ip; (void)src_port; (void)dst_port;
    /* Decode DHCP replies and react to OFFER/ACK messages. */
    if (len < sizeof(dhcp_packet_t)) return;
    dhcp_packet_t* pkt = (dhcp_packet_t*)data;

    if (pkt->op != DHCP_BOOTREPLY || ntohl(pkt->magic) != DHCP_MAGIC_COOKIE) return;
    if (pkt->xid != htonl(dhcp_xid)) return;

    uint8_t msg_type = 0;
    uint8_t* opt = pkt->options;
    while (opt < (uint8_t*)data + len && *opt != DHCP_OPT_END) {
        uint8_t type = *opt++;
        uint8_t opt_len = *opt++;
        if (type == DHCP_OPT_MSG_TYPE) msg_type = *opt;
        else if (type == DHCP_OPT_SERVER) memcpy(dhcp_server_ip, opt, 4);
        opt += opt_len;
    }

    if (msg_type == DHCP_MSG_OFFER) {
        memcpy(dhcp_offered_ip, pkt->yiaddr, 4);
        klog(LOG_INFO, "DHCP", "Received OFFER: %d.%d.%d.%d",
             dhcp_offered_ip[0], dhcp_offered_ip[1], dhcp_offered_ip[2], dhcp_offered_ip[3]);
        
        /* Respond to an OFFER with a REQUEST. */
        uint8_t broadcast_ip[4] = {255, 255, 255, 255};
        uint16_t total_len = sizeof(dhcp_packet_t) + 12;
        dhcp_packet_t* req = kmalloc(total_len);
        memset(req, 0, total_len);
        
        req->op = DHCP_BOOTREQUEST;
        req->htype = DHCP_HTYPE_ETHERNET;
        req->hlen = DHCP_HLEN_ETHERNET;
        req->xid = htonl(dhcp_xid);
        req->magic = htonl(DHCP_MAGIC_COOKIE);
        e1000_get_mac(req->chaddr);

        uint8_t* ropt = req->options;
        *ropt++ = DHCP_OPT_MSG_TYPE; *ropt++ = 1; *ropt++ = DHCP_MSG_REQUEST;
        *ropt++ = DHCP_OPT_REQ_IP;   *ropt++ = 4; memcpy(ropt, dhcp_offered_ip, 4); ropt += 4;
        *ropt++ = DHCP_OPT_SERVER;   *ropt++ = 4; memcpy(ropt, dhcp_server_ip, 4); ropt += 4;
        *ropt++ = DHCP_OPT_END;

        net_send_udp(broadcast_ip, 68, 67, req, total_len);
        kfree(req);
    } else if (msg_type == DHCP_MSG_ACK) {
        klog(LOG_INFO, "DHCP", "Received ACK. Network configured.");
        net_set_ip(pkt->yiaddr);
    }
}

void dhcp_init() {
    /* Listen for DHCP replies on UDP port 68. */
    net_udp_bind(68, dhcp_handle);
}

void dhcp_discover() {
    /* Broadcast a DHCPDISCOVER packet. */
    klog(LOG_INFO, "DHCP", "Sending DISCOVER...");
    
    uint8_t broadcast_ip[4] = {255, 255, 255, 255};
    uint16_t total_len = sizeof(dhcp_packet_t) + 4;
    dhcp_packet_t* pkt = kmalloc(total_len);
    memset(pkt, 0, total_len);

    pkt->op = DHCP_BOOTREQUEST;
    pkt->htype = DHCP_HTYPE_ETHERNET;
    pkt->hlen = DHCP_HLEN_ETHERNET;
    pkt->xid = htonl(dhcp_xid);
    pkt->magic = htonl(DHCP_MAGIC_COOKIE);
    pkt->flags = htons(0x8000); // Broadcast flag
    e1000_get_mac(pkt->chaddr);

    uint8_t* opt = pkt->options;
    *opt++ = DHCP_OPT_MSG_TYPE;
    *opt++ = 1;
    *opt++ = DHCP_MSG_DISCOVER;
    *opt++ = DHCP_OPT_END;

    net_send_udp(broadcast_ip, 68, 67, pkt, total_len);
    kfree(pkt);
}
