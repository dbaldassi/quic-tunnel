#include <iostream>

#include "quic_server.h"

int run_quic_server(const char * turn_hostname, int turn_port, uint16_t quic_port)
{
  out::UdpSocket udp_socket{turn_hostname, turn_port};
  QuicServer qserver{"127.0.0.1", quic_port, &udp_socket};

  qserver.start();
  
  return 0;
}

