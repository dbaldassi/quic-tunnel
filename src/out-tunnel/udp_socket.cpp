
#include <stdio.h>
#include <string.h>

#include "udp_socket.h"

namespace out
{

UdpSocket::UdpSocket(const char* hostname, int port) : _port(port), _socket(-1), _host(NULL)
{
  memset((char *)&_addr, 0, sizeof(_addr));

  _host = gethostbyname(hostname);
  if(!_host) perror("Could not get host by name");
  else _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
  if(_socket == -1) perror("Could not create UDP socket");
  
  _addr.sin_family = AF_INET;
  _addr.sin_port   = htons(_port);
  memcpy(&_addr.sin_addr, _host->h_addr_list[0], _host->h_length);
}

void UdpSocket::start()
{
  _recv_thread = std::thread([this](){
    struct sockaddr_in addr_tmp;
    socklen_t          slen_tmp;

    while(true) {			       
      auto rlen = recvfrom(_socket, _buf, MAX_BUF_LEN, 0,
			   (struct sockaddr *) &addr_tmp, &slen_tmp);
      if(rlen == -1) perror("Could not receive data in UDP socket");
      else if(_callback) _callback->onUdpMessage(_buf, rlen);
    }
  });
}

void UdpSocket::send(const char *buf, size_t len)
{
  auto l = sendto(_socket, buf, len, 0, (struct sockaddr *) &_addr, sizeof(_addr));
  if(l == -1) perror("Could not send data in UDP socket");
}

UdpSocket::~UdpSocket()
{
  if(_socket != -1) {
    close(_socket);
    _socket = -1;
    _recv_thread.join();
  }
}

}
