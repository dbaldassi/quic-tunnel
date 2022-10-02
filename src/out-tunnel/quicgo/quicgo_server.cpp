#include "quicgo_server.h"
#include "go_impl/libgo_server.h"
#include <cassert>
// #include <fmt/core.h>

QuicGoServer::QuicGoServer(std::string host, uint16_t port, out::UdpSocket * udp_socket) noexcept
  : _datagrams(true), _host(std::move(host)), _port(port), _udp_socket(udp_socket)
{
  assert(udp_socket != nullptr);
  udp_socket->set_callback(this);
}

void QuicGoServer::start()
{
  _udp_socket->start();

  std::string addr = _host + ":" + std::to_string(_port);
  goServerStart(const_cast<char*>(addr.c_str()), _datagrams, (void**)this);
}

void QuicGoServer::stop()
{
  goServerStop();
}

void QuicGoServer::set_qlog_filename(std::string file_name)
{

}

std::string_view QuicGoServer::get_qlog_path() const noexcept
{

}

std::string_view QuicGoServer::get_qlog_filename() const noexcept
{

}

bool QuicGoServer::set_datagrams(bool enable)
{
  _datagrams = enable;
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
