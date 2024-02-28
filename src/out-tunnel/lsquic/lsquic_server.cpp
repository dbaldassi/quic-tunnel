#include "lsquic_server.h"

#include <lsquic_types.h>
#include <lsquic.h>

#include <fmt/core.h>
#include <sys/time.h>

#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

#include <fstream>

#include <cstdlib>

#include "lsquic_hash.h"

extern "C"
{

struct ssl_ctx_st * lookup_cert (void *cert_lu_ctx, const struct sockaddr *sa_UNUSED, const char *sni)
{
  auto server = reinterpret_cast<LsquicServer*>(cert_lu_ctx);

  fmt::print("Lookup cert {}\n", (void*)server->get_cert_ctx());
  if(sni) fmt::print("sni : {} \n", sni);
  
  return server->get_cert_ctx();
}


static lsquic_conn_ctx_t * lsquic_on_new_conn(void * stream_if_ctx, lsquic_conn_t * conn)
{
  fmt::print("lsquic on new conn {}\n", stream_if_ctx);
  auto server = reinterpret_cast<LsquicServer*>(stream_if_ctx);  
  return server->on_new_conn(conn);
}

  
static lsquic_stream_ctx_t * lsquic_on_new_stream(void * stream_if_ctx, lsquic_stream_t * stream)
{
  fmt::print("lsquic on new stream\n");
  auto server = reinterpret_cast<LsquicServer*>(stream_if_ctx);
  return server->on_new_stream(stream);
}

static void lsquic_on_conn_closed (lsquic_conn_t * conn)
{
  fmt::print("lsquic On connection closed\n");
}

static void lsquic_on_read(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
  fmt::print("lsquic on read\n");
  auto server = reinterpret_cast<LsquicServer*>(st_h);
  server->on_read(stream);
}

static void lsquic_on_write(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
  fmt::print("lsquic On write\n");
}


static void lsquic_on_stream_close(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
  fmt::print("lsquic On stream close\n");
}

static void lsquic_on_datagram(lsquic_conn_t * conn, const void * buf, size_t size)
{
  fmt::print("lsquic On datagram\n");
}

static int lsquic_send_packets(void *ctx, const struct lsquic_out_spec *specs, unsigned n_specs)
{
  auto server = reinterpret_cast<LsquicServer*>(ctx);
  return server->send_packets_out(specs, n_specs);
}

static SSL_CTX * get_ssl_ctx(void *peer_ctx, const struct sockaddr *unused)
{
  fmt::print("get_ssl_ctx\n");

  auto server = reinterpret_cast<LsquicServer*>(peer_ctx);
  
  return server->get_ssl_ctx();
}

static int lsquic_log(void *logger_ctx, const char *buf, long unsigned int len)
{
  fmt::print("{}\n", buf);
  return 0;
}

static struct ssl_ctx_st * no_cert(void *cert_lu_ctx, const struct sockaddr *sa_UNUSED, const char *sni)
{
    return NULL;
}

static int
select_alpn (SSL *ssl, const unsigned char **out, unsigned char *outlen,
                    const unsigned char *in, unsigned int inlen, void *arg)
{
    int r;

    char s_alpn[0x100] = "0echo\0";
    s_alpn[0] = 4;
    
    r = SSL_select_next_proto((unsigned char **) out, outlen, in, inlen,
                                    (unsigned char *) s_alpn, strlen(s_alpn));
    if (r == OPENSSL_NPN_NEGOTIATED) {
      fmt::print("NPN nego OK\n");
      return SSL_TLSEXT_ERR_OK;
    }
    else {
      fmt::print("no supported protocol can be selected from {}\n",(char *) in);
      return SSL_TLSEXT_ERR_ALERT_FATAL;
    }
}


}

std::atomic<int> LsquicServer::_ref_count{0};

