#include <iostream>
#include <cmath>

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "udp_socket.h"

namespace out
{

UdpSocket::UdpSocket(const char* hostname, int port) : _port(port), _socket(-1), _host(NULL)
{
  memset((char *)&_addr, 0, sizeof(_addr));

  std::cout << "out::UdpSocket dst to " << hostname << " " << port << "\n";
  
  _host = gethostbyname(hostname);
  if(!_host) perror("Could not get host by name");
  else _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
  if(_socket == -1) {
    perror("Could not create UDP socket");
    std::exit(EXIT_FAILURE);
  }
  
  _addr.sin_family = AF_INET;
  _addr.sin_port   = htons(_port);
  memcpy(&_addr.sin_addr, _host->h_addr_list[0], _host->h_length);

  struct timeval optval;
  optval.tv_usec = 0;
  optval.tv_sec = 5; // 5 seconds timeout
  
  if(setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &optval, sizeof(optval)) < 0)
    perror("Could not set timeout");

  _start = false;
}

void UdpSocket::start()
{
  _start = true;
  _recv_thread = std::thread([this](){
    struct sockaddr_in addr_tmp;
    socklen_t          slen_tmp = sizeof(addr_tmp);

    while(true) {
      // printf("recvfrom\n");
      auto rlen = recvfrom(_socket, _buf, MAX_BUF_LEN, 0,
			   (struct sockaddr *) &addr_tmp, &slen_tmp);

      // printf("rec_len: %ld\n", rlen);

      if(rlen == -1) {
	// perror("some error ? ");
	if(_socket == -1) {
	  puts("Closing UDP socket");
	  return;
	}
	
	if(errno == EAGAIN || errno == EWOULDBLOCK) continue;
	else {
	  perror("Could not receive data in UDP socket");
	  return;
	}
      }
      else if(_callback) _callback->onUdpMessage(_buf, rlen);
    }
  });
}

void UdpSocket::send(const char *buf, size_t len)
{
  constexpr size_t SIZE = 2048;

  if(_socket == -1) return;
  
  while(len > 0) {
    auto buf_len = std::min(SIZE, len);
    // printf("send_len: %ld\n", buf_len);
    
    auto l = sendto(_socket, buf, buf_len, 0, (struct sockaddr *) &_addr, sizeof(_addr));
    
    if(l == -1) {
      perror("Could not send data in UDP socket");
      break;
    }

    len -= buf_len;
  }

  if(!_start) start();
}

void UdpSocket::close()
{
  if(_socket != -1) {
    puts("Try closing UDP socket");
    ::close(_socket);
    _socket = -1;

    if(_recv_thread.joinable()) {
      _recv_thread.join();
      puts("Thread is joined");
    }
    
    _start = false;
  }
}

UdpSocket::~UdpSocket()
{
  close();
}

}
