#ifndef MVFST_CLIENT_H
#define MVFST_CLIENT_H

#include "quic_client.h"

class MvfstClientImpl;

class MvfstClient : public QuicClient
{
  std::shared_ptr<MvfstClientImpl> _impl;

public:
  MvfstClient(std::string_view host,
	     uint16_t port,
	     std::string qlog_path=DEFAULT_QLOG_PATH) noexcept;

  ~MvfstClient() = default;

  static constexpr const char * IMPL_NAME = "mvfst";
  static Capabilities get_capabilities();
  
  void set_qlog_filename(std::string name) noexcept override;
  std::string_view get_qlog_path()     const noexcept override;
  std::string_view get_qlog_filename() const noexcept override;

  /* QuicClient overrides */
  bool set_cc(std::string_view cc) noexcept override;
  void start() override;
  void stop() override;
  void send_message_stream(const char * buffer, size_t len) override;
  void send_message_datagram(const char * buffer, size_t len) override;
};

#endif /* MVFST_CLIENT_H */
