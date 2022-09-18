package main

import "C"

import (
	"fmt"
	// "strings"
	"context"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"encoding/pem"
	"io"
	"time"
	"math/big"

	"github.com/lucas-clemente/quic-go"
)

const addr = "localhost:4242"

type loggingWriter struct { io.Writer }
func (w loggingWriter) Write(b []byte) (int, error) {
	fmt.Printf("server: got '%s'\n", string(b));
	return w.Writer.Write(b)
}

type loggingReader struct { io.Reader }
func (r loggingReader) Read(buf []byte) (int, error) {
	const resp = "recu chef"
	buf = []byte(resp)

	return len(buf), nil
}

func ff() {
	fmt.Println("Hello World");
	listener, err := quic.ListenAddr(addr, generateTLSConfig(), nil)
	if err != nil {
		panic(err)
	}
	conn, err := listener.Accept(context.Background())
	if err != nil {
		panic(err)
	}
	stream, err := conn.AcceptStream(context.Background())
	if err != nil {
		panic(err)
	}

	buf := make([]byte, 1024)
	stream.Read(buf)
	// _, err = io.Copy(loggingWriter{stream}, strings.NewReader("recu chef\n"))
	stream.Write([]byte("reçu chef"));

	time.Sleep(5 * time.Second)
	
	fmt.Println("The End ?")
}

//export goServerStart
func goServerStart() {
	fmt.Println("Hello server")
}


func main() {

}

func generateTLSConfig() *tls.Config {
	key, err := rsa.GenerateKey(rand.Reader, 1024)
	if err != nil {
		panic(err)
	}
	template := x509.Certificate{SerialNumber: big.NewInt(1)}
	certDER, err := x509.CreateCertificate(rand.Reader, &template, &template, &key.PublicKey, key)
	if err != nil {
		panic(err)
	}
	keyPEM := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(key)})
	certPEM := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: certDER})

	tlsCert, err := tls.X509KeyPair(certPEM, keyPEM)
	if err != nil {
		panic(err)
	}
	return &tls.Config{
		Certificates: []tls.Certificate{tlsCert},
		NextProtos:   []string{"quic-echo-example"},
	}
}
