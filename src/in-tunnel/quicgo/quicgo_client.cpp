#include "quicgo_client.h"
#include "go_impl/libgo_client.h"

#include "fmt/core.h"

Capabilities QuicGoClient::get_capabilities()
{
  Capabilities cap;

  cap.cc.emplace_back("newreno");
  cap.datagrams = true;
  cap.impl = IMPL_NAME;
  
  return cap;
}

QuicGoClient::QuicGoClient(std::string host, int port) noexcept
  : _host(std::move(host)), _port(port), _datagrams(true)
{
  _qlog_dir = QuicClient::DEFAULT_QLOG_PATH;
}

void QuicGoClient::set_qlog_filename(std::string filename) noexcept
{
  _qlog_filename = std::move(filename);
}

std::string_view QuicGoClient::get_qlog_path() const noexcept
{
  return _qlog_dir;
}

std::string_view QuicGoClient::get_qlog_filename() const noexcept
{
  return _qlog_filename;
}

bool QuicGoClient::set_cc(std::string_view cc) noexcept
{
  return true;
}

void QuicGoClient::start()
{
  std::string addr = _host + ":" + std::to_string(_port);
  goClientStart(const_cast<char*>(addr.c_str()),
		_datagrams,
		const_cast<char*>(_qlog_dir.c_str()),
		(void **)this);
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
