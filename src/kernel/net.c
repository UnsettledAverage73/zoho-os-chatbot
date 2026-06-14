#include "net.h"
#include "e1000.h"
#include "klog.h"
#include "string.h"
#include "kmalloc.h"
#include "pit.h"

static uint64_t rx_packet_count = 0;
static uint64_t tx_packet_count = 0;
static int net_trace_enabled = 1;

static uint8_t my_ip[4] = {10, 0, 2, 15};
static uint8_t my_mac[6];
static uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct {
    uint8_t ip[4];
    uint8_t mac[6];
    int valid;
} arp_entry_t;

#define ARP_CACHE_SIZE 32
static arp_entry_t arp_cache[ARP_CACHE_SIZE];

#define UDP_MAX_BINDINGS 16
typedef struct {
    uint16_t port;
    udp_handler_t handler;
    int valid;
} udp_binding_t;
static udp_binding_t udp_bindings[UDP_MAX_BINDINGS];

#define TCP_MAX_BINDINGS 16
typedef struct {
    uint16_t port;
    tcp_handler_t handler;
    int valid;
} tcp_binding_t;
static tcp_binding_t tcp_bindings[TCP_MAX_BINDINGS];

void net_init() {
    e1000_get_mac(my_mac);
    memset(arp_cache, 0, sizeof(arp_cache));
    memset(udp_bindings, 0, sizeof(udp_bindings));
    memset(tcp_bindings, 0, sizeof(tcp_bindings));
    klog(LOG_INFO, "NET", "Network stack initialized with IP 10.0.2.15");
}

void net_udp_bind(uint16_t port, udp_handler_t handler) {
    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (!udp_bindings[i].valid) {
            udp_bindings[i].port = port;
            udp_bindings[i].handler = handler;
            udp_bindings[i].valid = 1;
            return;
        }
    }
    klog(LOG_ERROR, "NET", "UDP bind failed: max bindings reached.");
}

void net_tcp_bind(uint16_t port, tcp_handler_t handler) {
    for (int i = 0; i < TCP_MAX_BINDINGS; i++) {
        if (!tcp_bindings[i].valid) {
            tcp_bindings[i].port = port;
            tcp_bindings[i].handler = handler;
            tcp_bindings[i].valid = 1;
            return;
        }
    }
    klog(LOG_ERROR, "NET", "TCP bind failed: max bindings reached.");
}

static uint16_t net_checksum(void* data, uint32_t len) {
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)data;
    while (len > 1) { sum += *ptr++; len -= 2; }
    if (len > 0) sum += *(uint8_t*)ptr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static void net_send_eth(const uint8_t* dest_mac, uint16_t type, const void* data, uint16_t len) {
    if (net_trace_enabled) {
        klog(LOG_INFO, "NET", "TX Eth: Dst=%x:%x:%x:%x:%x:%x, Type=0x%x, Len=%d",
             (int)dest_mac[0], (int)dest_mac[1], (int)dest_mac[2], 
             (int)dest_mac[3], (int)dest_mac[4], (int)dest_mac[5],
             (int)type, (int)len);
    }

    uint16_t total_len = sizeof(eth_header_t) + len;
    eth_header_t* eth = kmalloc(total_len);
    memcpy(eth->dest, dest_mac, 6);
    memcpy(eth->src, my_mac, 6);
    eth->type = htons(type);
    memcpy((uint8_t*)eth + sizeof(eth_header_t), data, len);
    e1000_send_packet(eth, total_len);
    tx_packet_count++;
    kfree(eth);
}

void net_send_arp_request(const uint8_t* target_ip) {
    arp_packet_t req;
    req.htype = htons(ARP_HTYPE_ETHERNET);
    req.ptype = htons(ARP_PTYPE_IPV4);
    req.hlen = 6;
    req.plen = 4;
    req.oper = htons(ARP_OP_REQUEST);
    memcpy(req.src_mac, my_mac, 6);
    memcpy(req.src_ip, my_ip, 4);
    memset(req.dst_mac, 0, 6);
    memcpy(req.dst_ip, target_ip, 4);
    net_send_eth(broadcast_mac, ETHERTYPE_ARP, &req, sizeof(arp_packet_t));
}

