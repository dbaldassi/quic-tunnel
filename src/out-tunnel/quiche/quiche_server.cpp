#include "quiche_server.h"

#include <iostream>
#include <quiche.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <array>
#include <span>
#include <algorithm>
#include <mutex>

#include <unistd.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ev.h>
#include <cinttypes>

constexpr size_t LOCAL_CONN_ID_LEN = 16;
constexpr size_t MAX_DATAGRAM_SIZE = 1350;

std::mutex server_flush_mutex;

#define MAX_TOKEN_LEN \
    sizeof("quiche") - 1 + \
    sizeof(struct sockaddr_storage) + \
    QUICHE_MAX_CONN_ID_LEN


static std::string get_quiche_error(ssize_t code)
{
  switch(code) {
  case QUICHE_ERR_DONE: return "QUICHE_ERR_DONE";
  case QUICHE_ERR_BUFFER_TOO_SHORT: return "QUICHE_ERR_BUFFER_TOO_SHORT";
  case QUICHE_ERR_UNKNOWN_VERSION: return "QUICHE_ERR_UNKNOWN_VERSION";
  case QUICHE_ERR_INVALID_FRAME: return "QUICHE_ERR_INVALID_FRAME";
  case QUICHE_ERR_INVALID_PACKET: return "QUICHE_ERR_INVALID_PACKET";
  case QUICHE_ERR_INVALID_STATE: return "QUICHE_ERR_INVALID_STATE";
  case QUICHE_ERR_INVALID_STREAM_STATE: return "QUICHE_ERR_INVALID_STREAM_STATE";
  case QUICHE_ERR_INVALID_TRANSPORT_PARAM: return "QUICHE_ERR_INVALID_TRANSPORT_PARAM";
  case QUICHE_ERR_CRYPTO_FAIL: return "QUICHE_ERR_CRYPTO_FAIL";
  case QUICHE_ERR_TLS_FAIL: return "QUICHE_ERR_TLS_FAIL";
  case QUICHE_ERR_FLOW_CONTROL: return "QUICHE_ERR_FLOW_CONTROL";
  case QUICHE_ERR_STREAM_LIMIT: return "QUICHE_ERR_STREAM_LIMIT";
  case QUICHE_ERR_STREAM_STOPPED: return "QUICHE_ERR_STREAM_STOPPED";    
  case QUICHE_ERR_STREAM_RESET: return "QUICHE_ERR_STREAM_RESET";
  case QUICHE_ERR_FINAL_SIZE: return "QUICHE_ERR_FINAL_SIZE";
  case QUICHE_ERR_CONGESTION_CONTROL: return "QUICHE_ERR_CONGESTION_CONTROL";
  case QUICHE_ERR_ID_LIMIT: return "QUICHE_ERR_ID_LIMIT";
  case QUICHE_ERR_OUT_OF_IDENTIFIERS: return "QUICHE_ERR_OUT_OF_IDENTIFIERS";
  case QUICHE_ERR_KEY_UPDATE: return "QUICHE_ERR_KEY_UPDATE";
  default: return "Unknown quiche error";
  }
}

