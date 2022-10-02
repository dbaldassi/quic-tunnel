package main

/*
#cgo CPPFLAGS: -Wno-overflow -Wno-write-strings
#include "callback.h"
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
	"context"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"encoding/pem"
	"io"
	"os"
	"log"
	"strings"
	"math/big"

	"github.com/lucas-clemente/quic-go"
	"github.com/lucas-clemente/quic-go/logging"
	"github.com/lucas-clemente/quic-go/qlog"
)

// const addr = "localhost:4242"

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


func getFileLogWriter(path string) (io.WriteCloser, error) {
	logfile, err := os.Create(path)
	if err != nil {
		return nil, err
	}
	return logfile, nil
}

// GetQLOGWriter creates the QLOGDIR and returns the GetLogWriter callback
func GetQLOGWriter() (func(perspective logging.Perspective, connID []byte) io.WriteCloser, error) {
	qlogDir := os.Getenv("QLOGDIR")
	fmt.Println("GetQLOG DIR")
	if len(qlogDir) == 0 {
		return nil, nil
	}
	_, err := os.Stat(qlogDir)
	if err != nil {
		if os.IsNotExist(err) {
			if err = os.MkdirAll(qlogDir, 0o775); err != nil {
				return nil, fmt.Errorf("failed to create qlog dir %s: %v", qlogDir, err)
			}
		} else {
			return nil, err
		}
	}
	return func(_ logging.Perspective, connID []byte) io.WriteCloser {
		path := fmt.Sprintf("%s/%x.qlog", strings.TrimRight(qlogDir, "/"), connID)
		w, err := getFileLogWriter(path)
		if err != nil {
			log.Printf("failed to create qlog file %s: %v", path, err)
			return nil
		}
		log.Printf("created qlog file: %s\n", path)
		return w
	}, nil
}

func sendDatagram(d *datagram) error {
	return quicSession.sess.SendMessage(d.data)
}

const streamDataPacketLength = 2048

//export goServerStart
func goServerStart(addr *C.char, datagrams bool, wrapper *C.wrapper_t) {
	fmt.Println("Starting quic go server ")
	quicConf := &quic.Config{
		EnableDatagrams: true,
	}
	qlogWriter, err := GetQLOGWriter()
	if qlogWriter != nil {
		quicConf.Tracer = qlog.NewTracer(qlogWriter)
	}
	
	listener, err := quic.ListenAddr(C.GoString(addr), generateTLSConfig(), quicConf)
	if err != nil {
		panic(err)
	}
	conn, err := listener.Accept(context.Background())	
	if err != nil {
		panic(err)
	}

	quicSession = &Session{ conn };
	
	go func() {
		for {
			stream, err := quicSession.sess.AcceptUniStream(context.Background())
			if err != nil {
				if err.Error() == "Application error 0x0: normal shutdown" {
					fmt.Println("Shutdown server")
					return
				} else {
					panic(err)
				}
			}

			buffer := make([]byte, streamDataPacketLength)
			for {				
				n, err := stream.Read(buffer)
				if err == io.EOF {
					C.callback(wrapper, C.CString(string(buffer[:n])), C.int(n))
					break
				} else if err != nil {
					break
				}				
			}
		}
	}()
	
	for {
		message, err := quicSession.sess.ReceiveMessage()
		if err != nil {
			if err.Error() == "Application error 0x0: normal shutdown" {
				fmt.Println("Shutdown server")
				return
			} else {
				panic(err)
			}
		}
		msg:= string(message[:]) // TODO: optimize this copy
		C.callback(wrapper, C.CString(msg), C.int(len(msg)))
	}
	fmt.Println("The end ?")	
}

//export goServerStop
func goServerStop() {
	quicSession.sess.CloseWithError(0, "normal shutdown")
}


//export goServerSendMessageStream
func goServerSendMessageStream(buf *C.char, len uint64) {
	stream, err := quicSession.sess.OpenUniStream();
	if err != nil {
		panic(err)
	}

	stream.Write(C.GoBytes(unsafe.Pointer(buf), C.int(len)))
	stream.Close()
}

//export goServerSendMessageDatagram
func goServerSendMessageDatagram(buf *C.char, len uint64) {
	if len <= 1200 {
		bufGo:= C.GoBytes(unsafe.Pointer(buf), C.int(len))
		sendDatagram(&datagram{0, bufGo})
	} else {
		goServerSendMessageStream(buf, len)
	}
}

func main() {

}