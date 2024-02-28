#ifndef LSQUIC_SERVER_H
#define LSQUIC_SERVER_H

#include <atomic>
#include <condition_variable>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "quic_server.h"

struct lsquic_engine;
typedef struct lsquic_engine lsquic_engine_t;

struct lsquic_engine_api;
struct lsquic_stream_if;
struct lsquic_logger_if;
struct lsquic_engine_settings;

struct lsquic_conn_ctx;
typedef struct lsquic_conn_ctx lsquic_conn_ctx_t;

struct lsquic_conn;
typedef struct lsquic_conn lsquic_conn_t;

struct lsquic_stream;
typedef struct lsquic_stream lsquic_stream_t;

struct lsquic_stream_ctx;
typedef struct lsquic_stream_ctx lsquic_stream_ctx_t;

struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;

class LsquicServer : public QuicServer
{
  enum Congestion : uint8_t {
    DEFAULT_CC = 0,
    CUBIC = 1,
    BBRV1 = 2,
    ADAPTIVE = 3
  };
  
  static constexpr auto MAX_BUF_LEN = 2048u;
  
  std::string      _host;
  uint16_t         _port;
  
  out::UdpSocket * _udp_socket;

  std::string _qlog_file;
  bool        _datagrams;
  bool        _start;

  lsquic_engine_t    *     _engine;
  lsquic_engine_api  *     _engine_api;
  lsquic_stream_if   *     _stream_if;
  lsquic_logger_if   *     _logger_if;
  lsquic_engine_settings * _engine_settings;

  SSL_CTX    * _ssl_ctx;
  ssl_ctx_st * _cert_ctx;
  
  uint8_t _cc;
  
  int _socket;
  unsigned char _buf[MAX_BUF_LEN];
  
  struct sockaddr_in _addr_peer;
  struct sockaddr_in _addr_local;
  socklen_t          _addr_local_len;
  socklen_t          _addr_peer_len;

  std::condition_variable _cv;
  std::mutex _cv_mutex;
  
  static std::atomic<int> _ref_count;

  static void init_lsquic();
  static void exit_lsquic();

  void init_socket();
  void close_socket();
  
public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-out-logs";
  static constexpr const char * IMPL_NAME = "lsquic";
  
  explicit LsquicServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket);
  ~LsquicServer() override;

  static Capabilities get_capabilities();
  
  void set_qlog_filename(std::string file_name) override;

  ssize_t recv();
  bool start() override;
  void loop() override;

  std::string_view get_qlog_path() const noexcept override { return DEFAULT_QLOG_PATH; }
  std::string_view get_qlog_filename() const noexcept override { return DEFAULT_QLOG_PATH; }
  bool set_datagrams(bool enable) override;
  
  bool set_cc(std::string_view cc) noexcept override;
  void stop() override;

  SSL_CTX* get_ssl_ctx() { return _ssl_ctx; }
  ssl_ctx_st* get_cert_ctx() { return _cert_ctx; }
  
  // lsquic callback
  lsquic_conn_ctx_t   * on_new_conn(lsquic_conn_t *conn);
  lsquic_stream_ctx_t * on_new_stream(lsquic_stream_t * stream);
  void on_conn_closed (lsquic_conn_t * conn);
  void on_read(lsquic_stream_t * stream);
  void on_write(lsquic_stream_t * stream);
  void on_stream_close(lsquic_stream_t * stream);
  int send_packets_out(const struct lsquic_out_spec *specs, unsigned n_specs);

};

#endif /* LSQUIC_SERVER_H */