extern "C"
{

struct conn_io
{
  ev_timer timer;

  int sock;
  
  uint8_t cid[LOCAL_CONN_ID_LEN];

  quiche_conn *conn;

  struct sockaddr_storage peer_addr;
  socklen_t               peer_addr_len;
};

struct timeout_cb_data
{
  struct conn_io * conn_io;
  QuicheServer   * server;
};
  
static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io)
{  
  static uint8_t out[MAX_DATAGRAM_SIZE];

  quiche_send_info send_info;

  while (1) {
    std::lock_guard<std::mutex> lock(server_flush_mutex);
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

  double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
  conn_io->timer.repeat = t;
  ev_timer_again(loop, &conn_io->timer);
}
  
static void mint_token(const uint8_t *dcid, size_t dcid_len,
                       struct sockaddr_storage *addr, socklen_t addr_len,
                       uint8_t *token, size_t *token_len)
{
  memcpy(token, "quiche", sizeof("quiche") - 1);
  memcpy(token + sizeof("quiche") - 1, addr, addr_len);
  memcpy(token + sizeof("quiche") - 1 + addr_len, dcid, dcid_len);

  *token_len = sizeof("quiche") - 1 + addr_len + dcid_len;
}

static bool validate_token(const uint8_t *token, size_t token_len,
                           struct sockaddr_storage *addr, socklen_t addr_len,
                           uint8_t *odcid, size_t *odcid_len)
{
  if ((token_len < sizeof("quiche") - 1) ||
      memcmp(token, "quiche", sizeof("quiche") - 1)) {
    return false;
  }

  token += sizeof("quiche") - 1;
  token_len -= sizeof("quiche") - 1;

  if ((token_len < addr_len) || memcmp(token, addr, addr_len)) {
    return false;
  }

  token += addr_len;
  token_len -= addr_len;

  if (*odcid_len < token_len) {
    return false;
  }

  memcpy(odcid, token, token_len);
  *odcid_len = token_len;

  return true;
}

static uint8_t *gen_cid(uint8_t *cid, size_t cid_len)
{
  int rng = open("/dev/urandom", O_RDONLY);
  if (rng < 0) {
    perror("failed to open /dev/urandom");
    return NULL;
  }

  ssize_t rand_len = read(rng, cid, cid_len);
  if (rand_len < 0) {
    perror("failed to create connection ID");
    return NULL;
  }

  return cid;
}

static void timeout_cb(EV_P_ ev_timer *w, int revents);

static struct conn_io *create_conn(uint8_t *scid, size_t scid_len,
				   uint8_t *odcid, size_t odcid_len,
				   struct sockaddr *local_addr,
				   socklen_t local_addr_len,
				   struct sockaddr_storage *peer_addr,
				   socklen_t peer_addr_len,
				   QuicheServer * server)
{
  struct conn_io *conn_io = (struct conn_io*)calloc(1, sizeof(*conn_io));
  if (conn_io == NULL) {
    fprintf(stderr, "failed to allocate connection IO\n");
    return NULL;
  }

  if (scid_len != LOCAL_CONN_ID_LEN) {
    fprintf(stderr, "failed, scid length too short\n");
  }

  memcpy(conn_io->cid, scid, LOCAL_CONN_ID_LEN);

  quiche_conn *conn = quiche_accept(conn_io->cid, LOCAL_CONN_ID_LEN,
				    odcid, odcid_len,
				    local_addr,
				    local_addr_len,
				    (struct sockaddr *) peer_addr,
				    peer_addr_len,
				    server->get_config());

  if (conn == NULL) {
    fprintf(stderr, "failed to create connection\n");
    return NULL;
  }

  // std::span<uint8_t> scid_view(scid, scid_len);
  // server->set_qlog_filename(fmt::format("{}.qlog", fmt::join(scid_view, "")));
  
  // auto qfile = fmt::format("{}/{}", server->get_qlog_path(), server->get_qlog_filename());

  // if(quiche_conn_set_qlog_path(conn, qfile.c_str(), "quiche-client qlog", "quiche-client qlog")) {
  //   fmt::print("Sucessfully enable qlog, file {}\n", qfile);
  // }
  // else {
  //   fmt::print("Failed to enable qlog, file {}\n", qfile);
  // }
  
  server->set_conn(conn);
  
  conn_io->sock = server->get_sock();
  conn_io->conn = conn;

  memcpy(&conn_io->peer_addr, peer_addr, peer_addr_len);
  conn_io->peer_addr_len = peer_addr_len;

  struct timeout_cb_data * data = (struct timeout_cb_data *)malloc(sizeof(struct timeout_cb_data));
  data->conn_io = conn_io;
  data->server = server;
    
  ev_init(&conn_io->timer, timeout_cb);
  conn_io->timer.data = (void*)data;

  server->conn_io = conn_io;
  fprintf(stderr, "new connection\n");

  return conn_io;
}

static void recv_cb(EV_P_ ev_io *w, int revents)
{
  struct conn_io *tmp, *conn_io = NULL;

  static uint8_t buf[65535];
  static uint8_t out[MAX_DATAGRAM_SIZE];

  QuicheServer* server = reinterpret_cast<QuicheServer*>(w->data);
  
  while (1) {
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    memset(&peer_addr, 0, peer_addr_len);

    fmt::print("buf size ? {}\n", sizeof(buf));
    ssize_t read = recvfrom(server->get_sock(), buf, sizeof(buf), 0,
			    (struct sockaddr *) &peer_addr,
			    &peer_addr_len);

    if (read < 0) {
      if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) break; 
      perror("failed to read");
      return;
    }

    fmt::print("Read {} bytes\n", read);

    uint8_t type;
    uint32_t version;

    uint8_t scid[QUICHE_MAX_CONN_ID_LEN];
    size_t scid_len = sizeof(scid);

    uint8_t dcid[QUICHE_MAX_CONN_ID_LEN];
    size_t dcid_len = sizeof(dcid);

    uint8_t odcid[QUICHE_MAX_CONN_ID_LEN];
    size_t odcid_len = sizeof(odcid);

    uint8_t token[MAX_TOKEN_LEN];
    size_t token_len = sizeof(token);

    int rc = quiche_header_info(buf, read, LOCAL_CONN_ID_LEN, &version,
				&type, scid, &scid_len, dcid, &dcid_len,
				token, &token_len);
    if (rc < 0) {
      fprintf(stderr, "failed to parse header: %d\n", rc);
      continue;
    }

    conn_io = server->conn_io;

    if (conn_io == NULL) {
      if (!quiche_version_is_supported(version)) {

	ssize_t written = quiche_negotiate_version(scid, scid_len,
						   dcid, dcid_len,
						   out, sizeof(out));

	if (written < 0) {
	  fprintf(stderr, "failed to create vneg packet: %zd\n", written);
	  continue;
	}

	ssize_t sent = sendto(server->get_sock(), out, written, 0,
			      (struct sockaddr *) &peer_addr,
			      peer_addr_len);
	if (sent != written) {
	  perror("failed to send");
	  continue;
	}

	continue;
      }

      if (token_len == 0) {
	fprintf(stderr, "stateless retry\n");

	mint_token(dcid, dcid_len, &peer_addr, peer_addr_len,
		   token, &token_len);

	uint8_t new_cid[LOCAL_CONN_ID_LEN];

	if (gen_cid(new_cid, LOCAL_CONN_ID_LEN) == NULL) {
	  continue;
	}

	ssize_t written = quiche_retry(scid, scid_len,
				       dcid, dcid_len,
				       new_cid, LOCAL_CONN_ID_LEN,
				       token, token_len,
				       version, out, sizeof(out));

	if (written < 0) {
	  fprintf(stderr, "failed to create retry packet: %zd\n", written);
	  continue;
	}

	ssize_t sent = sendto(server->get_sock(), out, written, 0,
			      (struct sockaddr *) &peer_addr,
			      peer_addr_len);
	if (sent != written) {
	  perror("failed to send");
	  continue;
	}

	continue;
      }


      if (!validate_token(token, token_len, &peer_addr, peer_addr_len,
			  odcid, &odcid_len)) {
	fprintf(stderr, "invalid address validation token\n");
	continue;
      }

      conn_io = create_conn(dcid, dcid_len, odcid, odcid_len,
			    server->get_local_addr(), server->get_local_addr_len(),
			    &peer_addr, peer_addr_len, server);

      if (conn_io == NULL) continue;
      
    }

    quiche_recv_info recv_info = {
      (struct sockaddr *)&peer_addr,
      peer_addr_len,

      server->get_local_addr(),
      server->get_local_addr_len(),
    };

    ssize_t done;
    {
      std::lock_guard<std::mutex> lock(server_flush_mutex);
      done = quiche_conn_recv(server->get_conn(), buf, read, &recv_info);
    }

    if (done < 0) {
      fmt::print("Failed to process packet: {} {}\n", done, get_quiche_error(done));
      break;
    }

    if (quiche_conn_is_established(server->get_conn())) {
      uint64_t s = 0;

      quiche_stream_iter *readable = quiche_conn_readable(server->get_conn());

      while (quiche_stream_iter_next(readable, &s)) {
	std::lock_guard<std::mutex> lock(server_flush_mutex);
	
	bool fin = false;
	ssize_t recv_len = quiche_conn_stream_recv(server->get_conn(), s,
						   buf, sizeof(buf),
						   &fin);

	if (recv_len < 0) break;
	
	server->on_recv(buf, recv_len);
      }

      quiche_stream_iter_free(readable);

      if(auto dgram_len = quiche_conn_dgram_recv(server->get_conn(), buf, sizeof(buf)); dgram_len >= 0) {
	server->on_recv(buf, dgram_len);
      }
    }
  }

  if(conn_io) {
    flush_egress(loop, conn_io);

    if (quiche_conn_is_closed(conn_io->conn)) {
      ev_timer_stop(loop, &conn_io->timer);
    }
  }
}

