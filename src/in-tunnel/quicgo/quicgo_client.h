#ifndef QUICGO_CLIENT_H
#define QUICGO_CLIENT_H

#ifdef GO_CLIENT_CALLBACK
#include "../../quic_client.h"
#else
#include "quic_client.h"
#endif

class QuicGoClient final : public QuicClient
{
  std::string _host;
  int         _port;
public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-in-logs";
  
  QuicGoClient(std::string host, int port) noexcept;
  ~QuicGoClient() = default;

  // -- quic client interface
  std::string_view get_qlog_path()   const noexcept override;
  std::string_view get_qlog_filename() const noexcept override;

  bool set_cc(std::string_view cc) noexcept override;
  void start() override;
  void stop() override;
  void send_message_stream(const char * buffer, size_t len) override;
  void send_message_datagram(const char * buffer, size_t len) override;

  void on_received(const char* buf, uint64_t len);
};

#endif /* QUICGO_CLIENT_H */
