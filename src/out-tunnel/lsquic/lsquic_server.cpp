#include "openssl/evp.h"

#include "lsquic_server.h"

#include <event2/event.h>

#include <lsquic.h>

#include <openssl/ssl.h>

#include <fmt/core.h>
#include <sys/time.h>

#include <cstdlib>

extern "C"
{

static lsquic_conn_ctx_t * lsquic_on_new_conn(void * stream_if_ctx, lsquic_conn_t * conn)
{
  auto server = reinterpret_cast<LsquicServer*>(stream_if_ctx);
  return server->on_new_conn(conn);
}

  
static lsquic_stream_ctx_t * lsquic_on_new_stream(void * stream_if_ctx, lsquic_stream_t * stream)
{
  auto server = reinterpret_cast<LsquicServer*>(stream_if_ctx);
  return server->on_new_stream(stream);
}

static void lsquic_on_conn_closed (lsquic_conn_t * conn)
{
}

static void lsquic_on_read(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
}


static void lsquic_on_write(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
}


static void lsquic_on_stream_close(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
  
}

static void lsquic_on_datagram(lsquic_conn_t * conn, const void * buf, size_t size)
{
  
}

static int lsquic_send_packets(void *ctx, const struct lsquic_out_spec *specs, unsigned n_specs)
{
  auto server = reinterpret_cast<LsquicServer*>(ctx);
  return server->send_packets_out(specs, n_specs);
}
  
static SSL_CTX * init_ssl_ctx()
{
  fmt::print("init_ssl_ctx\n");
  
  SSL_CTX * ctx = SSL_CTX_new(TLS_method());

  SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
  SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
  SSL_CTX_set_default_verify_paths(ctx);
  
  return ctx;
}

static SSL_CTX * get_ssl_ctx(void *peer_ctx, const struct sockaddr *unused)
{
  fmt::print("get_ssl_ctx\n");
  
  static SSL_CTX * ctx = init_ssl_ctx();
  return ctx;
}
  
}

std::atomic<int> LsquicServer::_ref_count{0};

void LsquicServer::init_lsquic()
{
  if (lsquic_global_init(LSQUIC_GLOBAL_SERVER) != 0) {
    fmt::print("error: Could not init lsquic\n");
    exit(EXIT_FAILURE);
  }
}

void LsquicServer::exit_lsquic()
{
  lsquic_global_cleanup();
}

LsquicServer::LsquicServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket)
  : _port(port), _socket(-1), _udp_socket(udp_socket)
{
  fmt::print("LsquicServer::LsquicServer\n");
  
  if(_ref_count == 0) init_lsquic();
  ++_ref_count;

  _stream_if.on_new_conn = lsquic_on_new_conn;
  _stream_if.on_conn_closed = lsquic_on_conn_closed;
  _stream_if.on_new_stream = lsquic_on_new_stream;
  _stream_if.on_read = lsquic_on_read;
  _stream_if.on_write = lsquic_on_write;
  _stream_if.on_close = lsquic_on_stream_close;
  _stream_if.on_datagram = lsquic_on_datagram;

  _engine_settings.es_datagrams = true;
  
  _engine_api.ea_stream_if = &_stream_if;
  _engine_api.ea_stream_if_ctx = (void*)this;
  _engine_api.ea_get_ssl_ctx = get_ssl_ctx;
  _engine_api.ea_settings = &_engine_settings;
  _engine_api.ea_packets_out = lsquic_send_packets;
  _engine_api.ea_packets_out_ctx = (void*)this;

  _engine = lsquic_engine_new(LSENG_SERVER, &_engine_api);

  _start = false;
}

LsquicServer::~LsquicServer()
{
  --_ref_count;

  if(_ref_count == 0) exit_lsquic();
}

void LsquicServer::init_socket()
{
  _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  
  struct timeval optval;
  optval.tv_usec = 0;
  optval.tv_sec = 5;

  setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &optval, sizeof(optval));

  memset((char *)&_addr_local, 0, sizeof(_addr_local));
  memset((char *)&_addr_peer, 0, sizeof(_addr_peer));

  _addr_local.sin_family = AF_INET;
  _addr_local.sin_port = htons(_port);
  _addr_local.sin_addr.s_addr = htonl(INADDR_ANY);

  if(_socket == -1) {
    perror("Could not create UDP socket");
    std::exit(EXIT_FAILURE);
  }
  else if(bind(_socket, (struct sockaddr*)&_addr_local, sizeof(_addr_local)) == -1) {
    perror("Could not bind UDP socket");
    std::exit(EXIT_FAILURE);
  }
}