static void timeout_cb(EV_P_ ev_timer *w, int revents)
{
  struct timeout_cb_data *data = reinterpret_cast<struct timeout_cb_data*>(w->data);

  {
    std::lock_guard<std::mutex> lock(server_flush_mutex);
    quiche_conn_on_timeout(data->conn_io->conn);

    if (quiche_conn_is_closed(data->conn_io->conn)) {
      ev_timer_stop(loop, &data->conn_io->timer);
      return;
    }
  }

  flush_egress(loop, data->conn_io);
}

static void debug_log(const char *line, void *argp)
{
  std::FILE * debug_file = std::fopen("quiche_server_log.txt", "w");
  fmt::print(debug_file, "{}\n", line);
}

static void async_callback(EV_P_ ev_async*, int)
{
  fmt::print("break loop\n");
  ev_break(EV_A_ EVBREAK_ONE);
}

}

QuicheServer::QuicheServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket)
  : _host(host), _port(port), _udp_socket(udp_socket), conn_io(nullptr), _conn(nullptr), _loop(nullptr), _socket{-1}, _stream_id{0}
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
  
  err = quiche_config_load_cert_chain_from_pem_file(_config, "./cert.pem");
  if(err < 0) {
    fmt::print("QUICHE error, could not load certificate {}\n", err);
  }
  
  err = quiche_config_load_priv_key_from_pem_file(_config, "./key.pem");

  if(err < 0) {
    fmt::print("QUICHE error, could not load prv key {}\n", err);
  }

  quiche_config_enable_dgram(_config, true, 2048, 2048);

  // quiche_config_set_max_idle_timeout(_config, 5000);
  quiche_config_set_max_recv_udp_payload_size(_config, MAX_DATAGRAM_SIZE);
  quiche_config_set_max_send_udp_payload_size(_config, MAX_DATAGRAM_SIZE);
  quiche_config_set_initial_max_data(_config, 10'000'000);
  quiche_config_set_initial_max_stream_data_bidi_local(_config, 1'000'000);
  quiche_config_set_initial_max_stream_data_bidi_remote(_config, 1'000'000);
  quiche_config_set_initial_max_stream_data_uni(_config, 1'000'000);
  quiche_config_set_initial_max_streams_bidi(_config, 1'000'000);
  quiche_config_set_initial_max_streams_uni(_config, 1'000'000);
  quiche_config_set_cc_algorithm(_config, QUICHE_CC_RENO);
  quiche_config_set_disable_active_migration(_config, true);
  
  fmt::print("Seems everything went well\n");
}

