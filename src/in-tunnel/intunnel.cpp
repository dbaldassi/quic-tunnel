#include <iostream>
#include <random>
#include <sstream>

#include <unistd.h>
#include <csignal>

#include "intunnel.h"

#include "quic_client.h"

#include "fmt/core.h"

#include <glog/logging.h>

std::unordered_map<int, std::shared_ptr<InTunnel>> InTunnel::sessions;

// Random Number generation ///////////////////////////////////////////////////

static RandomGenerator random_sequence()
{
  std::random_device r;

  std::default_random_engine dre(r());
  std::uniform_int_distribution<int> uniform_dist(0, InTunnel::MAX_NUMBER_SESSION - 1);

  for(;;) {
    if(InTunnel::sessions.size() == InTunnel::MAX_NUMBER_SESSION)
      co_yield -1;
    
    int id = uniform_dist(dre);
    if(InTunnel::sessions.contains(id))
      co_yield id;
  }
}

RandomGenerator InTunnel::_random_generator = random_sequence();

// Mvfstclient ////////////////////////////////////////////////////////////////

InTunnel::InTunnel(int id, std::string_view server_addr, uint16_t server_port)
  : _id(id), _quic_client(nullptr), _datagrams(true),
    _external_file_transfer(false), _multiplexed_file_transfer(false)
{
  QuicClientBuilder builder;
  builder.host = server_addr;
  builder.port = server_port;
  builder.impl = QuicClientBuilder::QuicImplementation::QUICGO;
  
  _quic_client = builder.create();
}

InTunnel::~InTunnel() noexcept
{}

void InTunnel::run()
{
  int pid = -1;

  if(_external_file_transfer) {
    if(const char * url = std::getenv("SCP_FILE_URL")) {
      _scp_file_url = url;
      fmt::print("scp file transfer set to : {}\n", _scp_file_url);
    }
    else {
      fmt::print("Can not make scp file transfer, you must set the SCP_FILE_URL environment variable first\n");
      _external_file_transfer = false;
    }
  }
  
  if(_external_file_transfer) pid = fork();

  if(pid == 0) {
    // Start scp file transfer
    fmt::print("Starting scp file transfer to : {}\n", _scp_file_url);
    execl("/usr/bin/scp", "/usr/bin/scp", _scp_file_url.c_str(), ".", NULL);
  }
  else {
    _udp_socket.open(_in_port);
  
    _quic_client->set_on_received_callback([this](const char* msg, size_t length) {
					     _udp_socket.send_back(msg, length);
					   });
  
    _quic_client->start();
  
    while(true) {
      auto len = _udp_socket.recv();
      if(len == -1) break;

      const char * buffer = _udp_socket.get_buffer();
    
      if(_datagrams) _quic_client->send_message_datagram(buffer, len);
      else _quic_client->send_message_stream(buffer, len);
    }

    if(pid != -1) kill(pid, SIGTERM);
  }
}

bool InTunnel::set_cc(std::string_view cc)
{
  fmt::print("Set {} congestion controller\n", cc);
  
  return _quic_client->set_cc(cc);
}

void InTunnel::set_datagram(bool enable)
{
  fmt::print("Set datagrams : {}\n", enable);
  _datagrams = enable;
}

int InTunnel::allocate_in_port()
{
  _in_port = 3479;
  fmt::print("Got port : {}\n", _in_port);
  return _in_port = 3479;
}
  
void InTunnel::stop()
{
  _udp_socket.close();
  _quic_client->stop();
}

std::string InTunnel::get_qlog_file()
{
  std::ostringstream oss;
  oss << _quic_client->get_qlog_path() << "/" << _quic_client->get_qlog_filename();

  return oss.str();
}

void InTunnel::enable_external_file_transfer()
{
  fmt::print("enable scp file transfer");
  _external_file_transfer = true;
}

void InTunnel::enable_multiplexed_file_transfer()
{
  _multiplexed_file_transfer = true;
}

std::shared_ptr<InTunnel> InTunnel::create(std::string_view server_addr,
						     uint16_t server_port)
{
  fmt::print("Create new mvfst client \n");
  
  auto id     = _random_generator();
  if(id == -1) return nullptr;
  
  auto client = std::make_shared<InTunnel>(id, server_addr, server_port);

  sessions[id] = client;
  
  return client;
}
