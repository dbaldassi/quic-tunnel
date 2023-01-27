#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "quic_client.h"

class TcpClient : public QuicClient
{
  std::string _dummy;
  std::string _dst_host;
  uint16_t    _dst_port;  
  uint16_t    _src_port;  

  int _socket;
  
public:

  TcpClient(std::string dst_host, uint16_t dst_port, uint16_t src_port) noexcept
    : _dst_host(std::move(dst_host)), _dst_port(dst_port), _src_port(src_port) {}

  ~TcpClient() override = default;
  
  void set_qlog_filename(std::string filename) noexcept override {}
  std::string_view get_qlog_path()   const noexcept override { return _dummy; }
  std::string_view get_qlog_filename() const noexcept override { return _dummy; }
  bool set_cc(std::string_view cc) noexcept override { return false; }
  void start() override;
  void stop() override;
  void send_message_stream(const char * buffer, size_t len) override;
  void send_message_datagram(const char * buffer, size_t len) override {}

  static Capabilities get_capabilities();
};

#endif /* TCP_CLIENT_H */
