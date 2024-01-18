#include "quiche_client.h"

#include <quiche.h>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ev.h>
#include <cinttypes>
#include <array>

constexpr size_t LOCAL_CONN_ID_LEN = 16;
constexpr size_t MAX_DATAGRAM_SIZE = 2500;

static std::mutex client_flush_mutex;

extern "C"
{

struct conn_io {
  ev_timer timer;
  int      sock;

  struct sockaddr_storage local_addr;
  socklen_t               local_addr_len;
  
  quiche_conn  * conn;
  QuicheClient * client;
};

struct timeout_cb_data
{
  struct conn_io * conn_io;
  QuicheClient   * server;
};
  
static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io)
{
  static uint8_t out[65535];

  quiche_send_info send_info;

  while (1) {
    std::lock_guard<std::mutex> lock(client_flush_mutex);
    ssize_t written = quiche_conn_send(conn_io->conn, out, sizeof(out), &send_info);

    if (written == QUICHE_ERR_DONE) break;
    
    if (written < 0) {
      fprintf(stderr, "failed to create packet: %zd\n", written);
      return;
    }

    ssize_t sent = sendto(conn_io->sock, out, written, 0,
			  (struct sockaddr *) &send_info.to,
			  send_info.to_len);

    if (sent != written) {
      perror("failed to send");
      return;
    }
  }

  auto uni = quiche_conn_peer_streams_left_uni(conn_io->conn);
  auto bidi = quiche_conn_peer_streams_left_bidi(conn_io->conn);

  double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
  conn_io->timer.repeat = t;
  ev_timer_again(loop, &conn_io->timer);
}

static void debug_log(const char *line, void *argp)
{
  std::FILE * logfile = std::fopen("quiche_log.txt", "w");
  fmt::print(logfile, "{}\n", line);
}

static void recv_cb(EV_P_ ev_io *w, int revents)
{
  static uint8_t buf[65535];
  
  struct conn_io *conn_io = (struct conn_io*)w->data;
  
  while (1) {
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    memset(&peer_addr, 0, peer_addr_len);

    ssize_t read = recvfrom(conn_io->sock, buf, sizeof(buf), 0,
			    (struct sockaddr *) &peer_addr,
			    &peer_addr_len);

    if (read < 0) {
      if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) break;

      perror("failed to read");
      return;
    }

    quiche_recv_info recv_info = {
      (struct sockaddr *) &peer_addr,
      peer_addr_len,

      (struct sockaddr *) &conn_io->local_addr,
      conn_io->local_addr_len,
    };

    auto uni = quiche_conn_peer_streams_left_uni(conn_io->conn);
    auto bidi = quiche_conn_peer_streams_left_bidi(conn_io->conn);

    ssize_t done;

    {
      std::lock_guard<std::mutex> lock(client_flush_mutex);
      done = quiche_conn_recv(conn_io->conn, buf, read, &recv_info);
    }

    if (done < 0) {
      fprintf(stderr, "failed to process packet\n");
      continue;
    }
  }

  if (quiche_conn_is_closed(conn_io->conn)) {
    fprintf(stderr, "connection closed\n");
    ev_break(EV_A_ EVBREAK_ONE);
    return;
  }

  if (quiche_conn_is_established(conn_io->conn)) {
    uint64_t s = 0;

    quiche_stream_iter *readable = quiche_conn_readable(conn_io->conn);
    
    while (quiche_stream_iter_next(readable, &s)) {
      std::lock_guard<std::mutex> lock(client_flush_mutex);
      
      bool fin = false;
      ssize_t recv_len = quiche_conn_stream_recv(conn_io->conn, s,
						 buf, sizeof(buf),
						 &fin);
      if (recv_len < 0) break;
            
      conn_io->client->recv_cb(buf, recv_len);
    }

    quiche_stream_iter_free(readable);

    if(auto dgram_len = quiche_conn_dgram_recv(conn_io->conn, buf, sizeof(buf)); dgram_len >= 0) {
      conn_io->client->recv_cb(buf, dgram_len);
    }
  }

  flush_egress(loop, conn_io);
}

static void timeout_cb(EV_P_ ev_timer *w, int revents)
{
  struct conn_io *conn_io = (struct conn_io*)w->data;
  quiche_conn_on_timeout(conn_io->conn);

  flush_egress(loop, conn_io);

  if (quiche_conn_is_closed(conn_io->conn)) {
    quiche_stats stats;
    quiche_path_stats path_stats;

    quiche_conn_stats(conn_io->conn, &stats);
    quiche_conn_path_stats(conn_io->conn, 0, &path_stats);

    fprintf(stderr, "connection closed, recv=%zu sent=%zu lost=%zu rtt=%" PRIu64 "ns\n",
	    stats.recv, stats.sent, stats.lost, path_stats.rtt);

    ev_break(EV_A_ EVBREAK_ONE);
    return;
  }
}

