#include <iostream>
#include <random>
#include <sstream>

#include <fmt/core.h>

#include "outtunnel.h"
#include "quic_server.h"

std::unordered_map<int, std::shared_ptr<OutTunnel>> OutTunnel::sessions;

// Random Number generation ///////////////////////////////////////////////////

static RandomGenerator random_sequence()
{
  std::random_device r;

  std::default_random_engine dre(r());
  std::uniform_int_distribution<int> uniform_dist(0, OutTunnel::MAX_NUMBER_SESSION - 1);

  for(;;) {
    if(OutTunnel::sessions.size() == OutTunnel::MAX_NUMBER_SESSION)
      co_yield -1;
    
    int id = uniform_dist(dre);
    if(OutTunnel::sessions.contains(id))
      co_yield id;
  }
}

RandomGenerator OutTunnel::_random_generator = random_sequence();

// Mvfst //////////////////////////////////////////////////////////////////////

OutTunnel::OutTunnel(int id,
		     std::string_view server_addr,
		     uint16_t server_port,
		     uint16_t out_port)
  : _id{id},
    _out_port{out_port},
    _udp_socket(server_addr.data(), out_port),
    _quic_server(nullptr)
{
  QuicServerBuilder builder;
  builder.impl = QuicServerBuilder::QuicImplementation::QUICGO;
  builder.host = "0.0.0.0";
  builder.port = 8888;
  builder.udp_socket = &_udp_socket;

  _quic_server = builder.create();
}

OutTunnel::~OutTunnel() noexcept
{}

void OutTunnel::run()
{    
  _quic_server->start();
}

void OutTunnel::stop()
{
  _quic_server->stop();
}

std::string OutTunnel::get_qlog_file()
{
  std::ostringstream oss;
  oss << _quic_server->get_qlog_path() << "/" << _quic_server->get_qlog_filename();

  return oss.str();
}

void OutTunnel::set_cc(std::string_view cc)
{
  fmt::print("Set {} congestion controller\n", cc);
  
  _quic_server->set_cc(cc);
}

void OutTunnel::set_datagrams(bool enable)
{
  fmt::print("Set datagrams : {}", enable);
  _quic_server->set_datagrams(enable);
}

std::shared_ptr<OutTunnel> OutTunnel::create(std::string_view server_addr,
					     uint16_t server_port,
					     uint16_t out_port)
{
  auto id     = _random_generator();
  if(id == -1) return nullptr;
  
  auto server = std::make_shared<OutTunnel>(id, server_addr, server_port, out_port);

  sessions[id] = server;
  
  return server;
}
