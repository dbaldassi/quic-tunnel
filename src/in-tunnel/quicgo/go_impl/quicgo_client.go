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
	return quicSession.sess.CloseWithError(0, "eos")
}

func sendDatagram(d *datagram) error {
	// buf := bytes.Buffer{}
	// quicvarint.Write(&buf, d.flowID)
	// buf.Write(d.data)
	return quicSession.sess.SendMessage(d.data)
}

//export goClientStart
func goClientStart(datagrams bool, wrapper *C.wrapper_t) {
	fmt.Println("Starting quic go client")

	tlsConf := &tls.Config{
		InsecureSkipVerify: true,
		NextProtos: []string{"quic-echo-example"},
	}

	quicConf := &quic.Config{
		EnableDatagrams: datagrams,
	}

	conn, err := quic.DialAddr(addr, tlsConf, quicConf)
	if err != nil {
		panic(err)
	}

	// quicSession = &Session{ conn, sync.RWMutex{}, make(map[uint64]*ReadFlow) };
	quicSession = &Session{ conn };

	go func() {
		for {
			message, err := quicSession.sess.ReceiveMessage()
			if err != nil {
				fmt.Printf("failed to receive message: %v\n", err)
				return
			}
			// fmt.Println(string(message[:]))
			msg:= string(message[:])
			C.callback(wrapper, C.CString(msg), C.int(len(msg)))
		}
	}()
}

//export goClientStop
func goClientStop() {
	Close()
}

//export goClientSendMessageStream
func goClientSendMessageStream(buf *C.char, len uint64) {
	
}

//export goClientSendMessageDatagram
func goClientSendMessageDatagram(buf *C.char, len uint64) {
	bufGo:= C.GoBytes(unsafe.Pointer(buf), C.int(len))
	sendDatagram(&datagram{0, bufGo})
}

func main() {
	fmt.Println("Main being called")
}