static void async_callback(EV_P_ ev_async*, int)
{
  ev_break(EV_A_ EVBREAK_ONE);
}
  
}

QuicheClient::QuicheClient(std::string host, int port) noexcept
  : _host(std::move(host)), _port(port), _qlog_dir(DEFAULT_QLOG_PATH)
{
  auto ver = quiche_version();
  fmt::print("Quiche version : {}\n", ver);

#ifdef QUICHE_DEBUG
  quiche_enable_debug_logging(debug_log, NULL);
#endif
  
  _config = quiche_config_new(QUICHE_PROTOCOL_VERSION);
  if(!_config) {
    fmt::print("QUICHE error, could not create config\n");
    std::exit(1);
  }

  char s_alpn[0x100] = "0echo\0";
  s_alpn[0] = 4;
  
  int err = quiche_config_set_application_protos(_config, (uint8_t*)s_alpn, strlen(s_alpn));
  if(err < 0) {
    fmt::print("QUICHE error, could not set applications proto {}\n", err);
  }

  quiche_config_enable_dgram(_config, true, 2048, 2048);

  // quiche_config_set_max_idle_timeout(_config, 5000);
  quiche_config_set_max_recv_udp_payload_size(_config, MAX_DATAGRAM_SIZE);
  quiche_config_set_max_send_udp_payload_size(_config, MAX_DATAGRAM_SIZE);
  quiche_config_set_initial_max_data(_config, 10000000);
  quiche_config_set_initial_max_stream_data_bidi_local(_config, 1000000);
  quiche_config_set_initial_max_stream_data_bidi_remote(_config, 1000000);
  quiche_config_set_initial_max_stream_data_uni(_config, 1000000);
  quiche_config_set_initial_max_streams_bidi(_config, 1000000);
  quiche_config_set_initial_max_streams_uni(_config, 1000000);
  quiche_config_set_cc_algorithm(_config, QUICHE_CC_RENO);
  quiche_config_set_disable_active_migration(_config, true);

#ifdef QUICHE_DEBUG
  quiche_config_log_keys(_config);
#endif
}

QuicheClient::~QuicheClient()
{
  quiche_config_free(_config);
}

// -- quic client interface
void QuicheClient::set_qlog_filename(std::string filename) noexcept
{
  _qlog_filename = std::move(filename);
}

std::string_view QuicheClient::get_qlog_path()   const noexcept
{
  return _qlog_dir;
}

std::string_view QuicheClient::get_qlog_filename() const noexcept
{
  return _qlog_filename;
}

bool QuicheClient::set_cc(std::string_view cc) noexcept
{
  if(cc == "newreno") {
    quiche_config_set_cc_algorithm(_config, QUICHE_CC_RENO);
  }
  else if(cc == "cubic") {
    quiche_config_set_cc_algorithm(_config, QUICHE_CC_CUBIC);
  }
  else if(cc == "BBRv1") {
    quiche_config_set_cc_algorithm(_config, QUICHE_CC_BBR);
  }
  else if(cc == "BBRv2") {
    quiche_config_set_cc_algorithm(_config, QUICHE_CC_BBR2);
  }
  else {
    fmt::print("Requested cc {} does not exist\n", cc);
    return false;
  }
  
  return true;
}

bool QuicheClient::init_socket()
{
  _peer_len = sizeof(_peer);
  _local_len = sizeof(_peer);

  memset(&_peer, 0, _peer_len);
  memset(&_local, 0, _local_len);

  const struct addrinfo hints = {
    .ai_family = PF_UNSPEC,
    .ai_socktype = SOCK_DGRAM,
    .ai_protocol = IPPROTO_UDP
  };
  
  if (getaddrinfo(_host.c_str(), std::to_string(_port).c_str(), &hints, &_peer_info) != 0) {
    perror("failed to resolve host");
    return false;
  }

  _socket = socket(_peer_info->ai_family, SOCK_DGRAM, 0);
  if(_socket < 0) {
    perror("failed to create socket");
    return -1;
  }

  if (fcntl(_socket, F_SETFL, O_NONBLOCK) != 0) {
    perror("failed to make socket non-blocking");
    return -1;
  }
  
  return true;
}

void QuicheClient::close_socket()
{
  ::close(_socket);
  _socket = -1;
}

