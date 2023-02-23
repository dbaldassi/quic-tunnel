#include <unistd.h>
#include <sstream>
#include <csignal>
#include <wait.h>
#include <sys/socket.h>
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <thread>
#include <stdlib.h>
#include <string.h>

#include "tcp_client.h"

#include <iostream>
#include <linux/tcp.h>

constexpr int ANNOUNCE_FIELD = 4;

ssize_t TcpClient::get_announce_len(int start, const char* buf, size_t len)
{
  auto size = ANNOUNCE_FIELD - announce_len_ptr;

  if(len - start < ANNOUNCE_FIELD) size -= (len - start);
  
  for(int i = start; i < start + size; ++i) {   
    announce_len[announce_len_ptr] = buf[i];
    ++announce_len_ptr;
  }

  if(announce_len_ptr == ANNOUNCE_FIELD) {
    announce_len_ptr = 0;
    return atoll(announce_len);
  }
  else return -1;
}

void TcpClient::receive_loop()
{
  constexpr size_t MAX = 2048;
  char buf[MAX], pending_buf[MAX];
  ssize_t pending_len = -1, pending_buf_size;

  static int i = 0;
  
  for(;;) {
    int data_start = 0;
      
    auto res = read(_socket, buf, MAX);

    if(res < 0) {
      perror("Error reading tcp socket");
      break;
    }

    while(res > 0) {
      if(pending_len == -1) {
	pending_len = pending_buf_size = get_announce_len(data_start, buf, res);
	data_start += ANNOUNCE_FIELD;
	res -= ANNOUNCE_FIELD;
      }

      std::cout << i << " " << "log :"
		<< res << " "
		<< pending_len << " "
		<< pending_buf_size << " "
		<< data_start << " "
		<< "\n";

      if(res <= 0) continue;

      int copy_len = std::min(pending_len, res);
      memcpy(pending_buf, buf + data_start, copy_len);

      pending_len -= copy_len;
      res -= copy_len;
      data_start += copy_len;

      if(pending_len == 0) {
	std::cout << i++ << " " << "Transmitting: " <<  pending_buf_size << "\n";
	if(_on_received_callback) _on_received_callback(pending_buf, pending_buf_size);
	pending_len = -1;
      }
    }

    ++i;
  }
}

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

  std::thread([this](){ receive_loop(); }).detach();
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
  write(_socket, std::to_string(len).c_str(), 4);
  
  if(write(_socket, buffer, len) < 0) {
    perror("tcp socket write error");
  }
}