static void net_handle_arp(void* data, uint16_t len) {
    if (len < sizeof(arp_packet_t)) return;
    arp_packet_t* arp = (arp_packet_t*)data;

    if (ntohs(arp->htype) != ARP_HTYPE_ETHERNET || ntohs(arp->ptype) != ARP_PTYPE_IPV4) return;

    // Update Cache
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid || memcmp(arp_cache[i].ip, arp->src_ip, 4) == 0) {
            memcpy(arp_cache[i].ip, arp->src_ip, 4);
            memcpy(arp_cache[i].mac, arp->src_mac, 6);
            arp_cache[i].valid = 1;
            break;
        }
    }

    if (ntohs(arp->oper) == ARP_OP_REQUEST) {
        if (memcmp(arp->dst_ip, my_ip, 4) == 0) {
            arp_packet_t reply;
            reply.htype = htons(ARP_HTYPE_ETHERNET);
            reply.ptype = htons(ARP_PTYPE_IPV4);
            reply.hlen = 6;
            reply.plen = 4;
            reply.oper = htons(ARP_OP_REPLY);
            memcpy(reply.src_mac, my_mac, 6);
            memcpy(reply.src_ip, my_ip, 4);
            memcpy(reply.dst_mac, arp->src_mac, 6);
            memcpy(reply.dst_ip, arp->src_ip, 4);
            net_send_eth(arp->src_mac, ETHERTYPE_ARP, &reply, sizeof(arp_packet_t));
        }
    }
}

static void net_send_ipv4(const uint8_t* dest_ip, const uint8_t* dest_mac, uint8_t proto, const void* data, uint16_t len) {
    uint16_t total_len = sizeof(ipv4_packet_t) + len;
    ipv4_packet_t* ip = kmalloc(total_len);
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->len = htons(total_len);
    ip->id = htons(0);
    ip->frag_offset = htons(0);
    ip->ttl = 64;
    ip->proto = proto;
    ip->checksum = 0;
    memcpy(ip->src_ip, my_ip, 4);
    memcpy(ip->dst_ip, dest_ip, 4);
    ip->checksum = net_checksum(ip, sizeof(ipv4_packet_t));
    memcpy((uint8_t*)ip + sizeof(ipv4_packet_t), data, len);
    net_send_eth(dest_mac, ETHERTYPE_IPV4, ip, total_len);
    kfree(ip);
}

static void net_handle_icmp(const uint8_t* src_mac, ipv4_packet_t* ip, void* data, uint16_t len) {
    if (len < sizeof(icmp_packet_t)) return;
    icmp_packet_t* icmp = (icmp_packet_t*)data;
    if (icmp->type == ICMP_TYPE_ECHO_REQUEST) {
        uint16_t reply_len = len;
        icmp_packet_t* reply = kmalloc(reply_len);
        memcpy(reply, icmp, reply_len);
        reply->type = ICMP_TYPE_ECHO_REPLY;
        reply->checksum = 0;
        reply->checksum = net_checksum(reply, reply_len);
        net_send_ipv4(ip->src_ip, src_mac, IPV4_PROTO_ICMP, reply, reply_len);
        kfree(reply);
    } else if (icmp->type == ICMP_TYPE_ECHO_REPLY) {
        klog(LOG_INFO, "NET", "Received ICMP Echo Reply from %d.%d.%d.%d",
             ip->src_ip[0], ip->src_ip[1], ip->src_ip[2], ip->src_ip[3]);
    }
}

static void net_handle_udp(ipv4_packet_t* ip, void* data, uint16_t len) {
    if (len < sizeof(udp_packet_t)) return;
    udp_packet_t* udp = (udp_packet_t*)data;
    
    uint16_t src_port = ntohs(udp->src_port);
    uint16_t dst_port = ntohs(udp->dst_port);
    uint16_t udp_len = ntohs(udp->len);
    
    if (udp_len < sizeof(udp_packet_t) || udp_len > len) return;

    void* payload = udp->payload;
    uint16_t payload_len = udp_len - sizeof(udp_packet_t);

    klog(LOG_DEBUG, "NET", "UDP Packet: %d.%d.%d.%d:%d -> :%d, len=%d",
         ip->src_ip[0], ip->src_ip[1], ip->src_ip[2], ip->src_ip[3],
         src_port, dst_port, payload_len);

    for (int i = 0; i < UDP_MAX_BINDINGS; i++) {
        if (udp_bindings[i].valid && udp_bindings[i].port == dst_port) {
            udp_bindings[i].handler(ip->src_ip, src_port, dst_port, payload, payload_len);
            return;
        }
    }
}

