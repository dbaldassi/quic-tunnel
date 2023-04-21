#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <mutex>
#include <condition_variable>

#include "quic_server.h"

class UdpServer final : public QuicServer, public out::UdpSocketCallback
{
  std::string _dummy_qlog;
  int _socket;

  out::UdpSocket * _out_socket;
  int _port;

  static constexpr auto BUF_SIZE = 2048;

  struct sockaddr_in _addr_other;
  socklen_t _len_addr_other;

  std::mutex              _cv_mutex;
  std::condition_variable _cv;
  
public:
  
  UdpServer(int port, out::UdpSocket * udp_socket) : _socket(-1), _port(port), _out_socket(udp_socket) {}
  ~UdpServer() = default;

  void start() override;
  void stop() override;
  void recv();

  /* no qlog file for udp */
  void set_qlog_filename(std::string) override {}
  std::string_view get_qlog_path() const noexcept override { return _dummy_qlog; }
  std::string_view get_qlog_filename() const noexcept override { return _dummy_qlog; }

  /* only datagram for udp */
  bool set_datagrams(bool enable) override { return true; }

  /* no congestion control for udp */
  bool set_cc(std::string_view cc) noexcept override { return false; }

  void onUdpMessage(const char * buffer, size_t len) noexcept override;
  
  static Capabilities get_capabilities();
};

#endif /* UDP_SERVER_H */
