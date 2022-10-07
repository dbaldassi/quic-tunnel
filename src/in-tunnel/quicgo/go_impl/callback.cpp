
#include "callback.h"

#define GO_CLIENT_CALLBACK
#include "../quicgo_client.h"

void on_message_received(wrapper_t * wrapper, const char* buf, int len)
{
  auto client = reinterpret_cast<QuicGoClient*>(wrapper);
  
  client->on_received(buf, static_cast<uint64_t>(len));
}

void on_qlog_filename(wrapper_t * wrapper, const char* buf, int len)
{
  auto client = reinterpret_cast<QuicGoClient*>(wrapper);
  std::string name(buf, len);
  
  client->set_qlog_filename(std::move(name));
}