static void net_handle_tcp(ipv4_packet_t* ip, void* data, uint16_t len) {
    if (len < sizeof(tcp_packet_t)) return;
    tcp_packet_t* tcp = (tcp_packet_t*)data;

    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dst_port = ntohs(tcp->dst_port);
    uint8_t  header_len = (tcp->off_res >> 4) * 4;

    if (header_len < sizeof(tcp_packet_t) || header_len > len) return;

    void* payload = (uint8_t*)data + header_len;
    uint16_t payload_len = len - header_len;

    if (net_trace_enabled) {
        klog(LOG_DEBUG, "NET", "TCP Packet: %d.%d.%d.%d:%d -> :%d, flags=0x%x, len=%d",
             ip->src_ip[0], ip->src_ip[1], ip->src_ip[2], ip->src_ip[3],
             src_port, dst_port, tcp->flags, payload_len);
    }

    for (int i = 0; i < TCP_MAX_BINDINGS; i++) {
        if (tcp_bindings[i].valid && tcp_bindings[i].port == dst_port) {
            tcp_bindings[i].handler(ip->src_ip, src_port, dst_port, tcp, payload, payload_len);
            return;
        }
    }
}

static void net_handle_ipv4(const uint8_t* src_mac, void* data, uint16_t len) {
    if (len < sizeof(ipv4_packet_t)) return;
    ipv4_packet_t* ip = (ipv4_packet_t*)data;
    if (memcmp(ip->dst_ip, my_ip, 4) != 0 && memcmp(ip->dst_ip, broadcast_mac, 4) != 0) return;
    void* payload = (uint8_t*)data + (ip->version_ihl & 0x0F) * 4;
    uint16_t payload_len = ntohs(ip->len) - (ip->version_ihl & 0x0F) * 4;
    if (ip->proto == IPV4_PROTO_ICMP) net_handle_icmp(src_mac, ip, payload, payload_len);
    else if (ip->proto == IPV4_PROTO_UDP) net_handle_udp(ip, payload, payload_len);
    else if (ip->proto == IPV4_PROTO_TCP) net_handle_tcp(ip, payload, payload_len);
}

void net_send_tcp(const uint8_t* target_ip, uint16_t src_port, uint16_t dst_port, uint32_t seq, uint32_t ack, uint8_t flags, const void* data, uint16_t len) {
    uint8_t* target_mac = NULL;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && memcmp(arp_cache[i].ip, target_ip, 4) == 0) {
            target_mac = arp_cache[i].mac;
            break;
        }
    }
    
    if (!target_mac) {
        klog(LOG_INFO, "NET", "TCP target MAC not in cache, sending ARP...");
        net_send_arp_request(target_ip);
        // In a real OS, we'd queue this. Here we just try to wait briefly.
        uint64_t start = pit_get_ticks();
        while (pit_get_ticks() - start < 10) { // 100ms
            e1000_poll();
            for (int i = 0; i < ARP_CACHE_SIZE; i++) {
                if (arp_cache[i].valid && memcmp(arp_cache[i].ip, target_ip, 4) == 0) {
                    target_mac = arp_cache[i].mac;
                    break;
                }
            }
            if (target_mac) break;
        }
    }

    if (!target_mac) {
        klog(LOG_ERROR, "NET", "TCP: ARP timeout for %d.%d.%d.%d", target_ip[0], target_ip[1], target_ip[2], target_ip[3]);
        return;
    }

    uint16_t total_len = sizeof(tcp_packet_t) + len;
    tcp_packet_t* tcp = kmalloc(total_len);
    memset(tcp, 0, total_len);

    tcp->src_port = htons(src_port);
    tcp->dst_port = htons(dst_port);
    tcp->seq = htonl(seq);
    tcp->ack = htonl(ack);
    tcp->off_res = (sizeof(tcp_packet_t) / 4) << 4;
    tcp->flags = flags;
    tcp->window = htons(8192);
    tcp->checksum = 0;

    memcpy(tcp->options, data, len);

    // TCP Checksum includes a pseudo-header
    struct {
        uint8_t src_ip[4];
        uint8_t dst_ip[4];
        uint8_t zero;
        uint8_t proto;
        uint16_t len;
    } __attribute__((packed)) pseudo;

    memcpy(pseudo.src_ip, my_ip, 4);
    memcpy(pseudo.dst_ip, target_ip, 4);
    pseudo.zero = 0;
    pseudo.proto = IPV4_PROTO_TCP;
    pseudo.len = htons(total_len);

    uint32_t sum = 0;
    uint16_t* p = (uint16_t*)&pseudo;
    for (int i = 0; i < 6; i++) sum += p[i];
    
    p = (uint16_t*)tcp;
    for (int i = 0; i < total_len / 2; i++) sum += p[i];
    if (total_len % 2) sum += ((uint8_t*)tcp)[total_len - 1];

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    tcp->checksum = (uint16_t)~sum;

    net_send_ipv4(target_ip, target_mac, IPV4_PROTO_TCP, tcp, total_len);
    kfree(tcp);
}

