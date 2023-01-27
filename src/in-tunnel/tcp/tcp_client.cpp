#include <unistd.h>
#include <sstream>
#include <csignal>
#include <wait.h>
#include <sys/socket.h>
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <thread>

#include "tcp_client.h"

#include <iostream>
#include <linux/tcp.h>

void TcpClient::start()
{
  _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);


  int optval = 1;
  int optlen = sizeof(optval);
  
  setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &optval, optlen);
  
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(_dst_host.c_str());
  addr.sin_port = htons(_dst_port);

  connect(_socket, (struct sockaddr*)&addr, sizeof(addr));

  std::thread([this](){
    constexpr size_t MAX = 2048;
    char buf[MAX];

    for(;;) {
      auto res = read(_socket, buf, MAX);
      // std::cout << "received : " << res << "\n";
      
      if(res < 0) {
	perror("Error reading tcp socket");
	break;
      }

      if(_on_received_callback) _on_received_callback(buf, res);
    }
  }).detach();
}

void TcpClient::stop()
{
  close(_socket);
}

Capabilities TcpClient::get_capabilities()
{
  Capabilities cap;
  cap.cc.emplace_back("newreno");

  cap.datagrams = false;
  
  cap.impl = "tcp";

  return cap;
}


void TcpClient::send_message_stream(const char * buffer, size_t len)
{
  // std::ostringstream oss;
  // oss << buffer << "\n";

  // std::cout << "sending : " << len << "\n";
  
  // write(_socket, oss.str().c_str(), len + 1);
  if(write(_socket, buffer, len) < 0) {
    perror("tcp socket write error");
  }
  // write(_socket, "\n", 1);
}
