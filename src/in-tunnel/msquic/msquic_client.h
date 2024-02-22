#ifndef MSQUIC_CLIENT_H
#define MSQUIC_CLIENT_H

#include "quic_client.h"

struct QUIC_API_TABLE;
struct QUIC_HANDLE;
struct QUIC_LISTENER_EVENT;
struct QUIC_CONNECTION_EVENT;
struct QUIC_STREAM_EVENT;
struct QUIC_BUFFER;

class MsquicClient final : public QuicClient
{ 
  std::string _host;
  std::string _qlog_dir;
  std::string _qlog_filename;
  int         _port;
  uint16_t    _cc;
  
  const QUIC_API_TABLE * _msquic = nullptr;
  QUIC_HANDLE * _listener = nullptr;
  QUIC_HANDLE * _configuration = nullptr;
  QUIC_HANDLE * _registration = nullptr;
  QUIC_HANDLE * _connection = nullptr;
    
public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-in-logs";
  static constexpr const char * IMPL_NAME = "msquic";

  struct conn_io * conn_io;
  
  MsquicClient(std::string host, int port) noexcept;
  ~MsquicClient() override;

  // -- quic client interface
  void set_qlog_filename(std::string filename) noexcept override;
  std::string_view get_qlog_path()   const noexcept override;
  std::string_view get_qlog_filename() const noexcept override;

  bool set_cc(std::string_view cc) noexcept override;
  void start() override;
  void stop() override;
  void send_message_stream(const char * buffer, size_t len) override;
  void send_message_datagram(const char * buffer, size_t len) override;

  unsigned int connection_callback(QUIC_HANDLE* connection, QUIC_CONNECTION_EVENT* event);
  unsigned int stream_callback(QUIC_HANDLE* stream, QUIC_STREAM_EVENT* event);
  void on_datagram_send_state_changed(unsigned int state, void *ctx);
  
  static Capabilities get_capabilities();
};

#endif /* MSQUIC_CLIENT_H */
