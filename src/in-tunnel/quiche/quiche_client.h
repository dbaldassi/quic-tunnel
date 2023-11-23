#ifndef QUICHE_CLIENT_H
#define QUICHE_CLIENT_H


#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

#include <quic_client.h>

class quiche_config;
class quiche_conn;
struct ev_loop;
struct ev_timer;
struct ev_io;
struct ev_async;
struct conn_io;

class QuicheClient final : public QuicClient
{
  static constexpr auto MAX_BUF_LEN = 2048u;
 
  std::string _host;
  std::string _qlog_dir;
  std::string _qlog_filename;
  int         _port;
  bool        _datagrams;
  uint8_t     _cc;

  int _socket;
  
  struct sockaddr_in _peer;
  struct addrinfo *  _peer_info;
  struct sockaddr_in _local;
  socklen_t          _local_len;
  socklen_t          _peer_len;

  std::condition_variable _cv;
  std::mutex _cv_mutex;
  
  std::atomic<bool>       _start;

  std::unique_ptr<ev_timer> _timer;
  std::unique_ptr<ev_async> _async_watcher;
  std::unique_ptr<ev_io>    _watcher;
  struct ev_loop  * _loop;

  quiche_config * _config;
  quiche_conn   * _conn;

  int _stream_id = 1;
  
  bool init_socket();
  void close_socket();

  std::thread _th;
  
public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-in-logs";
  static constexpr const char * IMPL_NAME = "quiche";

  struct conn_io * conn_io;
  
  QuicheClient(std::string host, int port) noexcept;
  ~QuicheClient() override;

  quiche_conn * get_conn() { return _conn; }
  quiche_config * get_config() { return _config; }
  int get_sock() const { return _socket; }
  const struct sockaddr * get_local_addr() const { return (sockaddr *)&_local; }
  struct sockaddr * get_local_addr() { return (sockaddr *)&_local; }

  // -- quic client interface
  void set_qlog_filename(std::string filename) noexcept override;
  std::string_view get_qlog_path()   const noexcept override;
  std::string_view get_qlog_filename() const noexcept override;

  bool set_cc(std::string_view cc) noexcept override;
  void start() override;
  void stop() override;
  void send_message_stream(const char * buffer, size_t len) override;
  void send_message_datagram(const char * buffer, size_t len) override;

  void recv_cb(const uint8_t* buf, size_t len);
  
  static Capabilities get_capabilities();
};

#endif /* QUICHE_CLIENT_H */
