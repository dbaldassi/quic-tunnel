#include "quiche_client.h"

#include <quiche.h>

#include <fmt/core.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ev.h>
#include <uthash.h>
#include <cinttypes>

#define LOCAL_CONN_ID_LEN 16

#define MAX_DATAGRAM_SIZE 2500

extern "C"
{

struct conn_io {
  ev_timer timer;

  int sock;

  struct sockaddr_storage local_addr;
  socklen_t local_addr_len;
  
  quiche_conn *conn;
  QuicheClient * client;
};

struct timeout_cb_data
{
  struct conn_io * conn_io;
  QuicheClient * server;
};
  
static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io)
{
  static uint8_t out[MAX_DATAGRAM_SIZE];

  quiche_send_info send_info;

  while (1) {
    ssize_t written = quiche_conn_send(conn_io->conn, out, sizeof(out),
				       &send_info);

    if (written == QUICHE_ERR_DONE) {
      fprintf(stderr, "done writing\n");
      break;
    }

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

    fprintf(stderr, "sent %zd bytes\n", sent);
  }

  double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
  conn_io->timer.repeat = t;
  ev_timer_again(loop, &conn_io->timer);
}

static void debug_log(const char *line, void *argp)
{
  fmt::print("{}\n", line);
}

static void recv_cb(EV_P_ ev_io *w, int revents)
{
  static uint8_t buf[65535];
  
  struct conn_io *conn_io = (struct conn_io*)w->data;
  // static bool req_sent = false;

  // fmt::print("recv_cb\n");
  
  while (1) {
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    memset(&peer_addr, 0, peer_addr_len);

    ssize_t read = recvfrom(conn_io->sock, buf, sizeof(buf), 0,
			    (struct sockaddr *) &peer_addr,
			    &peer_addr_len);

    if (read < 0) {
      if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
	fprintf(stderr, "recv would block\n");
	break;
      }

      perror("failed to read");
      return;
    }

    quiche_recv_info recv_info = {
      (struct sockaddr *) &peer_addr,
      peer_addr_len,

      (struct sockaddr *) &conn_io->local_addr,
      conn_io->local_addr_len,
    };

    ssize_t done = quiche_conn_recv(conn_io->conn, buf, read, &recv_info);

    if (done < 0) {
      fprintf(stderr, "failed to process packet\n");
      continue;
    }

    fprintf(stderr, "recv %zd bytes\n", done);
  }

  fprintf(stderr, "done reading\n");

  if (quiche_conn_is_closed(conn_io->conn)) {
    fprintf(stderr, "connection closed\n");

    ev_break(EV_A_ EVBREAK_ONE);
    return;
  }

  // if (quiche_conn_is_established(conn_io->conn) && !req_sent) {
  //   const uint8_t *app_proto;
  //   size_t app_proto_len;

  //   quiche_conn_application_proto(conn_io->conn, &app_proto, &app_proto_len);

  //   fprintf(stderr, "connection established: %.*s\n",
  // 	    (int) app_proto_len, app_proto);

  //   const static uint8_t r[] = "GET /index.html\r\n";
  //   if (quiche_conn_stream_send(conn_io->conn, 4, r, sizeof(r), true) < 0) {
  //     fprintf(stderr, "failed to send HTTP request\n");
  //     return;
  //   }

  //   fprintf(stderr, "sent HTTP request\n");

  //   req_sent = true;
  // }
  
  if (quiche_conn_is_established(conn_io->conn)) {
    uint64_t s = 0;

    quiche_stream_iter *readable = quiche_conn_readable(conn_io->conn);

    while (quiche_stream_iter_next(readable, &s)) {
      fprintf(stderr, "stream %" PRIu64 " is readable\n", s);

      bool fin = false;
      ssize_t recv_len = quiche_conn_stream_recv(conn_io->conn, s,
						 buf, sizeof(buf),
						 &fin);
      if (recv_len < 0) break;
      
      printf("%.*s", (int) recv_len, buf);
      
      conn_io->client->recv_cb(buf, recv_len);
      
      // if (fin) {
      // 	if (quiche_conn_close(conn_io->conn, true, 0, NULL, 0) < 0) {
      // 	  fprintf(stderr, "failed to close connection\n");
      // 	}
      // }
    }

    quiche_stream_iter_free(readable);

    if(auto dgram_len = quiche_conn_dgram_recv(conn_io->conn, buf, sizeof(buf)); dgram_len >= 0) {
      fmt::print("Dgram recv {}\n", dgram_len);
      conn_io->client->recv_cb(buf, dgram_len);
    }
    else {
      fmt::print("No dgram {}\n", dgram_len);
    }
  }

  flush_egress(loop, conn_io);
}