void QuicheClient::start()
{
  std::array<uint8_t, LOCAL_CONN_ID_LEN> scid;
  int rng = open("/dev/urandom", O_RDONLY);
  if (rng < 0) {
    perror("failed to open /dev/urandom");
    std::exit(-1);
  }

  ssize_t rand_len = read(rng, scid.data(), scid.size());
  if (rand_len < 0) {
    perror("failed to create connection ID");
    std::exit(-1);
  }

  init_socket();
  
  conn_io = (struct conn_io*)malloc(sizeof(struct conn_io));
  conn_io->local_addr_len = sizeof(conn_io->local_addr);
  if (getsockname(_socket, (struct sockaddr *)&conn_io->local_addr,
		  &conn_io->local_addr_len) != 0) {
    perror("failed to get local address of socket");
    std::exit(-1);
  };

  _conn = quiche_connect(_host.c_str(), (const uint8_t *) scid.data(), scid.size(),
			 (struct sockaddr *) &conn_io->local_addr,
			 conn_io->local_addr_len,
			 _peer_info->ai_addr, _peer_info->ai_addrlen, _config);

  if (_conn == NULL) {
    fprintf(stderr, "failed to create connection\n");
    std::exit(-1);
  }

  _qlog_filename = fmt::format("{}.qlog", fmt::join(scid, ""));
  auto qfile = fmt::format("{}/{}", _qlog_dir, _qlog_filename);

  if(quiche_conn_set_qlog_path(_conn, qfile.c_str(), "quiche-client qlog", "quiche-client qlog")) {
    fmt::print("Sucessfully enable qlog, file {}\n", qfile);
  }
  else {
    fmt::print("Failed to enable qlog, file {}\n", qfile);
  }

#ifdef QUICHE_DEBUG
  quiche_conn_set_keylog_path(_conn, "keylog.txt");
#endif
  
  conn_io->sock = _socket;
  conn_io->conn = _conn;
  conn_io->client = this;

  _loop = ev_default_loop(0);
  _watcher = std::make_unique<ev_io>();
  
  ev_io_init(_watcher.get(), ::recv_cb, conn_io->sock, EV_READ);
  ev_io_start(_loop, _watcher.get());
  _watcher->data = conn_io;

  ev_init(&conn_io->timer, timeout_cb);
  conn_io->timer.data = conn_io;
  
  flush_egress(_loop, conn_io);

  _th = std::thread([this](){ ev_run(_loop, 0); ev_loop_destroy(_loop); });
}

void QuicheClient::stop()
{
  fmt::print("Stopping the quiche client\n");
  quiche_conn_close(_conn, true, 0, NULL, 0);

  ev_timer_stop(_loop, &conn_io->timer);

  _async_watcher = std::make_unique<ev_async>();
  ev_async_init(_async_watcher.get(), async_callback);
  ev_async_start(_loop, _async_watcher.get());
  ev_async_send(_loop, _async_watcher.get());
  
  _th.join();

  quiche_conn_free(_conn);
  free(conn_io);

  conn_io = nullptr;
  _conn = nullptr;
  
  freeaddrinfo(_peer_info);
  close(_socket);
}

void QuicheClient::send_message_stream(const char * buffer, size_t len)
{
  {
    std::lock_guard<std::mutex> lock(client_flush_mutex);
    uint64_t id = (_stream_id++ << 2) | 0x02;
    
    if (int err = quiche_conn_stream_send(_conn, id, (const uint8_t*)buffer, len, true); err < 0) {
      fmt::print("failed to send HTTP request {}\n", err);
      return;
    }
  }
  
  flush_egress(_loop, conn_io);
}

void QuicheClient::send_message_datagram(const char * buffer, size_t len)
{
  {
    std::lock_guard<std::mutex> lock(client_flush_mutex);
    if(int err = quiche_conn_dgram_send(_conn, (const uint8_t*)buffer, len); err < 0) {
      fmt::print("failed to send dgram {}\n", err);
      return;
    }
  }

  flush_egress(_loop, conn_io);  
}

void QuicheClient::recv_cb(const uint8_t* buf, size_t len)
{
  if(_on_received_callback) _on_received_callback((const char*)buf, len);
}

Capabilities QuicheClient::get_capabilities()
{
  Capabilities caps;

  caps.impl = IMPL_NAME;
  caps.datagrams = true;
  caps.streams = true;
  
  caps.cc.push_back("newreno");
  caps.cc.push_back("cubic");
  caps.cc.push_back("BBRv1");
  caps.cc.push_back("BBRv2");

  return caps;
}
