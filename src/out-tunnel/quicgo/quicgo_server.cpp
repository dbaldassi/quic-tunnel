#include "quicgo_server.h"
#include "go_impl/libgo_server.h"
#include <cassert>
// #include <fmt/core.h>

Capabilities QuicGoServer::get_capabilities()
{
  Capabilities cap;
  cap.impl = IMPL_NAME;
  cap.cc.emplace_back("newreno");
  cap.datagrams = true;

  return cap;
}

QuicGoServer::QuicGoServer(std::string host, uint16_t port, out::UdpSocket * udp_socket) noexcept
  : _datagrams(true), _host(std::move(host)), _port(port), _udp_socket(udp_socket)
{
  assert(udp_socket != nullptr);
  _qlog_dir = QuicServer::DEFAULT_QLOG_PATH;
  udp_socket->set_callback(this);
}

void QuicGoServer::start()
{  
  std::string addr = _host + ":" + std::to_string(_port);
  goServerStart(const_cast<char*>(addr.c_str()),
		_datagrams,
		const_cast<char*>(_qlog_dir.c_str()),
		(void**)this);
}

void QuicGoServer::stop()
{
  _udp_socket->close();
  goServerStop();
}

void QuicGoServer::set_qlog_filename(std::string file_name)
{
  _qlog_filename = std::move(file_name);
}

std::string_view QuicGoServer::get_qlog_path() const noexcept
{
  return _qlog_dir;
}

std::string_view QuicGoServer::get_qlog_filename() const noexcept
{
  return _qlog_filename;
}

bool QuicGoServer::set_datagrams(bool enable)
{
  _datagrams = enable;
  return true;
}

bool QuicGoServer::set_cc(std::string_view cc) noexcept
{
  return true;
}

void QuicGoServer::onUdpMessage(const char *buffer, size_t len) noexcept
{
  if(_datagrams) goServerSendMessageDatagram(const_cast<char*>(buffer), len);
  else           goServerSendMessageStream(const_cast<char*>(buffer), len);
}

void QuicGoServer::on_received(const char* buf, uint64_t len)
{
  _udp_socket->send(buf, len);
}