QuicheServer::~QuicheServer()
{
  quiche_config_free(_config);  
}

Capabilities QuicheServer::get_capabilities()
{
  Capabilities caps;

  caps.impl = IMPL_NAME;
  caps.streams = true;
  caps.datagrams = true;

  caps.cc.push_back("newreno");
  caps.cc.push_back("cubic");
  caps.cc.push_back("BBRv1");
  caps.cc.push_back("BBRv2");
  
  return caps;
}
  
void QuicheServer::set_qlog_filename(std::string file_name)
{
  _qlog_file = std::move(file_name);
}

bool QuicheServer::init_socket()
{
  _local_len = sizeof(_local);
  _peer_len = sizeof(_peer);

  _local.sin_family = AF_INET;
  _local.sin_port = htons(_port);
  _local.sin_addr.s_addr = htonl(INADDR_ANY);
  
  _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(_socket < 0) {
    perror("Quicheserver could not create socket");
    return false;
  }

  if(fcntl(_socket, F_SETFL, O_NONBLOCK) < 0) {
    perror("Quicheserver could not set unblokcing");
    return false;
  }

  fmt::print("Trying to bind {} {}\n", _port, _local_len);
  
  if(bind(_socket, (struct sockaddr*)&_local, _local_len) < 0) {
    perror("Quicheserver could not bind socket");
    return false;
  }

  return true;
}

