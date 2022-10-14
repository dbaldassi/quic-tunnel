#ifndef QUICGO_SERVER_H
#define QUICGO_SERVER_H

#ifdef GO_SERVER_CALLBACK
#include "../../quic_server.h"
#include "../../udp_socket.h"
#else
#include "quic_server.h"
#include "udp_socket.h"
#endif

class QuicGoServer final : public QuicServer, public out::UdpSocketCallback
{
  bool             _datagrams;
  std::string      _host;
  uint16_t         _port;
  out::UdpSocket * _udp_socket;
  std::string      _qlog_dir;
  std::string      _qlog_filename;
  
public:
  static constexpr const char * IMPL_NAME = "quicgo";
  static Capabilities get_capabilities();
  
  QuicGoServer(std::string host, uint16_t port, out::UdpSocket * udp_socket) noexcept;
  ~QuicGoServer() override = default;

  /* Quic server interface */
  void start() override;
  void stop() override;
  void set_qlog_filename(std::string file_name) override;
  std::string_view get_qlog_path() const noexcept override;
  std::string_view get_qlog_filename() const noexcept override;
  bool set_datagrams(bool enable) override;
  // only newreno for quic go
  bool set_cc(std::string_view cc) noexcept override;

  /*  Udp socket callback */
  void onUdpMessage(const char * buffer, size_t len) noexcept override;

  /* Message callback from go */
  void on_received(const char* buf, uint64_t len);
};

#endif /* QUICGO_SERVER_H */
