#include "lsquic_client.h"

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

extern "C"
{

static lsquic_conn_ctx_t * lsquic_on_new_conn(void * stream_if_ctx, lsquic_conn_t * conn)
{
  auto client = reinterpret_cast<LsquicClient*>(stream_if_ctx);  
  return client->on_new_conn(conn);
}

  
static lsquic_stream_ctx_t * lsquic_on_new_stream(void * stream_if_ctx, lsquic_stream_t * stream)
{
  auto client = reinterpret_cast<LsquicClient*>(stream_if_ctx);
  return client->on_new_stream(stream);
}

static void lsquic_on_conn_closed (lsquic_conn_t * conn)
{
  fmt::print("lsquic On connection closed\n");
}

static void lsquic_on_read(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
  auto client = reinterpret_cast<LsquicClient*>(st_h);
  client->on_read(stream);
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
  auto client = reinterpret_cast<LsquicClient*>(ctx);
  // return client->send_packets_out(specs, n_specs);
}

static SSL_CTX * get_ssl_ctx(void *peer_ctx, const struct sockaddr *unused)
{
  fmt::print("get_ssl_ctx\n");

  auto client = reinterpret_cast<LsquicClient*>(peer_ctx);
  
  return client->get_ssl_ctx();
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

}

std::atomic<int> LsquicClient::_ref_count{0};

void LsquicClient::init_lsquic()
{
  fmt::print("lsquic global init\n");
  
  if (lsquic_global_init(LSQUIC_GLOBAL_CLIENT) != 0) {
    fmt::print("error: Could not init lsquic\n");
    exit(EXIT_FAILURE);
  }
}

void LsquicClient::exit_lsquic()
{
  lsquic_global_cleanup();
}


LsquicClient::LsquicClient(std::string host, int port) noexcept
{
   fmt::print("LsquicServer::LsquicServer {} {}\n", host, port);

  _ssl_ctx = SSL_CTX_new(TLS_method());

  SSL_CTX_set_min_proto_version(_ssl_ctx, TLS1_3_VERSION);
  SSL_CTX_set_max_proto_version(_ssl_ctx, TLS1_3_VERSION);
  SSL_CTX_set_default_verify_paths(_ssl_ctx);

  if(_ref_count == 0) init_lsquic();
  ++_ref_count;

  _logger_if.log_buf = lsquic_log;
  lsquic_set_log_level("Warning");
  lsquic_logger_init(&_logger_if, this, LLTS_NONE);

  memset(&_stream_if, 0, sizeof(_stream_if));
  memset(&_engine_api, 0, sizeof(_engine_api));
  
  _stream_if.on_new_conn = lsquic_on_new_conn;
  _stream_if.on_conn_closed = lsquic_on_conn_closed;
  _stream_if.on_new_stream = lsquic_on_new_stream;
  _stream_if.on_read = lsquic_on_read;
  _stream_if.on_write = lsquic_on_write;
  _stream_if.on_close = lsquic_on_stream_close;
  _stream_if.on_datagram = lsquic_on_datagram;

  lsquic_engine_init_settings(&_engine_settings, 0);
  _engine_settings.es_datagrams = true;

  fmt::print("QUIC VERSION : {}", _engine_settings.es_versions);
  
  _engine_api.ea_stream_if = &_stream_if;
  _engine_api.ea_stream_if_ctx = (void*)this;
  _engine_api.ea_get_ssl_ctx = ::get_ssl_ctx;
  _engine_api.ea_settings = &_engine_settings;
  _engine_api.ea_packets_out = lsquic_send_packets;
  _engine_api.ea_packets_out_ctx = (void*)this;
  _engine_api.ea_alpn = "echo";
  // _engine_api.ea_lookup_cert = lookup_cert;
  // _engine_api.ea_cert_lu_ctx = (void*)this;
  
  fmt::print("lsquic engine new\n");
  _engine = lsquic_engine_new(0, &_engine_api);
  if(!_engine) {
    fmt::print("lsquic engine not so new\n");
    std::exit(EXIT_SUCCESS);
  }

  _start = false;
}

// -- quic client interface
void LsquicClient::set_qlog_filename(std::string filename) noexcept
{

}

std::string_view LsquicClient::get_qlog_path()   const noexcept
{
  return _qlog_dir;
}

std::string_view LsquicClient::get_qlog_filename() const noexcept
{
  return _qlog_filename;
}

bool LsquicClient::set_cc(std::string_view cc) noexcept
{
  fmt::print("lsquic requested to set {} cc\n", cc);
  
  if(cc == "cubic")         _cc = Congestion::CUBIC;
  else if(cc == "bbrv1")    _cc = Congestion::BBRV1;
  else if(cc == "adaptive") _cc = Congestion::ADAPTIVE;
  else                      _cc = Congestion::DEFAULT_CC;
  
  return true;
}

void LsquicClient::start()
{
  memset((char *)&_addr_local, 0, sizeof(_addr_local));
  memset((char *)&_addr_peer, 0, sizeof(_addr_peer));

  _addr_local.sin_family = AF_INET;
  //   _addr_local.sin_port = ;
  _addr_local.sin_addr.s_addr = htonl(INADDR_ANY);

  auto host = gethostbyname(_host.c_str());
  
  _addr_peer.sin_family = AF_INET;
  _addr_peer.sin_port = htons(_port);
  memcpy(&_addr_peer.sin_addr, host->h_addr_list[0], host->h_length);
  
  _conn = lsquic_engine_connect(_engine, N_LSQVER,
				(struct sockaddr *)&_addr_local, (struct sockaddr *)&_addr_peer,
				(void *)this, NULL, NULL, 0, NULL, 0, NULL, 0);
}

void LsquicClient::stop()
{

}

void LsquicClient::send_message_stream(const char * buffer, size_t len)
{

}

void LsquicClient::send_message_datagram(const char * buffer, size_t len)
{

}

Capabilities LsquicClient::get_capabilities()
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

lsquic_conn_ctx_t * LsquicClient::on_new_conn(lsquic_conn_t *conn)
{
  fmt::print("LsquicClient::on_new_conn\n");
}

// lsquic callback
lsquic_stream_ctx_t * LsquicClient::on_new_stream(lsquic_stream_t * stream)
{
  fmt::print("LsquicClient::on_new_stream\n");
}

void LsquicClient::on_conn_closed(lsquic_conn_t * conn)
{
  fmt::print("LsquicClient::on_conn_closed\n");
}

void LsquicClient::on_read(lsquic_stream_t * stream)
{
  fmt::print("LsquicClient::on_read\n");

  unsigned char buf[MAX_BUF_LEN];
  ssize_t nr = lsquic_stream_read(stream, buf, sizeof(buf));

  fmt::print("Received {} bytes message : {}", nr, (char*)buf);

  if (nr == 0) /* EOF */ {
    lsquic_stream_shutdown(stream, 0);
    // lsquic_stream_wantwrite(stream, 1); /* Want to reply */
  }
}

void LsquicClient::on_write(lsquic_stream_t * stream)
{
  fmt::print("LsquicClient::on_write\n");
}

void LsquicClient::on_stream_close(lsquic_stream_t * stream)
{
  fmt::print("LsquicClient::on_stream_close\n");
}
