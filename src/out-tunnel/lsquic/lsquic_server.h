#ifndef LSQUIC_SERVER_H
#define LSQUIC_SERVER_H

#include <lsquic.h>
#include <atomic>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "quic_server.h"

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

  lsquic_engine_t   *    _engine;
  lsquic_engine_api      _engine_api;
  lsquic_stream_if       _stream_if;
  lsquic_engine_settings _engine_settings;

  uint8_t _cc;
  
  int _socket;
  unsigned char _buf[MAX_BUF_LEN];
  
  struct sockaddr_in _addr_peer;
  struct sockaddr_in _addr_local;
  socklen_t          _addr_local_len;
  socklen_t          _addr_peer_len;

  std::thread _recv_thread;
  
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
  void start() override;

  std::string_view get_qlog_path() const noexcept override { return DEFAULT_QLOG_PATH; }
  std::string_view get_qlog_filename() const noexcept override { return DEFAULT_QLOG_PATH; }
  bool set_datagrams(bool enable) override;
  
  bool set_cc(std::string_view cc) noexcept override;
  void stop() override;

  // lsquic callback
  lsquic_conn_ctx_t   * on_new_conn(lsquic_conn_t *conn);
  lsquic_stream_ctx_t * on_new_stream(lsquic_stream_t * stream);
  void on_conn_closed (lsquic_conn_t * conn);
  void on_read(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h);
  void on_write(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h);
  void on_stream_close(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h);
  int send_packets_out(const struct lsquic_out_spec *specs, unsigned n_specs);

};

#endif /* LSQUIC_SERVER_H */
