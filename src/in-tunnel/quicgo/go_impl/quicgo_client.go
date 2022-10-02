package main

/*
#cgo CPPFLAGS: -Wno-overflow -Wno-write-strings
#include "callback.h"
#include <stdlib.h>
*/
import "C"

import (
	"crypto/tls"
	"fmt"
	"unsafe"
	"context"
	//"bytes"
	// "sync"
	"io"

	"github.com/lucas-clemente/quic-go"
	// "github.com/lucas-clemente/quic-go/quicvarint"
)

const addr = "localhost:4242"
const message = "foobar"

type datagram struct {
	flowID uint64
	data   []byte
}

type Session struct {
	sess quic.Connection

	// readFlowsLock sync.RWMutex
	// readFlows     map[uint64]*ReadFlow
}

type ReadFlow struct {
	session *Session
	buffer  io.ReadWriteCloser
}

var quicSession *Session

func Close() error {
	return quicSession.sess.CloseWithError(0, "normal shutdown")
}

func sendDatagram(d *datagram) error {
	return quicSession.sess.SendMessage(d.data)
}

const streamDataPacketLength = 2048

//export goClientStart
func goClientStart(addr *C.char, datagrams bool, wrapper *C.wrapper_t) {
	fmt.Println("Starting quic go client", C.GoString(addr))

	tlsConf := &tls.Config{
		InsecureSkipVerify: true,
		NextProtos: []string{"quic-echo-example"},
	}

	quicConf := &quic.Config{
		EnableDatagrams: datagrams,
	}

	conn, err := quic.DialAddr(C.GoString(addr), tlsConf, quicConf)
	if err != nil {
		panic(err)
	}

	quicSession = &Session{ conn };

	go func() {
		for {
			message, err := quicSession.sess.ReceiveMessage()
			if err != nil {
				if err.Error() == "Application error 0x0: normal shutdown" {
					fmt.Println("Shutdown client")
					return
				} else {
					panic(err)
				}
			}
			// fmt.Println(string(message[:]))
			msg:= string(message[:])
			C.callback(wrapper, C.CString(msg), C.int(len(msg)))
		}
	}()

	go func() {
		for {
			ctx := context.Background()
			stream, err := quicSession.sess.AcceptUniStream(ctx)
			if err != nil {
				if err.Error() == "Application error 0x0: normal shutdown" {
					fmt.Println("Shutdown client")
					return
				} else {
					panic(err)
				}
			}

			exit := false
			buffer := make([]byte, streamDataPacketLength)
			for {
				select {
				case <-ctx.Done():
					exit=true
					break
				default:					
					n, err := stream.Read(buffer)
					if err == io.EOF {
						exit = true
						C.callback(wrapper, C.CString(string(buffer[:n])), C.int(n))
					} else if err != nil {
						exit = true
						break
					}
				}
				if exit {
					break
				}
			}
		}
	}()
}

//export goClientStop
func goClientStop() {
	Close()
}

//export goClientSendMessageStream
func goClientSendMessageStream(buf *C.char, len uint64) {
	stream, err := quicSession.sess.OpenUniStream();
	if err != nil {
		panic(err)
	}

	stream.Write(C.GoBytes(unsafe.Pointer(buf), C.int(len)))
	stream.Close()
}

//export goClientSendMessageDatagram
func goClientSendMessageDatagram(buf *C.char, len uint64) {
	if len <= 1200 {
		bufGo:= C.GoBytes(unsafe.Pointer(buf), C.int(len))
		sendDatagram(&datagram{0, bufGo})
	} else {
		goClientSendMessageStream(buf, len)
	}
}

func main() {
	fmt.Println("Main being called")
}
