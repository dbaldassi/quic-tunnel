#include <iostream>

#include <glog/logging.h>

#include <fizz/crypto/Utils.h>
#include <folly/init/Init.h>

#include "quic_client.h"
#include "udp_socket.h"

int run_client(int turn_port, uint16_t quic_port)
{  
  in::UdpSocket  udp_socket{turn_port};
  QuicClient qclient{"127.0.0.1", quic_port};  
  
  qclient.set_on_received_callback([&udp_socket](const std::string& msg) {
    udp_socket.send_back(msg.c_str(), msg.size());
  });
  
  qclient.start();
  
  while(true) {
    auto len = udp_socket.recv();
    if(len == -1) return 0;

    const char * buffer = udp_socket.get_buffer();
    
    qclient.send_message_datagram(buffer, len);
  }

  return 0;
}

