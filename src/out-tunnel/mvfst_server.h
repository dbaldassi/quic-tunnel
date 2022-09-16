#ifndef MVFST_SERVER_H
#define MVFST_SERVER_H

#include "callback_handler.h"

#include <glog/logging.h>

#include <folly/String.h>
#include <folly/ssl/OpenSSLCertUtils.h>

#include <quic/fizz/handshake/QuicFizzFactory.h>
#include <quic/samples/echo/EchoHandler.h>
#include <quic/samples/echo/LogQuicStats.h>
#include <quic/server/QuicServer.h>
#include <quic/server/QuicServerTransport.h>
#include <quic/server/QuicSharedUDPSocketFactory.h>

#include "quic_server.h"
#include "qlogfile.h"

constexpr folly::StringPiece kP256Key = R"(
-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIHMPeLV/nP/gkcgU2weiXl198mEX8RbFjPRoXuGcpxMXoAoGCCqGSM49
AwEHoUQDQgAEnYe8rdtl2Nz234sUipZ5tbcQ2xnJWput//E0aMs1i04h0kpcgmES
ZY67ltZOKYXftBwZSDNDkaSqgbZ4N+Lb8A==
-----END EC PRIVATE KEY-----
)";

constexpr folly::StringPiece kP256Certificate = R"(
-----BEGIN CERTIFICATE-----
MIIB7jCCAZWgAwIBAgIJAMVp7skBzobZMAoGCCqGSM49BAMCMFQxCzAJBgNVBAYT
AlVTMQswCQYDVQQIDAJOWTELMAkGA1UEBwwCTlkxDTALBgNVBAoMBEZpenoxDTAL
BgNVBAsMBEZpenoxDTALBgNVBAMMBEZpenowHhcNMTcwNDA0MTgyOTA5WhcNNDEx
MTI0MTgyOTA5WjBUMQswCQYDVQQGEwJVUzELMAkGA1UECAwCTlkxCzAJBgNVBAcM
Ak5ZMQ0wCwYDVQQKDARGaXp6MQ0wCwYDVQQLDARGaXp6MQ0wCwYDVQQDDARGaXp6
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEnYe8rdtl2Nz234sUipZ5tbcQ2xnJ
Wput//E0aMs1i04h0kpcgmESZY67ltZOKYXftBwZSDNDkaSqgbZ4N+Lb8KNQME4w
HQYDVR0OBBYEFDxbi6lU2XUvrzyK1tGmJEncyqhQMB8GA1UdIwQYMBaAFDxbi6lU
2XUvrzyK1tGmJEncyqhQMAwGA1UdEwQFMAMBAf8wCgYIKoZIzj0EAwIDRwAwRAIg
NJt9NNcTL7J1ZXbgv6NsvhcjM3p6b175yNO/GqfvpKUCICXFCpHgqkJy8fUsPVWD
p9fO4UsXiDUnOgvYFDA+YtcU
-----END CERTIFICATE-----
)";

class MvfstServer;

class MvfstTransportFactory : public quic::QuicServerTransportFactory
{  
  CallbackHandler * _handler;
  MvfstServer      * _server;
public:  
  using FizzServerCtxPtr = std::shared_ptr<const fizz::server::FizzServerContext>;
  
  explicit MvfstTransportFactory(CallbackHandler * hdl, MvfstServer * server) noexcept
    : _handler(hdl), _server(server) {}
  ~MvfstTransportFactory();

  quic::QuicServerTransport::Ptr make(folly::EventBase* evb,
				      std::unique_ptr<folly::AsyncUDPSocket> sock,
				      const folly::SocketAddress&,
				      quic::QuicVersion,
				      FizzServerCtxPtr ctx) noexcept override;  
};

class MvfstServer : public QuicServer
{
  std::string      _host;
  uint16_t         _port;
  folly::EventBase _evb;

  std::shared_ptr<quic::QuicServer> _server;
  std::unique_ptr<CallbackHandler>  _handler;
  
  out::UdpSocket * _udp_socket;

  quic::CongestionControlType _cc;

  std::string _qlog_file;
  bool        _datagrams;
  
public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-out-logs";
  
  explicit MvfstServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket);

  void set_qlog_filename(std::string file_name) override;
  
  void start() override;

  std::string_view get_qlog_path() const noexcept override { return DEFAULT_QLOG_PATH; }
  std::string_view get_qlog_filename() const noexcept override { return _handler->qlog_file; }
  bool set_datagrams(bool enable);
  
  bool set_cc(std::string_view cc) noexcept override;
  void stop() override;
};

#endif /* MVFST_SERVER_H */