void net_send_udp(const uint8_t* target_ip, uint16_t src_port, uint16_t dst_port, const void* data, uint16_t len) {
    uint8_t* target_mac = NULL;
    uint8_t broadcast_ip[4] = {255, 255, 255, 255};
    
    if (memcmp(target_ip, broadcast_ip, 4) == 0) {
        target_mac = broadcast_mac;
    } else {
        for (int i = 0; i < ARP_CACHE_SIZE; i++) {
            if (arp_cache[i].valid && memcmp(arp_cache[i].ip, target_ip, 4) == 0) {
                target_mac = arp_cache[i].mac;
                break;
            }
        }
        
        if (!target_mac) {
            klog(LOG_INFO, "NET", "UDP target MAC not in cache, cannot send (ARP request should be sent here in full implementation).");
            // For simplicity, we drop it if not in ARP cache, 
            // a real stack would queue it and send ARP.
            return; 
        }
    }

    uint16_t total_len = sizeof(udp_packet_t) + len;
    
    if (net_trace_enabled) {
        klog(LOG_INFO, "NET", "TX UDP: %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d, Len=%d",
             my_ip[0], my_ip[1], my_ip[2], my_ip[3], src_port,
             target_ip[0], target_ip[1], target_ip[2], target_ip[3], dst_port, len);
    }

    udp_packet_t* udp = kmalloc(total_len);
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->len = htons(total_len);
    udp->checksum = 0; // Optional in IPv4

    memcpy(udp->payload, data, len);

    net_send_ipv4(target_ip, target_mac, IPV4_PROTO_UDP, udp, total_len);
    kfree(udp);
}

void net_handle_packet(void* data, uint16_t len) {
    if (len < sizeof(eth_header_t)) return;
    rx_packet_count++;
    eth_header_t* eth = (eth_header_t*)data;
    uint16_t type = ntohs(eth->type);

    if (net_trace_enabled) {
        klog(LOG_INFO, "NET", "RX Eth: Src=%x:%x:%x:%x:%x:%x, Type=0x%x, Len=%d",
             (int)eth->src[0], (int)eth->src[1], (int)eth->src[2], 
             (int)eth->src[3], (int)eth->src[4], (int)eth->src[5],
             (int)type, (int)len);
    }

    void* payload = (uint8_t*)data + sizeof(eth_header_t);
    uint16_t payload_len = len - sizeof(eth_header_t);
    if (type == ETHERTYPE_ARP) net_handle_arp(payload, payload_len);
    else if (type == ETHERTYPE_IPV4) net_handle_ipv4(eth->src, payload, payload_len);
}

void net_ping(const uint8_t* target_ip) {
    // Check ARP cache
    uint8_t* target_mac = NULL;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && memcmp(arp_cache[i].ip, target_ip, 4) == 0) {
            target_mac = arp_cache[i].mac;
            break;
        }
    }

    if (!target_mac) {
        klog(LOG_INFO, "NET", "Target MAC not in cache, sending ARP request...");
        net_send_arp_request(target_ip);
        // Wait for reply (busy wait for simplicity)
        uint64_t start = pit_get_ticks();
        while (pit_get_ticks() - start < 100) { // 1 second timeout
            e1000_poll(); // Must poll to receive the reply
            for (int i = 0; i < ARP_CACHE_SIZE; i++) {
                if (arp_cache[i].valid && memcmp(arp_cache[i].ip, target_ip, 4) == 0) {
                    target_mac = arp_cache[i].mac;
                    break;
                }
            }
            if (target_mac) break;
            __asm__ ("pause");
        }
    }

    if (!target_mac) {
        klog(LOG_ERROR, "NET", "Ping: ARP timeout");
        return;
    }

    klog(LOG_INFO, "NET", "Sending ICMP Echo Request to %d.%d.%d.%d",
         target_ip[0], target_ip[1], target_ip[2], target_ip[3]);

    icmp_packet_t req;
    req.type = ICMP_TYPE_ECHO_REQUEST;
    req.code = 0;
    req.checksum = 0;
    req.id = htons(0x1234);
    req.seq = htons(1);
    req.checksum = net_checksum(&req, sizeof(icmp_packet_t));

    net_send_ipv4(target_ip, target_mac, IPV4_PROTO_ICMP, &req, sizeof(icmp_packet_t));
}

uint64_t net_get_rx_count() { return rx_packet_count; }
uint64_t net_get_tx_count() { return tx_packet_count; }
void net_get_ip(uint8_t* ip) { memcpy(ip, my_ip, 4); }

void net_set_ip(const uint8_t* ip) {
    memcpy(my_ip, ip, 4);
    klog(LOG_INFO, "NET", "IP Address updated to %d.%d.%d.%d",
         my_ip[0], my_ip[1], my_ip[2], my_ip[3]);
}
