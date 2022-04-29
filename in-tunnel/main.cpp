#include <iostream>

#include <glog/logging.h>

#include <fizz/crypto/Utils.h>
#include <folly/init/Init.h>

#include "quic_client.h"
#include "udp_socket.h"

#include "csignal"

struct Data
{
  UdpSocket * socket;
} data;

void trap(int sig)
{
  LOG(INFO) << "Closing QUIC tunnel";
  data.socket->close();
}

int main(int argc, char *argv[])
{
  constexpr auto UDP_TURN_PORT    = 3479;
  constexpr auto QUIC_TUNNEL_PORT = 8888;
  
  UdpSocket  udp_socket{UDP_TURN_PORT};
  QuicClient qclient{"127.0.0.1", QUIC_TUNNEL_PORT};  

  data.socket = &udp_socket;
  
  signal(SIGINT, trap);
  signal(SIGTERM, trap);

  qclient.set_on_received_callback([&udp_socket](const std::string& msg) {
    udp_socket.send_back(msg.c_str(), msg.size());
  });
  
  qclient.start();
  
  while(true) {
    auto len = udp_socket.recv();
    if(len == -1) return 0;

    const char * buffer = udp_socket.get_buffer();
    
    qclient.send_message_stream(buffer, len);
  }

  return 0;
}

