#ifndef QUICHE_SERVER_H
#define QUICHE_SERVER_H

#include <atomic>
#include <queue>
#include <condition_variable>

// #include <openssl/ssl.h>
#include <sys/socket.h>

#include "quic_server.h"

class quiche_config;
class quiche_conn;
struct ev_loop;
struct ev_timer;
struct ev_async;

class QuicheServer : public QuicServer, public out::UdpSocketCallback
{
  std::string      _host;
  uint16_t         _port;
  
  out::UdpSocket * _udp_socket = nullptr;

  std::string _qlog_file;
  bool        _datagrams;
  bool        _start;

  uint8_t _cc;

  int _socket = -1;
  
  std::condition_variable _cv;
  std::mutex _cv_mutex;

  quiche_config * _config = nullptr;
  quiche_conn   * _conn = nullptr;

  int _stream_id;
  
  struct sockaddr_in _local;
  struct sockaddr_in _peer;
  socklen_t _local_len;
  socklen_t _peer_len;

  std::unique_ptr<struct ev_timer> _timer;
  std::unique_ptr<struct ev_async> _async_watcher;
  struct ev_loop * _loop;

  std::queue<std::vector<char>> _queue;
  
  bool init_socket();
  
public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-out-logs";
  static constexpr const char * IMPL_NAME = "quiche";

  struct conn_io * conn_io = nullptr;
  
  explicit QuicheServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket);
  ~QuicheServer() override;

  static Capabilities get_capabilities();
  
  void set_qlog_filename(std::string file_name) override;

  bool start() override;
  void loop() override;

  quiche_conn * get_conn() { return _conn; }
  void set_conn(quiche_conn * conn) { _conn = conn; }
  quiche_config * get_config() { return _config; }
  int get_sock() const { return _socket; }

  const struct sockaddr * get_local_addr() const { return (sockaddr *)&_local; }
  struct sockaddr * get_local_addr() { return (sockaddr *)&_local; }
  
  socklen_t get_local_addr_len() const { return _local_len; }
  
  std::string_view get_qlog_path() const noexcept override { return DEFAULT_QLOG_PATH; }
  std::string_view get_qlog_filename() const noexcept override { return _qlog_file; }
  bool set_datagrams(bool enable) override;
  
  bool set_cc(std::string_view cc) noexcept override;
  void stop() override;

  void on_recv(const uint8_t * buf, size_t len);
  void onUdpMessage(const char* buffer, size_t len) noexcept override;
};

#endif /* QUICHE_SERVER_H */
