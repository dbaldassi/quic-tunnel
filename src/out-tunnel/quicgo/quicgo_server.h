#ifndef QUICGO_SERVER_H
#define QUICGO_SERVER_H

#include "quic_server.h"

class QuicGoServer final : public QuicServer
{

public:

  QuicGoServer() = default;
  ~QuicGoServer() override = default;
  
  void start() override;
  void stop() override;
  void set_qlog_filename(std::string file_name) override;
  std::string_view get_qlog_path() const noexcept override;
  std::string_view get_qlog_filename() const noexcept override;
  bool set_datagrams(bool enable) override;
  bool set_cc(std::string_view cc) noexcept override;
};

#endif /* QUICGO_SERVER_H */
