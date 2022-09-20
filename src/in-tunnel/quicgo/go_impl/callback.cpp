
#include "callback.h"

#define GO_CLIENT_CALLBACK
#include "../quicgo_client.h"

void callback(wrapper_t * wrapper, const char* buf, int len)
{
  auto client = reinterpret_cast<QuicGoClient*>(wrapper);
  
  client->on_received(buf, static_cast<uint64_t>(len));
}
