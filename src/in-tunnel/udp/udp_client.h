#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include "quic_client.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <mutex>
#include <condition_variable>

class UdpClient : public QuicClient
{
  std::string dummy_qlog_path;

  std::string _dst_host;
  uint16_t    _dst_port;  
  uint16_t    _src_port;  

  int _socket;

  struct sockaddr_in _addr;
  socklen_t          _len_addr;
  
  static constexpr auto MAX_BUF_LEN = 2048u;

  std::mutex              _stop_mutex;
  std::condition_variable _stop_cv;
  
public:
  UdpClient(std::string dst_host, int dst_port, int src_port);
  ~UdpClient();
  
  /* No qlog file for udp */
  void set_qlog_filename(std::string) noexcept override {}
  std::string_view get_qlog_path()   const noexcept override { return dummy_qlog_path; }
  std::string_view get_qlog_filename() const noexcept override { return dummy_qlog_path; }

  /* No congestion control for UDP */
  bool set_cc(std::string_view cc) noexcept override { return false; };

  void start() override;
  void stop() override;
  void recv();

  /* only datagram available for udp */
  void send_message_stream(const char * buffer, size_t len) override {}
  void send_message_datagram(const char * buffer, size_t len) override;

  static Capabilities get_capabilities();
};

#endif /* UDP_CLIENT_H */
