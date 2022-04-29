
#include <iostream>

#include <string.h>

#include "udp_socket.h"

UdpSocket::UdpSocket(int port) noexcept : _port(port), _socket(-1)
{
  _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  
  struct sockaddr_in addr;
  
  memset((char *)&addr, 0, sizeof(addr));
  memset((char *)&_addr_other, 0, sizeof(_addr_other));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if(_socket == -1) {
    perror("Could not create UDP socket");
  }
  else if(bind(_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("Could not bind UDP socket");
  }
}

ssize_t UdpSocket::recv() noexcept
{
  if(_socket == -1) return -1;
  
  struct sockaddr_in addr;
  socklen_t          slen;
  
  auto rec_len = recvfrom(_socket, _buf, MAX_BUF_LEN, 0, (struct sockaddr*)&addr, &slen);

  _addr_other     = addr;
  _addr_other_len = slen;

  if(rec_len == -1) perror("Could not receive data in UDP socket");
  
  return rec_len;
}

bool UdpSocket::send_back(const char * buf, size_t len)
{
  if(_socket == -1) return false;
  
  auto send_len = sendto(_socket, buf, len, 0, (struct sockaddr *)&_addr_other, _addr_other_len);
  if(send_len == -1) {
    perror("Could not send back");
    return false;
  }

  return true;
}

void UdpSocket::close()
{
  if(_socket != -1) {
    ::close(_socket);
    _socket = -1;
  }
}

UdpSocket::~UdpSocket() noexcept
{
  close();
}
