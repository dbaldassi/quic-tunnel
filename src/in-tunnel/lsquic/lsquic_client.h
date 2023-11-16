#ifndef LSQUIC_CLIENT_H
#define LSQUIC_CLIENT_H

#include "quic_client.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <lsquic.h>
#include <openssl/ssl.h>

class LsquicClient final : public QuicClient
{
  static constexpr auto MAX_BUF_LEN = 2048u;

  enum Congestion : uint8_t {
    DEFAULT_CC = 0,
    CUBIC = 1,
    BBRV1 = 2,
    ADAPTIVE = 3
  };
  
  std::string _host;
  std::string _qlog_dir;
  std::string _qlog_filename;
  int         _port;
  bool        _datagrams;
  uint8_t     _cc;

  SSL_CTX    * _ssl_ctx;

  lsquic_engine_t   *    _engine;
  lsquic_engine_api      _engine_api;
  lsquic_stream_if       _stream_if;
  lsquic_logger_if       _logger_if;
  lsquic_engine_settings _engine_settings;
  lsquic_conn_t *        _conn;
  
  int _socket;
  unsigned char _buf[MAX_BUF_LEN];
  
  struct sockaddr_in _addr_peer;
  struct sockaddr_in _addr_local;
  socklen_t          _addr_local_len;
  socklen_t          _addr_peer_len;

  std::condition_variable _cv;
  std::mutex _cv_mutex;
  
  static std::atomic<int> _ref_count;
  std::atomic<bool>       _start;
  
  static void init_lsquic();
  static void exit_lsquic();

  void init_socket();
  void close_socket();
  
public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-in-logs";
  static constexpr const char * IMPL_NAME = "lsquic";
  
  LsquicClient(std::string host, int port) noexcept;
  ~LsquicClient() = default;

  // -- quic client interface
  void set_qlog_filename(std::string filename) noexcept override;
  std::string_view get_qlog_path()   const noexcept override;
  std::string_view get_qlog_filename() const noexcept override;

  bool set_cc(std::string_view cc) noexcept override;
  void start() override;
  void stop() override;
  void send_message_stream(const char * buffer, size_t len) override;
  void send_message_datagram(const char * buffer, size_t len) override;

  SSL_CTX* get_ssl_ctx() { return _ssl_ctx; }
  
  // lsquic callback
  lsquic_conn_ctx_t   * on_new_conn(lsquic_conn_t *conn);
  lsquic_stream_ctx_t * on_new_stream(lsquic_stream_t * stream);
  void on_conn_closed (lsquic_conn_t * conn);
  void on_read(lsquic_stream_t * stream);
  void on_write(lsquic_stream_t * stream);
  void on_stream_close(lsquic_stream_t * stream);
  // int send_packets_out(const struct lsquic_out_spec *specs, unsigned n_specs);
  
  static Capabilities get_capabilities();
};


#endif /* LSQUIC_CLIENT_H */
