#include "udp_client.h"

#include <stdio.h>
#include <cstdlib>
#include <string.h>
#include <sys/time.h>
#include <thread>

UdpClient::UdpClient(std::string dst_host, int dst_port, int src_port)
  : _dst_host(std::move(dst_host)), _dst_port(dst_port), _src_port(src_port), _socket(-1)
{
  memset((char *)&_addr, 0, sizeof(_addr));
}

UdpClient::~UdpClient()
{
}

void UdpClient::recv()
{
  char buffer[MAX_BUF_LEN];

  struct sockaddr_in addr_tmp;
  socklen_t slen_tmp = sizeof(addr_tmp);

  while(_socket >= 0) {
    auto rlen = recvfrom(_socket, buffer, MAX_BUF_LEN, 0, (struct sockaddr *)&addr_tmp, &slen_tmp);
    
    if(rlen <= 0) {
      if(_socket < 0) puts("closing udp socket");
      else if(errno == EAGAIN || errno == EWOULDBLOCK) continue;
      else perror("Could not receive data in udp socket");
    }
    else if(_on_received_callback) _on_received_callback(buffer, rlen);
  }
}

void UdpClient::start()
{
  _addr.sin_family = AF_INET;
  _addr.sin_port = htons(_dst_port);
  _addr.sin_addr.s_addr = inet_addr(_dst_host.c_str());

  _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if(_socket < 0) {
    perror("Could not create UDP client socket");
    std::exit(EXIT_FAILURE);
  }

  struct timeval optval;
  optval.tv_usec = 0;
  optval.tv_sec = 5; // 5 seconds timeout

  if(setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &optval, sizeof(optval)) < 0)
    perror("Could not set timeout");

  _recv_th = std::thread([this](){ recv(); });
}

void UdpClient::stop()
{
  if(_socket < 0) return;

  puts("Try closing udp socket");

  _socket = -1;

  if(_recv_th.joinable()) _recv_th.join();
}

void UdpClient::send_message_datagram(const char * buffer, size_t len)
{
  if(_socket < 0) return;

  while(len > 0) {
    auto buf_len = std::min((int)MAX_BUF_LEN, (int)len);
    
    auto l = sendto(_socket, buffer, buf_len, 0, (struct sockaddr*) &_addr, sizeof(_addr));
    
    if(l < 0) {
      perror("Could not send data in UDP socket");
      break;
    }

    len -= buf_len;
  }
}

Capabilities UdpClient::get_capabilities()
{
  Capabilities cap;

  cap.datagrams = true;
  cap.impl = "udp";
  cap.streams = false;

  return cap;
}
