#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "quic_server.h"

class TcpServer final : public QuicServer, public out::UdpSocketCallback
{
  std::string dummy_file_name;

  std::string _dst_host;
  uint16_t _dst_port;
  
  std::string _host;
  uint16_t    _port;

  int _pid_socat;
  int _pid_ss;

  int _tcp_socket;
  int _connfd;
  out::UdpSocket * _udp_socket;
  
  void setup_socat();
  void setup_ss();
  
public:
  TcpServer(const std::string& dst_host, uint16_t dst_port, uint16_t src_port, out::UdpSocket * sock);
  ~TcpServer() = default;
  
  void start() override;
  void stop() override;
  void set_qlog_filename(std::string) override {}
  std::string_view get_qlog_path() const noexcept override { return dummy_file_name; }
  std::string_view get_qlog_filename() const noexcept override { return dummy_file_name; }
  bool set_datagrams(bool) override { return false; }
  bool set_cc(std::string_view) noexcept override { return false; }
  static Capabilities get_capabilities();

  void onUdpMessage(const char * buffer, size_t len) noexcept override;
};

#endif /* TCP_SERVER_H */
