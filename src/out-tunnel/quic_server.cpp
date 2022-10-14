#include "quic_server.h"
#include "mvfst/mvfst_server.h"
#include "quicgo/quicgo_server.h"

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
  }

  return nullptr;
}

void QuicServerBuilder::get_capabilities(std::vector<Capabilities>& cap)
{
  cap.push_back(MvfstServer::get_capabilities());
  cap.push_back(QuicGoServer::get_capabilities());
}
