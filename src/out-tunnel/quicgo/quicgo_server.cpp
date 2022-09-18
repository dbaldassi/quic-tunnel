#include "quicgo_server.h"
#include "go_impl/libgo_server.h"

void QuicGoServer::start()
{
  goServerStart();
}

void QuicGoServer::stop()
{

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

}

bool QuicGoServer::set_cc(std::string_view cc) noexcept
{

}
