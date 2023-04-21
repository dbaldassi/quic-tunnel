#include "udp_server.h"

#include <string.h>
#include <sys/time.h>

void UdpServer::start()
{
  _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if(_socket < 0) {
    perror("Could not create socket for udp server");
    std::exit(EXIT_FAILURE);
  }

  struct timeval optval;
  optval.tv_usec = 0;
  optval.tv_sec = 5; // 5 seconds timeout
  
  setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &optval, sizeof(optval));

  struct sockaddr_in addr;

  memset((char *)&addr, 0, sizeof(addr));
  memset((char *)&_addr_other, 0, sizeof(struct sockaddr_in));
  
  addr.sin_family = AF_INET;
  addr.sin_port = htons(_port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if(bind(_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("Could not bind UDP socket");
    std::exit(EXIT_FAILURE);
  }

  _out_socket->set_callback(this);
  _out_socket->start();
  
  recv();
}

void UdpServer::stop()
{
  if(_socket < 0) return;

  close(_socket);
  _socket = -1;
  
  std::unique_lock<std::mutex> lock(_cv_mutex);
  _cv.wait(lock);
  
  _out_socket->close();
}

void UdpServer::recv()
{
  char buffer[BUF_SIZE];

  struct sockaddr_in addr;
  socklen_t          slen = sizeof(addr);
  ssize_t            rec_len;

  while(_socket >= 0) {
    rec_len = recvfrom(_socket, buffer, BUF_SIZE, 0, (struct sockaddr*)&addr, &slen);
    
    if(rec_len < 0) {
      if(_socket < 0) puts("Closing UDP socket");
      else if(errno == EAGAIN || errno == EWOULDBLOCK) puts("Refreshing UDP socket");      
      else perror("Could not receive data in UDP socket");
    }
    else {
      _addr_other.sin_family = AF_INET;
      _addr_other.sin_port = addr.sin_port;
      _addr_other.sin_addr.s_addr = addr.sin_addr.s_addr;
      _len_addr_other = slen;

      _out_socket->send(buffer, rec_len);
    }
  }

  _cv.notify_all();
}

void UdpServer::onUdpMessage(const char * buffer, size_t len) noexcept
{
  if(_socket == -1) return;

  auto send_len = sendto(_socket, buffer, len, 0, (struct sockaddr *)&_addr_other, _len_addr_other);
  if(send_len == -1) {
    perror("Could not send back");
  }
}

Capabilities UdpServer::get_capabilities()
{
  Capabilities cap;

  cap.datagrams = true;
  cap.streams = false;
  cap.impl = "udp";

  return cap;
}
