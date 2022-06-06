#include <iostream>
#include <random>
#include <sstream>

#include "intunnel.h"

#include "quic_client.h"

#include "fmt/core.h"

#include <glog/logging.h>

std::unordered_map<int, std::shared_ptr<MvfstInClient>> MvfstInClient::sessions;

// Random Number generation ///////////////////////////////////////////////////

RandomGenerator random_sequence()
{
  std::random_device r;

  std::default_random_engine dre(r());
  std::uniform_int_distribution<int> uniform_dist(0, MvfstInClient::MAX_NUMBER_SESSION - 1);

  for(;;) {
    if(MvfstInClient::sessions.size() == MvfstInClient::MAX_NUMBER_SESSION)
      co_yield -1;
    
    int id = uniform_dist(dre);
    if(MvfstInClient::sessions.contains(id))
      co_yield id;
  }
}

RandomGenerator MvfstInClient::_random_generator = random_sequence();

// Mvfstclient ////////////////////////////////////////////////////////////////

MvfstInClient::MvfstInClient(int id, std::string_view server_addr, uint16_t server_port)
  : _id(id), _quic_client(new QuicClient(server_addr, server_port)), _datagrams(true)
{}

MvfstInClient::~MvfstInClient() noexcept
{}

void MvfstInClient::run()
{  
  _udp_socket.open(_in_port);
  
  _quic_client->set_on_received_callback([this](const char* msg, size_t length) {
    _udp_socket.send_back(msg, length);
  });
  
  _quic_client->start();
  
  while(true) {
    auto len = _udp_socket.recv();
    if(len == -1) return;

    const char * buffer = _udp_socket.get_buffer();
    
    if(_datagrams) _quic_client->send_message_datagram(buffer, len);
    else _quic_client->send_message_stream(buffer, len);
  }
}

void MvfstInClient::set_cc(std::string_view cc)
{
  fmt::print("Set {} congestion controller\n", cc);
  
  if(cc == "newreno")    _quic_client->set_cc(quic::CongestionControlType::NewReno);
  else if(cc == "cubic") _quic_client->set_cc(quic::CongestionControlType::Cubic);
  else if(cc == "copa")  _quic_client->set_cc(quic::CongestionControlType::Copa);
  else if(cc == "bbr")   _quic_client->set_cc(quic::CongestionControlType::BBR);
  else                   _quic_client->set_cc(quic::CongestionControlType::None);
}

void MvfstInClient::set_datagram(bool enable)
{
  _datagrams = enable;
}

int MvfstInClient::allocate_in_port()
{
  _in_port = 3479;
  return _in_port = 3479;
}
  
void MvfstInClient::stop()
{
  _udp_socket.close();
  _quic_client->stop();
}

std::string MvfstInClient::get_qlog_file()
{
  std::ostringstream oss;
  oss << _quic_client->get_qlog_path() << "/" << _quic_client->get_qlog_filename();

  return oss.str();
}

std::shared_ptr<MvfstInClient> MvfstInClient::create(std::string_view server_addr,
						     uint16_t server_port)
{
  auto id     = _random_generator();
  if(id == -1) return nullptr;
  
  auto client = std::make_shared<MvfstInClient>(id, server_addr, server_port);

  sessions[id] = client;
  
  return client;
}
