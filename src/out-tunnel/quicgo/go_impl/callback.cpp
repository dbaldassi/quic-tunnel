
#include "callback.h"

#define GO_SERVER_CALLBACK
#include "../quicgo_server.h"

void callback(wrapper_t * wrapper, const char* buf, int len)
{
  auto server = reinterpret_cast<QuicGoServer*>(wrapper);
  
  server->on_received(buf, static_cast<uint64_t>(len));
}
