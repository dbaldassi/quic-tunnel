#include "quic_client.h"
#include "mvfst/mvfst_client.h"
#include "quicgo/quicgo_client.h"
#include "tcp/tcp_client.h"
#include "udp/udp_client.h"

QuicClientBuilder::QuicClientBuilder() noexcept
  : dst_host("0.0.0.0"), // Default values
    dst_port(8888),
    impl(QuicImplementation::MVFST)
{}

std::unique_ptr<QuicClient> QuicClientBuilder::create() const
{
  switch (impl) {
  case QuicImplementation::MVFST:
    return std::make_unique<MvfstClient>(dst_host, dst_port);
  case QuicImplementation::QUICGO:
  return std::make_unique<QuicGoClient>(dst_host, dst_port);
  case QuicImplementation::TCP:
    return std::make_unique<TcpClient>(dst_host, dst_port, src_port);
  case QuicImplementation::UDP:
    return std::make_unique<UdpClient>(dst_host, dst_port, src_port);
  }

  return nullptr;
}

void QuicClientBuilder::get_capabilities(std::vector<Capabilities>& cap)
{
  cap.push_back(MvfstClient::get_capabilities());
  cap.push_back(QuicGoClient::get_capabilities());
  cap.push_back(TcpClient::get_capabilities());
  cap.push_back(UdpClient::get_capabilities());
}
