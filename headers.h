#ifndef HEADERS_H
#define HEADERS_H

#include <stdint.h>
#include <netinet/in.h>

// IP Header (20 bytes)
struct ipheader {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned int iph_ihl:4;
    unsigned int iph_ver:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
    unsigned int iph_ver:4;
    unsigned int iph_ihl:4;
#else
# error "Please fix <bits/endian.h>"
#endif
    uint8_t  iph_tos;
    uint16_t iph_len;
    uint16_t iph_ident;
    uint16_t iph_offset;
    uint8_t  iph_ttl;
    uint8_t  iph_protocol;
    uint16_t iph_chksum;
    uint32_t iph_sourceip;
    uint32_t iph_destip;
};

// TCP Header (20 bytes)
struct tcpheader {
    uint16_t th_sport;  // Source port
    uint16_t th_dport;  // Destination port
    uint32_t th_seq;    // Sequence number
    uint32_t th_ack;    // Acknowledgment number
    uint8_t  th_offx2;  // Data offset and reserved bits
    uint8_t  th_flags;
    uint16_t th_win;    // Window
    uint16_t th_sum;    // Checksum
    uint16_t th_urp;    // Urgent pointer
};

// UDP Header (8 bytes)
struct udpheader {
    uint16_t udph_srcport;   // Source port
    uint16_t udph_destport;  // Destination port
    uint16_t udph_len;       // Length
    uint16_t udph_chksum;    // Checksum
};


// ICMP Header (8 bytes minimum)
struct icmpheader {
    uint8_t  icmp_type;
    uint8_t  icmp_code;
    uint16_t icmp_cksum;
    uint16_t icmp_id;
    uint16_t icmp_seq;
};

#endif // HEADERS_H
