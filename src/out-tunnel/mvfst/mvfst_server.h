#ifndef MVFST_SERVER_H
#define MVFST_SERVER_H

#include "quic_server.h"

class MvfstServer;
class CallbackHandler;

namespace quic
{
class QuicServer;
}

namespace folly
{
class EventBase;
}

class MvfstServer : public QuicServer
{
  std::string      _host;
  uint16_t         _port;
  
  std::shared_ptr<folly::EventBase> _evb;
  std::shared_ptr<quic::QuicServer> _server;
  std::shared_ptr<CallbackHandler>  _handler;
  
  out::UdpSocket * _udp_socket;

  uint8_t _cc;

  std::string _qlog_file;
  bool        _datagrams;
  
public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-out-logs";
  static constexpr const char * IMPL_NAME = "mvfst";
  
  explicit MvfstServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket);

  static Capabilities get_capabilities();
  
  void set_qlog_filename(std::string file_name) override;
  
  void start() override;

  std::string_view get_qlog_path() const noexcept override;
  std::string_view get_qlog_filename() const noexcept override;
  bool set_datagrams(bool enable);
  
  bool set_cc(std::string_view cc) noexcept override;
  void stop() override;
};

#endif /* MVFST_SERVER_H */
