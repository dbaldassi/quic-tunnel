#include "mvfst_server.h"

#include <quic/fizz/handshake/QuicFizzFactory.h>
#include <quic/samples/echo/EchoHandler.h>
#include <quic/samples/echo/LogQuicStats.h>
#include <quic/server/QuicServer.h>
#include <quic/server/QuicServerTransport.h>
#include <quic/server/QuicSharedUDPSocketFactory.h>
#include <quic/congestion_control/ServerCongestionControllerFactory.h>

#include "callback_handler.h"
#include "qlogfile.h"

#include <glog/logging.h>

#include <folly/String.h>
#include <folly/ssl/OpenSSLCertUtils.h>

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

class MvfstTransportFactory : public quic::QuicServerTransportFactory
{  
  CallbackHandler * _handler;
  MvfstServer     * _server;
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

// Linking issue if we don't define these, somehow the symbol are not the mvfst lib
constexpr folly::StringPiece fizz::Sha256::BlankHash;
constexpr folly::StringPiece fizz::Sha384::BlankHash;

// Crypto utils ///////////////////////////////////////////////////////////////

folly::ssl::EvpPkeyUniquePtr get_private_key(folly::StringPiece key)
{
  folly::ssl::BioUniquePtr bio(BIO_new(BIO_s_mem()));
  
  CHECK(bio);
  CHECK_EQ(BIO_write(bio.get(), key.data(), key.size()), key.size());
  
  folly::ssl::EvpPkeyUniquePtr pkey(
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
  
  CHECK(pkey);
  return pkey;
}
  
folly::ssl::X509UniquePtr get_cert(folly::StringPiece cert)
{
  folly::ssl::BioUniquePtr bio(BIO_new(BIO_s_mem()));
  CHECK(bio);
  CHECK_EQ(BIO_write(bio.get(), cert.data(), cert.size()), cert.size());
  folly::ssl::X509UniquePtr x509(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
  CHECK(x509);
  return x509;
}

std::shared_ptr<fizz::SelfCert> read_cert()
{
  auto certificate = get_cert(kP256Certificate);
  auto pkey = get_private_key(kP256Key);
  std::vector<folly::ssl::X509UniquePtr> certs;
  certs.emplace_back(std::move(certificate));
  return std::make_shared<fizz::SelfCertImpl<fizz::KeyType::P256>>(
	   std::move(pkey), std::move(certs));
}

std::shared_ptr<fizz::server::FizzServerContext> create_server_ctx()
{
  auto cert = read_cert();
  auto cert_manager = std::make_unique<fizz::server::CertManager>();
  
  cert_manager->addCert(std::move(cert), true);
  
  auto ctx = std::make_shared<fizz::server::FizzServerContext>();
  ctx->setFactory(std::make_shared<quic::QuicFizzFactory>());
  ctx->setCertManager(std::move(cert_manager));
  ctx->setOmitEarlyRecordLayer(true);
  ctx->setClock(std::make_shared<fizz::SystemClock>());
  ctx->setSupportedAlpns({ "quic-echo-example" });
  ctx->setAlpnMode(fizz::server::AlpnMode::Required);
  
  return ctx;
}

// QuicTransportFactory ///////////////////////////////////////////////////////

MvfstTransportFactory::~MvfstTransportFactory()
{}

quic::QuicServerTransport::Ptr
MvfstTransportFactory::make(folly::EventBase* evb,
				    std::unique_ptr<folly::AsyncUDPSocket> sock,
				    const folly::SocketAddress&,
				    quic::QuicVersion ver,
				    FizzServerCtxPtr ctx) noexcept
{
  LOG(INFO) << "Make new QUIC transport ver : " << ver;
  
  CHECK_EQ(evb, sock->getEventBase());

  _handler->set_evb(evb);
  
  auto transport = quic::QuicServerTransport::make(evb,
						   std::move(sock),
						   _handler,
						   _handler,
						   ctx);

  _handler->set_quic_socket(transport);
  
  transport->setDatagramCallback(_handler);
  transport->setQLogger(QLog<quic::VantagePoint::Server>::create(QuicServer::DEFAULT_QLOG_PATH));

  // std::vector<quic::QuicVersion> v = { quic::QuicVersion::QUIC_DRAFT, ver };
  // transport->setSupportedVersions(v);
  
  return transport;
}

// QuicServer /////////////////////////////////////////////////////////////////

Capabilities MvfstServer::get_capabilities()
{
  Capabilities cap;
  cap.impl = IMPL_NAME;
  
  cap.cc.emplace_back("newreno");
  cap.cc.emplace_back("bbr");
  cap.cc.emplace_back("copa");
  cap.cc.emplace_back("cubic");
  cap.cc.emplace_back("none");

  cap.datagrams = true;

  return cap;
}

MvfstServer::MvfstServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket)
  : _host(host), _port(port), _server(nullptr), _udp_socket(udp_socket), _datagrams(false)
{}

void MvfstServer::set_qlog_filename(std::string file_name)
{
  _qlog_file = std::move(file_name);
}

void MvfstServer::start()
{
  folly::SocketAddress addr(_host.c_str(), _port);
  addr.setFromHostPort(_host, _port);

  _handler = std::make_unique<CallbackHandler>();
  _handler->set_udp_socket(_udp_socket);
  _handler->datagrams = _datagrams;

  _udp_socket->set_callback(_handler.get());

  _evb = std::make_unique<folly::EventBase>();

  _server = quic::QuicServer::createQuicServer();
  _server->setQuicServerTransportFactory(std::make_unique<MvfstTransportFactory>(_handler.get(),
											 this));
  _server->setTransportStatsCallbackFactory(std::make_unique<quic::samples::LogQuicStatsFactory>());

  auto ctx = create_server_ctx();
  _server->setFizzContext(ctx);
  
  auto settings = _server->getTransportSettings();
  settings.datagramConfig.enabled      = true;
  settings.datagramConfig.readBufSize  = 2048;
  settings.datagramConfig.writeBufSize = 2048;
  settings.maxRecvPacketSize = 4096;
  settings.canIgnorePathMTU = true;
  settings.defaultCongestionController = static_cast<quic::CongestionControlType>(_cc);

  _server->setCongestionControllerFactory(std::make_shared<quic::ServerCongestionControllerFactory>());
  _server->setTransportSettings(std::move(settings));  
  _server->start(addr, 0);
  
  LOG(INFO) << "Quic out tunnel started at: " << addr.describe();
  _evb->loopForever();
  LOG(INFO) << "Quic Server stopped ";
}

void MvfstServer::stop()
{
  LOG(INFO) << "Stopping quic server";
  _udp_socket->close();
  _server->shutdown();
  _evb->terminateLoopSoon();
}

bool MvfstServer::set_datagrams(bool enable)
{
  _datagrams = enable;
  return true;
}

bool MvfstServer::set_cc(std::string_view cc) noexcept
{
  if(cc == "newreno")    _cc = static_cast<uint8_t>(quic::CongestionControlType::NewReno);
  else if(cc == "cubic") _cc = static_cast<uint8_t>(quic::CongestionControlType::Cubic);
  else if(cc == "copa")  _cc = static_cast<uint8_t>(quic::CongestionControlType::Copa);
  else if(cc == "bbr")   _cc = static_cast<uint8_t>(quic::CongestionControlType::BBR);
  else                   _cc = static_cast<uint8_t>(quic::CongestionControlType::None);

  return true;
}

std::string_view MvfstServer::get_qlog_path() const noexcept
{
  return DEFAULT_QLOG_PATH;
}

std::string_view MvfstServer::get_qlog_filename() const noexcept
{
  return _handler->qlog_file;
}
