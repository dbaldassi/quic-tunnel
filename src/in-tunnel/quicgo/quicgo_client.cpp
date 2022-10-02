#include "quicgo_client.h"
#include "go_impl/libgo_client.h"

QuicGoClient::QuicGoClient(std::string host, int port) noexcept
  : _host(std::move(host)), _port(port)
{
}

std::string_view QuicGoClient::get_qlog_path() const noexcept
{

}

std::string_view QuicGoClient::get_qlog_filename() const noexcept
{

}

bool QuicGoClient::set_cc(std::string_view cc) noexcept
{
  return true;
}

void QuicGoClient::start()
{
  std::string addr = _host + ":" + std::to_string(_port);
  goClientStart(const_cast<char*>(addr.c_str()), true, (void **)this);
}

void QuicGoClient::stop()
{
  goClientStop();
}

void QuicGoClient::send_message_stream(const char * buffer, size_t len)
{
  goClientSendMessageStream(const_cast<char*>(buffer), len);
}

void QuicGoClient::send_message_datagram(const char * buffer, size_t len)
{
  // hmm const_cast ...
  // TODO: find a way to pass a const char* to go
  goClientSendMessageDatagram(const_cast<char*>(buffer), len);
}

void QuicGoClient::on_received(const char* buf, uint64_t len)
{
  if(_on_received_callback) _on_received_callback(buf, len);
}
