
#include "mvfst_client.h"
#include "mvfst_client_impl.h"

MvfstClient::MvfstClient(std::string_view host, uint16_t port, std::string qlog_path) noexcept
{
  _impl = std::make_shared<MvfstClientImpl>(host, port, qlog_path);
}

Capabilities MvfstClient::get_capabilities()
{
  return MvfstClientImpl::get_capabilities();
}
  
void MvfstClient::set_qlog_filename(std::string name) noexcept
{
  _impl->set_qlog_filename(std::move(name));
};

std::string_view MvfstClient::get_qlog_path() const noexcept
{
  return _impl->get_qlog_path();
}

std::string_view MvfstClient::get_qlog_filename() const noexcept
{
  return _impl->get_qlog_filename();
}

/* QuicClient overrides */
bool MvfstClient::set_cc(std::string_view cc) noexcept
{
  return _impl->set_cc(cc);
}

void MvfstClient::start()
{
  _impl->set_on_received_callback(_on_received_callback);
  _impl->start();
}

void MvfstClient::stop()
{
  _impl->stop();
}

void MvfstClient::send_message_stream(const char * buffer, size_t len)
{
  _impl->send_message_stream(buffer, len);
}

void MvfstClient::send_message_datagram(const char * buffer, size_t len)
{
  _impl->send_message_datagram(buffer, len);
}

