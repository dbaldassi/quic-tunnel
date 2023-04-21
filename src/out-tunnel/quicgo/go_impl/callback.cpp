
#include "callback.h"

#include <string>

#define GO_SERVER_CALLBACK
#include "../quicgo_server.h"

void on_message_received(wrapper_t * wrapper, const char* buf, int len)
{
  auto server = reinterpret_cast<QuicGoServer*>(wrapper);
  
  server->on_received(buf, static_cast<uint64_t>(len));
}

void on_qlog_filename(wrapper_t * wrapper, const char* buf, int len)
{
  auto server = reinterpret_cast<QuicGoServer*>(wrapper);
  std::string name(buf, len);
  
  server->set_qlog_filename(std::move(name));
}
