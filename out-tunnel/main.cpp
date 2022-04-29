#include <iostream>

#include "quic_server.h"

int main(int argc, char *argv[])
{
  constexpr auto QUIC_TUNNEL_PORT = 8888;
  constexpr auto TURN_PORT        = 3478;
  constexpr auto TURN_HOSTNAME    = "localhost";

  UdpSocket udp_socket{TURN_HOSTNAME, TURN_PORT};
  QuicServer qserver{ "127.0.0.1", QUIC_TUNNEL_PORT, &udp_socket };

  qserver.start();
  
  return 0;
}

