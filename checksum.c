#include "checksum.h"
#include <string.h>
#include <arpa/inet.h>

// Basic checksum function (used for IP header)
unsigned short calculate_checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (; nwords > 0; nwords--) {
        sum += *buf++;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);

    return (unsigned short)(~sum);
}


unsigned short tcp_udp_checksum(const void *buff, size_t len, uint32_t src_addr, uint32_t dest_addr, uint8_t proto) {
    const uint16_t *buf = buff;
    uint32_t sum = 0;
    size_t length = len;


    sum += (src_addr >> 16) & 0xFFFF;
    sum += (src_addr) & 0xFFFF;
    sum += (dest_addr >> 16) & 0xFFFF;
    sum += (dest_addr) & 0xFFFF;
    sum += htons(proto);
    sum += htons(length);


    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    if (len == 1) {
        sum += *((uint8_t *)buf);
    }


    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}