void LsquicServer::close_socket()
{
  if(_socket != -1) {
    ::close(_socket);
    _socket = -1;
  }
}

Capabilities LsquicServer::get_capabilities()
{
  Capabilities cap;
  cap.impl = IMPL_NAME;
  cap.datagrams = true;
  cap.streams = true;
  cap.cc.emplace_back("cubic");
  cap.cc.emplace_back("bbrv1");
  cap.cc.emplace_back("adaptive");
  
  return cap;
}
  
void LsquicServer::set_qlog_filename(std::string file_name)
{
  
}

ssize_t LsquicServer::recv()
{
  if(_socket == -1) return -1;
  
  struct sockaddr_in addr;
  socklen_t          slen = sizeof(addr);
  ssize_t rec_len;
  
  do {
    rec_len = recvfrom(_socket, _buf, MAX_BUF_LEN, 0, (struct sockaddr*)&addr, &slen);
    
    if(rec_len == -1) {
      if(_socket == -1) {
	puts("Closing UDP socket");
	return -1;
      }
      if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
	puts("Refreshing UDP socket");
      }
      else {
	perror("Could not receive data in UDP socket");
	return -1;
      }
    }
  } while(rec_len < 0);

  // _addr_other = addr;
  _addr_peer.sin_family = AF_INET;
  _addr_peer.sin_port = addr.sin_port;
  _addr_peer.sin_addr.s_addr = addr.sin_addr.s_addr;
  _addr_peer_len = slen;

  return rec_len;
}


void LsquicServer::start()
{
  init_socket();

  _start = true;

  _recv_thread = std::thread([this](){
  
    while(_start) {
      auto len = recv();

      auto result = lsquic_engine_packet_in(_engine, _buf, len,
					    (struct sockaddr*)&_addr_local, (struct sockaddr*)&_addr_peer,
					    (void*)this, 0);

      switch(result) {
      case 0:
	fmt::print("Packet was processed by a real connection.");
	break;
      case 1:
	fmt::print("Packet was handled successfully, but not by a connection");
	break;
      case -1:
	fmt::print("Error from lsquic_engine_packet_in");
	break;
      default:
	fmt::print("Unknown result from lsquic_engine_packet_in");
      }
    }
  });
}

bool LsquicServer::set_datagrams(bool enable)
{
  _datagrams = enable;
  
  return true;
}
  
bool LsquicServer::set_cc(std::string_view cc) noexcept
{  
  if(cc == "cubic")         _cc = Congestion::CUBIC;
  else if(cc == "bbrv1")    _cc = Congestion::BBRV1;
  else if(cc == "adaptive") _cc = Congestion::ADAPTIVE;
  else                      _cc = Congestion::DEFAULT_CC;
  
  return true;
}

void LsquicServer::stop()
{
  _start = false;
  close_socket();

  _recv_thread.join();
}

lsquic_conn_ctx_t * LsquicServer::on_new_conn(lsquic_conn_t *conn)
{
  fmt::print("LsquicServer::on_new_conn\n");
}

// lsquic callback
lsquic_stream_ctx_t * LsquicServer::on_new_stream(lsquic_stream_t * stream)
{
  fmt::print("LsquicServer::on_new_stream\n");
}

void LsquicServer::on_conn_closed(lsquic_conn_t * conn)
{
  fmt::print("LsquicServer::on_conn_closed\n");
}

void LsquicServer::on_read(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
  fmt::print("LsquicServer::on_read\n");
}

void LsquicServer::on_write(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
  fmt::print("LsquicServer::on_write\n");
}

void LsquicServer::on_stream_close(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
  fmt::print("LsquicServer::on_stream_close\n");
}

int LsquicServer::send_packets_out(const struct lsquic_out_spec *specs, unsigned n_specs)
{
  struct msghdr msg;
  unsigned n;

  memset(&msg, 0, sizeof(msg));

  for (n = 0; n < n_specs; ++n)
    {
      msg.msg_name       = (void *) specs[n].dest_sa;
      msg.msg_namelen    = sizeof(struct sockaddr_in);
      msg.msg_iov        = specs[n].iov;
      msg.msg_iovlen     = specs[n].iovlen;
      if (sendmsg(_socket, &msg, 0) < 0) break;
    }

  return (int) n;
}
