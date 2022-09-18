#include "quic_client.h"
#include "mvfst/mvfst_client.h"
#include "quicgo/quicgo_client.h"

QuicClientBuilder::QuicClientBuilder() noexcept
  : host("0.0.0.0"), // Default values
    port(8888),
    impl(QuicImplementation::MVFST)
{}

std::unique_ptr<QuicClient> QuicClientBuilder::create() const
{
  switch (impl) {
  case QuicImplementation::MVFST:
    return std::make_unique<MvfstClient>(host, port);
  case QuicImplementation::QUICGO:
    return std::make_unique<QuicGoClient>();
  }

  return nullptr;
}
