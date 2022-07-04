#include <iostream>

#include "outtunnel.h"
#include "quic_server.h"

std::unordered_map<int, std::shared_ptr<MvfstOutClient>> MvfstOutClient::sessions;

// Random Number generation ///////////////////////////////////////////////////

static RandomGenerator random_sequence()
{
  std::random_device r;

  std::default_random_engine dre(r());
  std::uniform_int_distribution<int> uniform_dist(0, MvfstOutClient::MAX_NUMBER_SESSION - 1);

  for(;;) {
    if(MvfstOutClient::sessions.size() == MvfstOutClient::MAX_NUMBER_SESSION)
      co_yield -1;
    
    int id = uniform_dist(dre);
    if(MvfstOutClient::sessions.contains(id))
      co_yield id;
  }
}

RandomGenerator MvfstOutClient::_random_generator = random_sequence();

// Mvfst //////////////////////////////////////////////////////////////////////

MvfstOutClient::MvfstOutClient(int id, std::string_view server_addr,
			       uint16_t server_port,
			       uint16_t out_port)
  : _id{id},
    _out_port{out_port},
    _udp_socket(server_addr.data(), out_port),
    _quic_server(std::make_unique<QuicServer>("0.0.0.0", server_port, &_udp_socket))
{}

MvfstOutClient::~MvfstOutClient() noexcept
{}

void MvfstOutClient::run()
{    
  _quic_server->start();
}

void MvfstOutClient::stop()
{
  // TODO
}

void MvfstOutClient::set_cc(std::string_view cc)
{
  fmt::print("Set {} congestion controller\n", cc);
  
  if(cc == "newreno")    _quic_server->set_cc(quic::CongestionControlType::NewReno);
  else if(cc == "cubic") _quic_server->set_cc(quic::CongestionControlType::Cubic);
  else if(cc == "copa")  _quic_server->set_cc(quic::CongestionControlType::Copa);
  else if(cc == "bbr")   _quic_server->set_cc(quic::CongestionControlType::BBR);
  else                   _quic_server->set_cc(quic::CongestionControlType::None);
}

std::shared_ptr<MvfstOutClient> MvfstOutClient::create(std::string_view server_addr,
						       uint16_t server_port,
						       uint16_t out_port)
{
  auto id     = _random_generator();
  if(id == -1) return nullptr;
  
  auto server = std::make_shared<MvfstOutClient>(id, server_addr, server_port, out_port);

  sessions[id] = server;
  
  return server;
}
