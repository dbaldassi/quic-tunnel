#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

class UdpSocket
{
  static constexpr auto MAX_BUF_LEN = 2048u;
  
  int  _port;
  int  _socket;

  char _buf[MAX_BUF_LEN];

  struct sockaddr_in _addr_other;
  socklen_t          _addr_other_len;
  
public:  
  explicit UdpSocket(int port) noexcept;
  ~UdpSocket() noexcept;
  
  ssize_t recv() noexcept;
  const char* get_buffer() const noexcept { return _buf; }

  void close();
  bool send_back(const char * buf, size_t len);
};

#endif /* UDP_SOCKET_H */
