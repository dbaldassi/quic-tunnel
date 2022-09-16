#include "quic_client.h"
#include "mvfst_client.h"

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
  }

  return nullptr;
}
