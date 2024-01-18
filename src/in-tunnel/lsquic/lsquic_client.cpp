#include "lsquic_client.h"

#include <lsquic_types.h>
#include <lsquic.h>

#include <fmt/core.h>
#include <sys/time.h>

#include <limits.h>

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

void lsquic_hexstr(const unsigned char *buf, size_t bufsz, char *out, size_t outsz)
{
    static const char b2c[] = "0123456789ABCDEF";
    const unsigned char *const end_input = buf + bufsz;
    char *const end_output = out + outsz;

    while (buf < end_input && out + 2 < end_output)
    {
        *out++ = b2c[ *buf >> 4 ];
        *out++ = b2c[ *buf & 0xF ];
        ++buf;
    }

    if (buf < end_input)
        out[-1] = '!';

    *out = '\0';
}

static lsquic_conn_ctx_t * lsquic_on_new_conn(void * stream_if_ctx, lsquic_conn_t * conn)
{
  fmt::print("lsquic on new conn {}\n", stream_if_ctx);
  auto client = reinterpret_cast<LsquicClient*>(stream_if_ctx);  
  return client->on_new_conn(conn);
}

  
static lsquic_stream_ctx_t * lsquic_on_new_stream(void * stream_if_ctx, lsquic_stream_t * stream)
{
  fmt::print("lsquic on new stream\n");
  auto client = reinterpret_cast<LsquicClient*>(stream_if_ctx);
  return client->on_new_stream(stream);
}

static void lsquic_on_conn_closed (lsquic_conn_t * conn)
{
  fmt::print("lsquic On connection closed\n");
}

static void lsquic_on_read(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
   fmt::print("lsquic On read\n");
  auto client = reinterpret_cast<LsquicClient*>(st_h);
  client->on_read(stream);
}

