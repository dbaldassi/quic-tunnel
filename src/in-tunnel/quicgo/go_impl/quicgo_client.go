package main

import "C"

import (
	"context"
	"crypto/tls"
	"fmt"

	"github.com/lucas-clemente/quic-go"
)

const addr = "localhost:4242"
const message = "foobar"

func qq() {
	tlsConf := &tls.Config{
		InsecureSkipVerify: true,
		NextProtos: []string{"quic-echo-example"},
	}

	conn, err := quic.DialAddr(addr, tlsConf, nil)
	if err != nil {
		panic(err)
	}
	
	stream, err := conn.OpenStreamSync(context.Background())
	if err != nil {
		panic(err)
	}
	
	fmt.Printf("Client: Sending '%s'\n", message)
	_, err = stream.Write([]byte(message))
	// err = stream.Close()
	if err != nil {
		panic(err)
	}

	buf := make([]byte, 1024)
	_, err = stream.Read(buf)
	if err != nil {
		panic(err)
	}

	fmt.Printf("Client: Got '%s'\n", buf)
}

//export goStart
func goStart() {
	fmt.Println("Start lolilol")
}

func main() {

}