void QuicheServer::start()
{
  if(!init_socket()) return;

  _udp_socket->set_callback(this);
  _udp_socket->start();
  
  ev_io watcher;

  _loop = ev_default_loop(0);

  ev_io_init(&watcher, recv_cb, _socket, EV_READ);
  ev_io_start(_loop, &watcher);
  watcher.data = (void*)this;

  _async_watcher = std::make_unique<struct ev_async>();
  ev_async_init(_async_watcher.get(), async_callback);
  ev_async_start(_loop, _async_watcher.get());

  ev_loop(_loop, 0);
  ev_loop_destroy(_loop);
    
  _loop = nullptr;
}

bool QuicheServer::set_datagrams(bool enable)
{
  _datagrams = enable;
  return true;
}

bool QuicheServer::set_cc(std::string_view cc) noexcept
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

void QuicheServer::stop()
{
  fmt::print("Closing out udp socket\n");
  _udp_socket->close();

  if(_loop) {
    fmt::print("Closing ev loop\n");

    if(conn_io) ev_timer_stop(_loop, &conn_io->timer);
 
    ev_async_send(_loop, _async_watcher.get());
  }
  
  if(conn_io) {
    fmt::print("free conn_io\n");
    free(conn_io);
    conn_io = nullptr;
  }

  if(_conn) {
    fmt::print("free connection\n");
    
    quiche_conn_close(_conn, true, 0, NULL, 0);
    quiche_conn_free(_conn);
    _conn = nullptr;
  }

  if(_socket != -1) {
    fmt::print("Closing quiche socket\n");
    close(_socket);
    _socket = -1;
  }
}

void QuicheServer::on_recv(const uint8_t * buf, size_t len)
{
  // fmt::print("on recv: {}\n", len);
  _udp_socket->send((const char*)buf, len);
} 

void QuicheServer::onUdpMessage(const char* buffer, size_t len) noexcept
{
  if(_datagrams && len < MAX_DATAGRAM_SIZE) {
    std::lock_guard<std::mutex> lock(server_flush_mutex);
    
    if (quiche_conn_dgram_send(_conn, (const uint8_t*)buffer, len) < 0) {
      fmt::print("Failed to send data in dgram\n");
      return;
    }
  }
  else {
    std::lock_guard<std::mutex> lock(server_flush_mutex);
    
    uint64_t id = (_stream_id++ << 2) | 0x03;
    // uint64_t id = (_stream_id << 2) | 0x03;

    std::vector<char> v(len);
    memcpy(v.data(), buffer, len);
    _queue.push(v);
    
    while(!_queue.empty()) {
      if (auto code = quiche_conn_stream_send(_conn, id, (const unsigned char*)_queue.front().data(), _queue.front().size(), true); code < 0) {
	auto cap = quiche_conn_stream_capacity(_conn, id);
	fmt::print("Send Capacity : {}\n", cap);
	fmt::print("Failed to send data in stream, id {}, error {} : {}\n", id, code, get_quiche_error(code));
	return;
      }

      _queue.pop();
    }
    
    auto cap = quiche_conn_stream_capacity(_conn, id);
    fmt::print("Send Capacity : {}\n", cap);
  }

  flush_egress(_loop, conn_io);
}

