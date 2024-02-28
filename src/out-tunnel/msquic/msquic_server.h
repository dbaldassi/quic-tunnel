#ifndef MSQUIC_SERVER_H
#define MSQUIC_SERVER_H

#include <condition_variable>
#include <mutex>
#include <fstream>
#include <chrono>

#include "quic_server.h"

struct QUIC_API_TABLE;
struct QUIC_HANDLE;
struct QUIC_LISTENER_EVENT;
struct QUIC_CONNECTION_EVENT;
struct QUIC_STREAM_EVENT;
struct QUIC_BUFFER;

class MsquicServer : public QuicServer, public out::UdpSocketCallback
{
  std::string      _host;
  uint16_t         _port;
  out::UdpSocket * _udp_socket;
  uint16_t         _cc;

  std::string   _qlog_file;
  std::ofstream _qlog_ofs;
  bool          _datagrams;

  const QUIC_API_TABLE * _msquic = nullptr;
  QUIC_HANDLE * _listener = nullptr;
  QUIC_HANDLE * _configuration = nullptr;
  QUIC_HANDLE * _registration = nullptr;
  QUIC_HANDLE * _connection = nullptr;

  std::mutex              _mutex;
  std::condition_variable _cv;

  std::chrono::time_point<std::chrono::system_clock> _time_0;

  void server_send_stream(QUIC_BUFFER* buffer);
  void server_send_datagram(QUIC_BUFFER* buffer);
  void write_stats(const QUIC_CONNECTION_EVENT* event);
  
public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-out-logs";
  static constexpr const char * IMPL_NAME = "msquic";
  
  MsquicServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket);
  ~MsquicServer() override;

  static Capabilities get_capabilities();
  
  void set_qlog_filename(std::string file_name) override;
  
  bool start() override;
  void loop() override;

  std::string_view get_qlog_path() const noexcept override;
  std::string_view get_qlog_filename() const noexcept override;
  bool set_datagrams(bool enable) override;
  
  bool set_cc(std::string_view cc) noexcept override;
  void stop() override;

  unsigned int server_listener_callback(QUIC_HANDLE* listener, QUIC_LISTENER_EVENT* event);
  unsigned int server_connection_callback(QUIC_HANDLE* connection, QUIC_CONNECTION_EVENT* event);
  unsigned int server_stream_callback(QUIC_HANDLE* stream, QUIC_STREAM_EVENT* event);
  void server_on_datagram_send_state_changed(unsigned int state, void* ctx);
  
  void onUdpMessage(const char* buffer, size_t len) noexcept override;
};

#endif /* MSQUIC_SERVER_H */
