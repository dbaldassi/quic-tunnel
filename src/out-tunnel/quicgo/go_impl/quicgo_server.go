package main

/*
#cgo CPPFLAGS: -Wno-overflow -Wno-write-strings
#include "callback.h"
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	// "bytes"
	// "sync"
	// "strings"
	"unsafe"
	"context"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"encoding/pem"
	"io"
	"math/big"

	"github.com/lucas-clemente/quic-go"
	// "github.com/lucas-clemente/quic-go/quicvarint"
	// "github.com/pion/transport/packetio"
)

const addr = "localhost:4242"

type datagram struct {
	flowID uint64
	data   []byte
}

type Session struct {
	sess quic.Connection

	// readFlowsLock sync.RWMutex
	// readFlows     map[uint64]*ReadFlow
}

// func (s *Session) getFlow(id uint64) *ReadFlow {
// 	s.readFlowsLock.RLock()
// 	defer s.readFlowsLock.RUnlock()

// 	return s.readFlows[id]
// }

// func (s *Session) AcceptFlow(flowID uint64) (*ReadFlow, error) {
// 	rf := &ReadFlow{
// 		session: s,
// 		buffer:  packetio.NewBuffer(),
// 	}
// 	s.readFlowsLock.Lock()
// 	s.readFlows[flowID] = rf
// 	s.readFlowsLock.Unlock()
// 	return rf, nil
// }

type ReadFlow struct {
	session *Session
	buffer  io.ReadWriteCloser
}

func (r *ReadFlow) close() error {
	return r.buffer.Close()
}

func (r *ReadFlow) write(buf []byte) (int, error) {
	n, err := r.buffer.Write(buf)
	return n, err
}

func (r *ReadFlow) Read(buf []byte) (n int, err error) {
	return r.buffer.Read(buf)
}

var quicSession *Session

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

func sendDatagram(d *datagram) error {
	// buf := bytes.Buffer{}
	// quicvarint.Write(&buf, d.flowID)
	// buf.Write(d.data)
	return quicSession.sess.SendMessage(d.data)
}

//export goServerStart
func goServerStart(datagrams bool, wrapper *C.wrapper_t) {
	fmt.Println("Starting quic go server")
	quicConf := &quic.Config{
		EnableDatagrams: true,
	}
	listener, err := quic.ListenAddr(addr, generateTLSConfig(), quicConf)
	if err != nil {
		panic(err)
	}
	conn, err := listener.Accept(context.Background())	
	if err != nil {
		panic(err)
	}

	// quicSession := &Session{ conn, sync.RWMutex{}, make(map[uint64]*ReadFlow) };
	quicSession = &Session{ conn };
	// quicSession.AcceptFlow(0)
	
	fmt.Println("Start receiving message")
	for {
		message, err := quicSession.sess.ReceiveMessage()
		if err != nil {
			// if err.Error() == "Application error 0x0: eos" {
			// 	quicSession.readFlowsLock.Lock()
			// 	for _, flow := range quicSession.readFlows {
			// 		err = flow.close()
			// 		if err != nil {
			// 			fmt.Printf("failed to close flow: %s\n", err)
			// 		}
			// 	}
			// 	quicSession.readFlowsLock.Unlock()
			// 	return
			// }
			fmt.Printf("failed to receive message: %v\n", err)
			return
		}
		// fmt.Println(string(message[:]))
		msg:= string(message[:])
		C.callback(wrapper, C.CString(msg), C.int(len(msg)))

		// below not needed I think
		// reader := bytes.NewReader(message)
		// flowID, err := quicvarint.Read(reader)
		// if err != nil {
		// 	fmt.Printf("failed to parse flow identifier from message of length: %v: %s\n", len(message), err)
		// 	return
		// }
		// flow := quicSession.getFlow(flowID)
		// if flow == nil {
		// 	// TODO: Create flow?
		// 	fmt.Printf("dropping message for unknown flow, forgot to create flow with ID %v?", flowID)
		// 	continue
		// }
		// _, err = flow.write(message[quicvarint.Len(flowID):])
		// if err != nil {
		// 	// TODO: Handle error?
		// 	fmt.Printf("// TODO: handler flow.write error: %s\n", err)
		// 	return
		// }
	}
	fmt.Println("The end ?")	
}

//export goServerStop
func goServerStop() {

}


//export goServerSendMessageStream
func goServerSendMessageStream(buf *C.char, len uint64) {
	
}

//export goServerSendMessageDatagram
func goServerSendMessageDatagram(buf *C.char, len uint64) {
	// bufGoStr := C.GoStringN(buf, C.int(len))
	bufGo:= C.GoBytes(unsafe.Pointer(buf), C.int(len))
	sendDatagram(&datagram{0, bufGo})
}

func main() {

}
