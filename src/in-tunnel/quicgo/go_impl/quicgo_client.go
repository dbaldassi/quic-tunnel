package main

/*
#cgo CPPFLAGS: -Wno-overflow -Wno-write-strings -I../../..
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
	"os"
	"log"
	"strings"

	"github.com/lucas-clemente/quic-go"
	"github.com/lucas-clemente/quic-go/logging"
	"github.com/lucas-clemente/quic-go/qlog"
)

const addr = "localhost:4242"
const message = "foobar"

type datagram struct {
	flowID uint64
	data   []byte
}

type Session struct {
	sess quic.Connection
	wrapper *C.wrapper_t
}

type ReadFlow struct {
	session *Session
	buffer  io.ReadWriteCloser
}

var quicSession *Session

func getFileLogWriter(path string) (io.WriteCloser, error) {
	logfile, err := os.Create(path)
	if err != nil {
		return nil, err
	}
	return logfile, nil
}

// GetQLOGWriter creates the QLOGDIR and returns the GetLogWriter callback
func GetQLOGWriter(qlogDir string, wrapper *C.wrapper_t) (func(perspective logging.Perspective, connID []byte) io.WriteCloser, error) {
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
		filename := fmt.Sprintf("%x.qlog", connID)
		w, err := getFileLogWriter(path)
		if err != nil {
			log.Printf("failed to create qlog file %s: %v", path, err)
			return nil
		}
		log.Printf("created qlog file: %s\n", path)
		C.on_qlog_filename(wrapper, C.CString(filename), C.int(len(filename)))
		return w
	}, nil
}

func Close() error {
	return quicSession.sess.CloseWithError(0, "normal shutdown")
}

func sendDatagram(d *datagram) error {
	return quicSession.sess.SendMessage(d.data)
}

const streamDataPacketLength = 2048

//export goClientStart
func goClientStart(addr *C.char, datagrams bool, qlogDir *C.char, wrapper *C.wrapper_t) {
	fmt.Println("Starting quic go client", C.GoString(addr))

	tlsConf := &tls.Config{
		InsecureSkipVerify: true,
		NextProtos: []string{"quic-echo-example"},
	}

	quicConf := &quic.Config{
		EnableDatagrams: datagrams,
	}
	qlogWriter, err := GetQLOGWriter(C.GoString(qlogDir), wrapper)
	if qlogWriter != nil {
		quicConf.Tracer = qlog.NewTracer(qlogWriter)
	}

	conn, err := quic.DialAddr(C.GoString(addr), tlsConf, quicConf)
	if err != nil {
		panic(err)
	}

	quicSession = &Session{ conn, wrapper };

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
			C.on_message_received(wrapper, C.CString(msg), C.int(len(msg)))
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
						C.on_message_received(wrapper, C.CString(string(buffer[:n])), C.int(n))
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