void LsquicServer::init_lsquic()
{
  fmt::print("lsquic global init\n");
  
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
  : _port(port), _socket(-1), _udp_socket(nullptr)
{
  fmt::print("LsquicServer::LsquicServer {} {}\n", host, port);

  _ssl_ctx = SSL_CTX_new(TLS_method());
  _cert_ctx = SSL_CTX_new(TLS_method());

  SSL_CTX_set_min_proto_version(_ssl_ctx, TLS1_3_VERSION);
  SSL_CTX_set_max_proto_version(_ssl_ctx, TLS1_3_VERSION);
  SSL_CTX_set_default_verify_paths(_ssl_ctx);

  SSL_CTX_set_min_proto_version(_cert_ctx, TLS1_3_VERSION);
  SSL_CTX_set_max_proto_version(_cert_ctx, TLS1_3_VERSION);
  SSL_CTX_set_default_verify_paths(_cert_ctx);
  SSL_CTX_set_alpn_select_cb(_cert_ctx, select_alpn, NULL);

  fmt::print("Lookup cert {}\n", (void*)_cert_ctx);
  
  // SSL_CTX_set_early_data_enabled(_cert_ctx, 1);
  // SSL_CTX_set_session_cache_mode(_cert_ctx, 1);

  // unsigned char ticket_keys[48];
  // memset(ticket_keys, 0, sizeof(ticket_keys));
  // if (1 != SSL_CTX_set_tlsext_ticket_keys(_ssl_ctx, ticket_keys, sizeof(ticket_keys))) {
  //   fmt::print("ERROR: SSL_CTX_set_tlsext_ticket_keys failed\n");
  //   // return -1;
  // }

  
  // SSL_CTX_set_early_data_enabled(prog->prog_ssl_ctx, 1);

  // std::ofstream keyfs("key.pem"), certfs("cert.pem");
  // keyfs << key;
  // certfs << certificate;

  // keyfs.close();
  // certfs.close();
  
  if(SSL_CTX_use_certificate_chain_file(_cert_ctx, "cert.pem") != 1) {
    perror("error: Could not use ssl certificate : \n");
    exit(EXIT_FAILURE);
  }

  SSL_CTX_use_certificate_chain_file(_ssl_ctx, "cert.pem");

  if(SSL_CTX_use_PrivateKey_file(_cert_ctx, "key.pem", SSL_FILETYPE_PEM) != 1) {
    fmt::print("error: Could not use ssl key\n");
    exit(EXIT_FAILURE);
  }

  SSL_CTX_use_PrivateKey_file(_ssl_ctx, "key.pem", SSL_FILETYPE_PEM);
    
  if(_ref_count == 0) init_lsquic();
  ++_ref_count;

  _engine_api = (struct lsquic_engine_api*)malloc(sizeof(struct lsquic_engine_api));
  _stream_if = (lsquic_stream_if*)malloc(sizeof(lsquic_stream_if));
  _logger_if = (lsquic_logger_if*)malloc(sizeof(lsquic_logger_if));
  _engine_settings = (lsquic_engine_settings*)malloc(sizeof(lsquic_engine_settings));


  _logger_if->log_buf = lsquic_log;
  lsquic_set_log_level("Warning");
  lsquic_logger_init(_logger_if, this, LLTS_NONE);

  memset(_stream_if, 0, sizeof(struct lsquic_stream_if));
  memset(_engine_api, 0, sizeof(struct lsquic_engine_api));
  
  _stream_if->on_new_conn = lsquic_on_new_conn;
  _stream_if->on_conn_closed = lsquic_on_conn_closed;
  _stream_if->on_new_stream = lsquic_on_new_stream;
  _stream_if->on_read = lsquic_on_read;
  _stream_if->on_write = lsquic_on_write;
  _stream_if->on_close = lsquic_on_stream_close;
  _stream_if->on_datagram = lsquic_on_datagram;

  lsquic_engine_init_settings(_engine_settings, LSENG_SERVER);
  _engine_settings->es_datagrams = true;
  _engine_settings->es_ecn = 0;

  fmt::print("QUIC VERSION : {}\n", _engine_settings->es_versions);
  
  _engine_api->ea_stream_if = _stream_if;
  _engine_api->ea_stream_if_ctx = (void*)this;
  _engine_api->ea_get_ssl_ctx = ::get_ssl_ctx;
  _engine_api->ea_settings = _engine_settings;
  _engine_api->ea_packets_out = lsquic_send_packets;
  _engine_api->ea_packets_out_ctx = (void*)this;
  _engine_api->ea_alpn = "echo";
  _engine_api->ea_lookup_cert = lookup_cert;
  _engine_api->ea_cert_lu_ctx = (void*)this;
  
  fmt::print("lsquic engine new\n");
  _engine = lsquic_engine_new(LSENG_SERVER, _engine_api);
  if(!_engine) {
    fmt::print("lsquic engine not so new\n");
    std::exit(EXIT_SUCCESS);
  }

  _start = false;
}

LsquicServer::~LsquicServer()
{
  free(_engine_api);
  free(_stream_if);
  free(_logger_if);
  free(_engine_settings);
  
  --_ref_count;
  
  if(_ref_count == 0) exit_lsquic();
}

void LsquicServer::init_socket()
{
  fmt::print("Init lsquic udp socket\n");
  
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

  _addr_peer.sin_family = AF_INET;
  _addr_peer.sin_port = addr.sin_port;
  _addr_peer.sin_addr.s_addr = addr.sin_addr.s_addr;
  _addr_peer_len = slen;

  return rec_len;
}


bool LsquicServer::start()
{
  fmt::print("Starting lsquic\n");
  
  init_socket();

  _start = true;

  std::thread([this]() {
    int diff;

    while(_start) {
      lsquic_engine_process_conns(_engine);

      if (lsquic_engine_earliest_adv_tick(_engine, &diff)) {
	std::this_thread::sleep_for(std::chrono::microseconds(diff));	
      }
      else {
	std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
      // std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    fmt::print("Exit Process cons\n");
  }).detach();
  
  fmt::print("Start receiving udp message\n");
  
  return true;
}

void LsquicServer::loop()
{
  while(_start) {
    auto len = recv();
    if(len == -1) break;

    auto result = lsquic_engine_packet_in(_engine, _buf, len,
					  (struct sockaddr*)&_addr_local, (struct sockaddr*)&_addr_peer,
					  (void*)this, 0);

    switch(result) {
    case 0:
      fmt::print("Packet was processed by a real connection.\n");
      break;
    case 1:
      fmt::print("Packet was handled successfully, but not by a connection\n");
      break;
    case -1:
      fmt::print("Error from lsquic_engine_packet_in\n");
      break;
    default:
      fmt::print("Unknown result from lsquic_engine_packet_in\n");
    }

    lsquic_engine_process_conns(_engine);
  }

  _cv.notify_all();
}

bool LsquicServer::set_datagrams(bool enable)
{
  _datagrams = enable;
  
  return true;
}
  
bool LsquicServer::set_cc(std::string_view cc) noexcept
{
  fmt::print("lsquic requested to set {} cc\n", cc);
  
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

  // std::unique_lock<std::mutex> lock(_cv_mutex);
  // _cv.wait(lock);
}

lsquic_conn_ctx_t * LsquicServer::on_new_conn(lsquic_conn_t *conn)
{
  fmt::print("LsquicServer::on_new_conn\n");
}

// lsquic callback
lsquic_stream_ctx_t * LsquicServer::on_new_stream(lsquic_stream_t * stream)
{
  fmt::print("LsquicServer::on_new_stream\n");
  lsquic_stream_wantread(stream, 1);
}

void LsquicServer::on_conn_closed(lsquic_conn_t * conn)
{
  fmt::print("LsquicServer::on_conn_closed\n");
}

void LsquicServer::on_read(lsquic_stream_t * stream)
{
  fmt::print("LsquicServer::on_read\n");

  unsigned char buf[MAX_BUF_LEN];
  ssize_t nr = lsquic_stream_read(stream, buf, sizeof(buf));

  fmt::print("Received {} bytes message : {}", nr, (char*)buf);

  if (nr == 0) /* EOF */ {
    lsquic_stream_shutdown(stream, 0);
    // lsquic_stream_wantwrite(stream, 1); /* Want to reply */
  }
}

void LsquicServer::on_write(lsquic_stream_t * stream)
{
  fmt::print("LsquicServer::on_write\n");
}

void LsquicServer::on_stream_close(lsquic_stream_t * stream)
{
  fmt::print("LsquicServer::on_stream_close\n");
}

int LsquicServer::send_packets_out(const struct lsquic_out_spec *specs, unsigned n_specs)
{
  fmt::print("LsquicServer::send_packets_out\n");
  
  struct msghdr msg;
  unsigned n;

  memset(&msg, 0, sizeof(msg));

  for (n = 0; n < n_specs; ++n) {
    msg.msg_name       = (void *) specs[n].dest_sa;
    msg.msg_namelen    = sizeof(struct sockaddr_in);
    msg.msg_iov        = specs[n].iov;
    msg.msg_iovlen     = specs[n].iovlen;
    if (sendmsg(_socket, &msg, 0) < 0) {
      perror("noooo");
      break;
    }
    else {
      // fmt::print("Sending at least something\n");
    }
  }

  // fmt::print("n : {} {}\n", n, n_specs);

  return (int) n;
}