static void timeout_cb(EV_P_ ev_timer *w, int revents)
{
  struct conn_io *conn_io = (struct conn_io*)w->data;
  quiche_conn_on_timeout(conn_io->conn);

  fprintf(stderr, "timeout\n");

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

}

QuicheClient::QuicheClient(std::string host, int port) noexcept
  : _host(std::move(host)), _port(port)
{
  auto ver = quiche_version();
  fmt::print("Quiche version : {}\n", ver);

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
  quiche_config_set_initial_max_streams_bidi(_config, 100);
  quiche_config_set_initial_max_streams_uni(_config, 100);
  quiche_config_set_cc_algorithm(_config, QUICHE_CC_RENO);

  quiche_enable_debug_logging(debug_log, NULL);
}

QuicheClient::~QuicheClient()
{
  quiche_config_free(_config);
}

// -- quic client interface
void QuicheClient::set_qlog_filename(std::string filename) noexcept
{

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
  uint8_t scid[LOCAL_CONN_ID_LEN];
  int rng = open("/dev/urandom", O_RDONLY);
  if (rng < 0) {
    perror("failed to open /dev/urandom");
    std::exit(-1);
  }

  ssize_t rand_len = read(rng, &scid, sizeof(scid));
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

  _conn = quiche_connect(_host.c_str(), (const uint8_t *) scid, sizeof(scid),
			 (struct sockaddr *) &conn_io->local_addr,
			 conn_io->local_addr_len,
			 _peer_info->ai_addr, _peer_info->ai_addrlen, _config);

  if (_conn == NULL) {
    fprintf(stderr, "failed to create connection\n");
    std::exit(-1);
  }

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

  _th = std::thread([this](){ ev_loop(_loop, 0); });
}

void QuicheClient::stop()
{
  quiche_conn_free(_conn);
  freeaddrinfo(_peer_info);
  close(_socket);
}

void QuicheClient::send_message_stream(const char * buffer, size_t len)
{
  // _stream_id = quiche_conn_stream_writable_next(_conn);
  // fmt::print("Sending to stream {}, Writable ? {} \n", _stream_id, quiche_conn_stream_writable(_conn, _stream_id, len));
  // fmt::print("Left uni : {}, left bidi {}, next {}\n", quiche_conn_peer_streams_left_uni(_conn),
  // 	     quiche_conn_peer_streams_left_bidi(_conn),
  // 	     quiche_conn_stream_readable_next(_conn));

  // auto it = quiche_conn_writable(_conn);

  // uint64_t id;
  // while(quiche_stream_iter_next(it, &id)) {
  //   fmt::print("{} writable\n", id);
  // }
  
  // if (int err = quiche_conn_stream_send(_conn, _stream_id, (const uint8_t*)buffer, len, true); err < 0) {
  //   fmt::print("failed to send HTTP request {}\n", err);
  // }
  // else {
  //   fmt::print("successfully sent id\n\n");
  //   flush_egress(_loop, conn_io);
  // }

  // _stream_id++;

  send_message_datagram(buffer, len);
}

void QuicheClient::send_message_datagram(const char * buffer, size_t len)
{
  if(int err = quiche_conn_dgram_send(_conn, (const uint8_t*)buffer, len); err < 0) {
    fmt::print("failed to send dgram {}\n", err);
    return;
  }

  flush_egress(_loop, conn_io);
}

void QuicheClient::recv_cb(const uint8_t* buf, size_t len)
{
  if(_on_received_callback) {
    _on_received_callback((const char*)buf, len);
  }
}

Capabilities QuicheClient::get_capabilities()
{
  Capabilities caps;

  caps.impl = IMPL_NAME;
  caps.datagrams = true;
  caps.streams = true;
  
  caps.cc.push_back("NewReno");
  caps.cc.push_back("Cubic");
  caps.cc.push_back("BBRv1");
  caps.cc.push_back("BBRv2");

  return caps;
}
