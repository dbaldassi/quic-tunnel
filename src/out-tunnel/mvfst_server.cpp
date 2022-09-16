#include "mvfst_server.h"

#include <quic/congestion_control/ServerCongestionControllerFactory.h>

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
  
  return transport;
}

// QuicServer /////////////////////////////////////////////////////////////////

MvfstServer::MvfstServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket)
  : _host(host), _port(port), _server(quic::QuicServer::createQuicServer()), _udp_socket(udp_socket)
{
  _handler = std::make_unique<CallbackHandler>();
  _handler->set_udp_socket(_udp_socket);
  _udp_socket->set_callback(_handler.get());
  
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
  settings.defaultCongestionController = _cc;

  _server->setCongestionControllerFactory(std::make_shared<quic::ServerCongestionControllerFactory>());
  _server->setTransportSettings(std::move(settings));
}

void MvfstServer::set_qlog_filename(std::string file_name)
{
  _qlog_file = std::move(file_name);
}

void MvfstServer::start()
{
  folly::SocketAddress addr(_host.c_str(), _port);
  addr.setFromHostPort(_host, _port);
  _server->start(addr, 0);
  _udp_socket->start();
  LOG(INFO) << "Quic out tunnel started at: " << addr.describe();
  _evb.loopForever();
  LOG(INFO) << "Quic Server stopped ";
}

void MvfstServer::stop()
{
  LOG(INFO) << "Stopping quic server";
  _udp_socket->close();
  _server->shutdown();
  _evb.terminateLoopSoon();
}

bool MvfstServer::set_datagrams(bool enable)
{
  _handler->datagrams = enable;
  return true;
}

bool MvfstServer::set_cc(std::string_view cc) noexcept
{
  if(cc == "newreno")    _cc = quic::CongestionControlType::NewReno;
  else if(cc == "cubic") _cc = quic::CongestionControlType::Cubic;
  else if(cc == "copa")  _cc = quic::CongestionControlType::Copa;
  else if(cc == "bbr")   _cc = quic::CongestionControlType::BBR;
  else                   _cc = quic::CongestionControlType::None;

  return true;
}