static void lsquic_on_write(lsquic_stream_t * stream, lsquic_stream_ctx_t * st_h)
{
  // fmt::print("lsquic On write\n");
  auto client = reinterpret_cast<LsquicClient*>(st_h);
  client->on_write(stream);
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
  return client->send_packets_out(specs, n_specs);
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

static FILE * keylog_open_file(const SSL *ssl)
{
  const lsquic_conn_t *conn;
  const lsquic_cid_t *cid;
  FILE *fh;
  int sz;
  char id_str[MAX_CID_LEN * 2 + 1];
  char path[PATH_MAX];

  conn = lsquic_ssl_to_conn(ssl);
  cid = lsquic_conn_id(conn);
  lsquic_hexstr(cid->idbuf, cid->len, id_str, sizeof(id_str));
  sz = snprintf(path, sizeof(path), "%s/%s.keys", ".", id_str);
  if ((size_t) sz >= sizeof(path))
    {
      fmt::print("{}: file too long", __func__);
      return NULL;
    }
  fh = fopen(path, "ab");
  if (!fh)
    fmt::print("could not open {} for appending: {}", path, strerror(errno));
  return fh;
}

  
static void keylog_log_line (const SSL *ssl, const char *line)
{
  FILE *file;

  file = keylog_open_file(ssl);
  if (file)
    {
      fputs(line, file);
      fputs("\n", file);
      fclose(file);
    }
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
  : _host(host), _port(port)
{
   fmt::print("LsquicServer::LsquicServer {} {}\n", host, port);

  _ssl_ctx = SSL_CTX_new(TLS_method());

  SSL_CTX_set_min_proto_version(_ssl_ctx, TLS1_3_VERSION);
  SSL_CTX_set_max_proto_version(_ssl_ctx, TLS1_3_VERSION);
  SSL_CTX_set_default_verify_paths(_ssl_ctx);

  SSL_CTX_set_keylog_callback(_ssl_ctx, keylog_log_line);
  
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

  lsquic_engine_init_settings(_engine_settings, 0);
  _engine_settings->es_datagrams = 1;
  _engine_settings->es_ecn = 0;

  fmt::print("QUIC VERSION : {}\n", _engine_settings->es_versions);
  
  _engine_api->ea_stream_if = _stream_if;
  _engine_api->ea_stream_if_ctx = (void*)this;
  _engine_api->ea_get_ssl_ctx = ::get_ssl_ctx;
  _engine_api->ea_settings = _engine_settings;
  _engine_api->ea_packets_out = lsquic_send_packets;
  _engine_api->ea_packets_out_ctx = (void*)this;
  _engine_api->ea_alpn = "echo";
  // _engine_api.ea_lookup_cert = lookup_cert;
  // _engine_api.ea_cert_lu_ctx = (void*)this;
  
  fmt::print("lsquic engine new\n");
  _engine = lsquic_engine_new(0, _engine_api);
  if(!_engine) {
    fmt::print("lsquic engine not so new\n");
    std::exit(EXIT_SUCCESS);
  }

  _start = false;
}

LsquicClient::~LsquicClient()
{
  free(_engine_api);
  free(_stream_if);
  free(_logger_if);
  free(_engine_settings);
  
  --_ref_count;
  
  if(_ref_count == 0) exit_lsquic();
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

void LsquicClient::init_socket()
{
  fmt::print("Init lsquic udp socket\n");
  
  _socket = socket(AF_INET, SOCK_DGRAM, 0);
  
  struct timeval optval;
  optval.tv_usec = 0;
  optval.tv_sec = 5;

  setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &optval, sizeof(optval));

  // memset((char *)&_addr_local, 0, sizeof(_addr_local));
  // memset((char *)&_addr_peer, 0, sizeof(_addr_peer));

  // _addr_local.sin_family = AF_INET;
  // _addr_local.sin_port = htons(_port);
  // _addr_local.sin_addr.s_addr = htonl(INADDR_ANY);

  if(_socket == -1) {
    perror("Could not create UDP socket");
    std::exit(EXIT_FAILURE);
  }
  // else if(bind(_socket, (struct sockaddr*)&_addr_local, sizeof(_addr_local)) == -1) {
  //   perror("Could not bind UDP socket");
  //   std::exit(EXIT_FAILURE);
  // }
}

void LsquicClient::close_socket()
{
  if(_socket != -1) {
    ::close(_socket);
    _socket = -1;
  }
}

ssize_t LsquicClient::recv()
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

  // _addr_peer.sin_family = AF_INET;
  // _addr_peer.sin_port = addr.sin_port;
  // _addr_peer.sin_addr.s_addr = addr.sin_addr.s_addr;
  // _addr_peer_len = slen;

  return rec_len;
}


ssize_t LsquicClient::recv_msg()
{
  // struct msghdr msg = {
  //       .msg_name       = &_addr_peer,
  //       .msg_namelen    = sizeof(_addr_peer),
  //       .msg_iov        = &packs_in->vecs[iter->ri_idx],
  //       .msg_iovlen     = 1,
  //       .msg_control    = ctl_buf,
  //       .msg_controllen = CTL_SZ,
  //  };

  //  auto nread = recvmsg(_socket, &msg, 0);

  //  if (-1 == nread) {
  //    if (!(EAGAIN == errno || EWOULDBLOCK == errno))
  //      LSQ_ERROR("recvmsg: %s", strerror(errno));
  //    return ROP_ERROR;
  //  }
  //  if (msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC))
  //    {
  //      if (msg.msg_flags & MSG_TRUNC)
  // 	 LSQ_INFO("packet truncated - drop it");
  //      if (msg.msg_flags & MSG_CTRUNC)
  // 	 LSQ_WARN("packet's auxilicary data truncated - drop it");
  //      goto top;
  //    }
}


void LsquicClient::start()
{
  memset((char *)&_addr_local, 0, sizeof(_addr_local));
  memset((char *)&_addr_peer, 0, sizeof(_addr_peer));

  _addr_local.sin_family = AF_INET;
  _addr_local.sin_port = 0;
  _addr_local.sin_addr.s_addr = htonl(INADDR_ANY);

  bind(_socket, (struct sockaddr*)&_addr_local, sizeof(_addr_local));

  auto host = gethostbyname(_host.c_str());
  
  _addr_peer.sin_family = AF_INET;
  _addr_peer.sin_port = htons(_port);
  memcpy(&_addr_peer.sin_addr, host->h_addr_list[0], host->h_length);

  init_socket();
  
  _conn = lsquic_engine_connect(_engine, N_LSQVER,
				(struct sockaddr *)&_addr_local, (struct sockaddr *)&_addr_peer,
				(void *)this, NULL, NULL, 0, NULL, 0, NULL, 0);

  lsquic_engine_process_conns(_engine);
  
  _thread = std::thread([this](){
    _run = true;
    int diff;
    
    while(_run) {
      lsquic_engine_process_conns(_engine);

      if (lsquic_engine_earliest_adv_tick(_engine, &diff)) {
	std::this_thread::sleep_for(std::chrono::microseconds(diff));	
      }
      else {
	std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    }
  });

  _recv_th = std::thread([this](){
    _run = true;

    while(_run) {
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
  });
}

void LsquicClient::stop()
{
  _run = false;
  _thread.join();
  _recv_th.join();
  close_socket();
  
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
  // lsquic_engine_process_conns(_engine);
  lsquic_conn_make_stream(conn);
}

// lsquic callback
lsquic_stream_ctx_t * LsquicClient::on_new_stream(lsquic_stream_t * stream)
{
  fmt::print("LsquicClient::on_new_stream\n");
  _stream = stream;
  // lsquic_engine_process_conns(_engine);
  
  lsquic_stream_wantwrite(_stream, 1);
  lsquic_stream_wantread(_stream, 1);
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

  // lsquic_engine_process_conns(_engine);
  
  char buf[] = "Hello";
  lsquic_stream_write(stream, buf, sizeof(buf));
  
  lsquic_stream_wantwrite(stream, 0);
  lsquic_stream_wantread(stream, 1);
  
  // lsquic_stream_shutdown(stream, 0);
}

void LsquicClient::on_stream_close(lsquic_stream_t * stream)
{
  fmt::print("LsquicClient::on_stream_close\n");
}

int LsquicClient::send_packets_out(const struct lsquic_out_spec *specs, unsigned n_specs)
{
  fmt::print("LsquicClient::send_packets_out\n");
  
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
