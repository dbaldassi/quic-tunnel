#include "quic_server.h"
#include "mvfst/mvfst_server.h"
#include "quicgo/quicgo_server.h"
#include "lsquic/lsquic_server.h"
#include "quiche/quiche_server.h"
#include "msquic/msquic_server.h"
#include "tcp/tcp_server.h"
#include "udp/udp_server.h"

#include <fmt/core.h>

QuicServerBuilder::QuicServerBuilder() noexcept
  : host("0.0.0.0"),
    port(8888),
    impl(QuicImplementation::MVFST),
    udp_socket(nullptr)
{}

std::unique_ptr<QuicServer> QuicServerBuilder::create() const
{
  if(!udp_socket)
    fmt::print("error: Can't create quic server, no udp socket has been given");

  switch (impl) {
  case QuicImplementation::MVFST:
    return std::make_unique<MvfstServer>(host, port, udp_socket);
  case QuicImplementation::QUICGO:
    return std::make_unique<QuicGoServer>(host, port, udp_socket);
  case QuicImplementation::LSQUIC:
    return std::make_unique<LsquicServer>(host, port, udp_socket);
  case QuicImplementation::QUICHE:
    return std::make_unique<QuicheServer>(host, port, udp_socket);
  case QuicImplementation::MSQUIC:
    return std::make_unique<MsquicServer>(host, port, udp_socket);
  case QuicImplementation::TCP:
    return std::make_unique<TcpServer>(dst_host, dst_port, port, udp_socket);
  case QuicImplementation::UDP:
    return std::make_unique<UdpServer>(port, udp_socket);
  }

  return nullptr;
}

void QuicServerBuilder::get_capabilities(std::vector<Capabilities>& cap)
{
  cap.push_back(MvfstServer::get_capabilities());
  cap.push_back(QuicGoServer::get_capabilities());
  // cap.push_back(LsquicServer::get_capabilities());
  cap.push_back(QuicheServer::get_capabilities());
  cap.push_back(MsquicServer::get_capabilities());
  // cap.push_back(TcpServer::get_capabilities());
  cap.push_back(UdpServer::get_capabilities());
}
