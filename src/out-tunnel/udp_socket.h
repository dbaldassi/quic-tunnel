#ifndef UDP_OUT_SOCKET_H
#define UDP_OUT_SOCKET_H

#include <thread>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

namespace out
{

class UdpSocketCallback
{
public:
  UdpSocketCallback() = default;
  virtual ~UdpSocketCallback() = default;
  
  virtual void onUdpMessage(const char * buffer, size_t len) noexcept = 0;
};

class UdpSocket
{
  static constexpr auto MAX_BUF_LEN = 2048u;
  
  int _port;
  int _socket;

  char _buf[MAX_BUF_LEN];

  UdpSocketCallback * _callback;

  struct hostent *   _host;
  struct sockaddr_in _addr;
  socklen_t          _addr_len;

  std::thread        _recv_thread;
  
public:
  explicit UdpSocket(const char* hostname, int port);
  
  void set_callback(UdpSocketCallback * callback) { _callback = callback; }

  void start();
  
  void send(const char * buf, size_t len);
  
  ~UdpSocket() noexcept;
};

}

#endif /* UDP_SOCKET_H */
