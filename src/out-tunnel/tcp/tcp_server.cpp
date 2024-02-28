#include "tcp_server.h"

#include <csignal>
#include <sstream>
#include <wait.h>
#include <cstring>
#include <stdlib.h>

#include <iostream>
#include <linux/tcp.h>

TcpServer::TcpServer(const std::string& dst_host, uint16_t dst_port, uint16_t src_port,
		     out::UdpSocket * sock)
  : _dst_host(dst_host),
    _dst_port(dst_port),
    _port(src_port),
    _pid_socat(-1),
    _pid_ss(-1),
    _tcp_socket(-1),
    _udp_socket(sock)
{
  _udp_socket->set_callback(this);
}

bool TcpServer::start()
{
  _udp_socket->start();
  
  _tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  int optval = 1;
  int optlen = sizeof(optval);
  
  setsockopt(_tcp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
  setsockopt(_tcp_socket, IPPROTO_TCP, TCP_NODELAY, &optval, optlen);
  
  struct sockaddr_in addr;
  memset((char *)&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(_port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  std::cout << "bind" << "\n";
  if(bind(_tcp_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("Could not bind");
    return false;
  }
  
  std::cout << "listen" << "\n";
  if(listen(_tcp_socket, 10) < 0) {
    perror("Could not listen");
    return false;
  }
  
  return true;
}

void TcpServer::loop()
{
  struct sockaddr_in addr_client;
  socklen_t len = sizeof(addr_client);

  std::cout << "accept" << "\n";
  _connfd = accept(_tcp_socket, (struct sockaddr*)&addr_client, &len);

  _dst_port = ntohs(addr_client.sin_port);
  _dst_host = std::string(inet_ntoa(addr_client.sin_addr));

  if(_pid_ss == -1) setup_ss();

  std::cout << "accepted tcp client " << _dst_host << ":" << _dst_port << "\n";
  receive_loop();
}

constexpr int ANNOUNCE_FIELD = 4;

ssize_t TcpServer::get_announce_len(int start, const char* buf, size_t len)
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

void TcpServer::receive_loop()
{
  constexpr size_t MAX = 2048;
  char buf[MAX], pending_buf[MAX];
  ssize_t pending_len = -1, pending_buf_size;
  
  for(;;) {
    int data_start = 0;
    bzero(buf, MAX);

    auto res = read(_connfd, buf, MAX);
    // std::cout << "received: " << res << "\n";

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

      if(res <= 0) continue;

      int copy_len = std::min(pending_len, res);
      memcpy(pending_buf, buf + data_start, copy_len);

      pending_len -= copy_len;
      res -= copy_len;
      data_start += copy_len;

      if(pending_len == 0) {
	static int i = 0;
	std::cout << i++ << " " << "Transmitting: " <<  pending_buf_size << "\n";
	_udp_socket->send(pending_buf, pending_buf_size);
	pending_len = -1;
      }
    }
  }
}

void TcpServer::stop()
{
  if(_pid_ss > 0) {
    kill(_pid_ss, SIGTERM);
    _pid_ss = -1;
  }

  auto pid = fork();

  if(pid == 0) {
    execl("/usr/local/bin/show_csv_ss.py", "/usr/local/bin/show_csv_ss.py", "save", NULL);
  }
  else waitpid(pid, NULL, 0);

  _udp_socket->close();
}

void TcpServer::setup_socat()
{
  std::ostringstream tcp, udp;

  tcp << "tcp-l:" << _port << ",fork,reuseaddr";
  udp << "udp:" << _dst_host << ":" << _dst_port;
  
  execl("/usr/bin/socat", "/usr/bin/socat", tcp.str().c_str(), udp.str().c_str(), NULL);
}

void TcpServer::setup_ss()
{
  _pid_ss = fork();

  if(_pid_ss == 0) {
  
    std::ostringstream param;
  
    param << _dst_host << ":" << _dst_port;
    
    execl("/usr/local/bin/ss-output.sh", "/usr/local/bin/ss-output.sh", param.str().c_str(), NULL);
  }
}

Capabilities TcpServer::get_capabilities()
{
  Capabilities cap;
  cap.impl = "tcp";
  
  cap.cc.emplace_back("newreno");

  cap.datagrams = false;

  return cap;
}

#include <iostream>
void TcpServer::onUdpMessage(const char *buffer, size_t len) noexcept
{
  static size_t i = 0;
  std::cout << i++ << " sent: " << len << std::endl;

  [[maybe_unused]] auto _ = write(_connfd, std::to_string(len).c_str(), 4);
  
  if(write(_connfd, buffer, len) < 0) {
    perror("Error writing tcp");
  }
}
