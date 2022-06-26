
#include <iostream>

#include <string.h>
#include <sys/time.h>

#include "udp_socket.h"

namespace in
{

UdpSocket::UdpSocket() noexcept : _socket(-1)
{}

void UdpSocket::open(int port)
{
  _port = port;
  
  _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  struct timeval optval;
  optval.tv_sec = 5; // 5 seconds timeout
  
  setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &optval, sizeof(optval));
  
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
  socklen_t          slen = sizeof(addr);
  ssize_t rec_len;
  
  do {
    rec_len = recvfrom(_socket, _buf, MAX_BUF_LEN, 0, (struct sockaddr*)&addr, &slen);

    if(rec_len == -1) {
      if(errno == EAGAIN || errno == EWOULDBLOCK) {
	if(_socket == -1) {
	  puts("Closing UDP socket");
	  return -1;
	}
	else continue;
      }
      else {
	perror("Could not receive data in UDP socket");
	return -1;
      }
    }
  } while(rec_len < 0);

  // _addr_other = addr;
  _addr_other.sin_family = AF_INET;
  _addr_other.sin_port = addr.sin_port;
  _addr_other.sin_addr.s_addr = addr.sin_addr.s_addr;
  _addr_other_len = slen;

  return rec_len;
}

bool UdpSocket::send_back(const char * buf, size_t len)
{
  if(_socket == -1) return false;

  // puts("Sendign back some good data");
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

}
