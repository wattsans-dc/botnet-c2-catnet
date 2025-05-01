#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>
#include <stddef.h>

// Calculate IP/TCP/UDP checksum
unsigned short calculate_checksum(unsigned short *buf, int nwords);

// TCP/UDP checksum with pseudo header
unsigned short tcp_udp_checksum(const void *buff, size_t len, uint32_t src_addr, uint32_t dest_addr, uint8_t proto);

#endif
