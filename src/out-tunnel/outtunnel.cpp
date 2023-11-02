#include <iostream>
#include <random>
#include <sstream>
#include <csignal>

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
		     std::string_view impl,
		     std::string_view server_addr,
		     uint16_t server_port,
		     uint16_t out_port)
  : _id{id},
    _out_port{out_port},
    _udp_socket(server_addr.data(), out_port),
    _quic_server(nullptr),
    _external_file_transfer(false)
{
  using namespace std::string_view_literals;
  
  QuicServerBuilder builder;
  builder.host = "0.0.0.0";
  builder.port = server_port;
  builder.dst_host = server_addr;
  builder.dst_port = out_port;
  builder.udp_socket = &_udp_socket;

  if(impl == "quicgo"sv)      builder.impl = QuicServerBuilder::QuicImplementation::QUICGO;
  else if(impl == "mvfst"sv)  builder.impl = QuicServerBuilder::QuicImplementation::MVFST;
  else if(impl == "lsquic"sv) builder.impl = QuicServerBuilder::QuicImplementation::LSQUIC;
  else if(impl == "tcp"sv)    builder.impl = QuicServerBuilder::QuicImplementation::TCP;
  else if(impl == "udp"sv)    builder.impl = QuicServerBuilder::QuicImplementation::UDP;

  _quic_server = builder.create();
}

OutTunnel::~OutTunnel() noexcept
{}

int run_tcpdump(const char* if_env_name, const char* file_name)
{
  // first get the interface name
  if(const char* interface = std::getenv(if_env_name)) {
    // Fork process
    pid_t pid = fork();
    // return if parent process
    if(pid != 0) return pid;
    
    fmt::print("Starting tcpdump for quic output\n");

    int fd[2]; // pipe file descriptor. 0: rx 1: tx

    // open pipe
    if(pipe(fd) == -1) {
      fmt::print(stderr, "Pipe failed\n");
      return -1;
    }
    
    // fork the child process to pipe tcpdump output into tcpdumpbitrate.py
    pid_t child_pid = fork();
    if(child_pid == 0) {
      // close reading end of the pipe
      close(fd[0]);
      // Redirect standard output into writing end of the pipe
      dup2(fd[1], STDOUT_FILENO);
      // Close the pipe
      close(fd[1]);
      
      // run tcpdump
      execl("/usr/bin/tcpdump", "/usr/bin/tcpdump",
	    "-i", interface, "-l", "-e", "-n", NULL);
    }
    else {
      // Close writing end of the pipe
      close(fd[1]);
      // Redirect reading end of the pipe into stdin
      dup2(fd[0], STDIN_FILENO);
      // close pipe
      close(fd[0]);

      // run tcpdumpbitrate.py script
      execl("/usr/local/bin/tcpdumpbitrate.py", "/usr/local/bin/tcpdumpbitrate.py", file_name, NULL);
    }
  }
  
  return -1;
}

void OutTunnel::run()
{
  int quic_pid = run_tcpdump("IFQUIC", "quic.csv");
  if(quic_pid == 0) return;

  int file_pid = -1;
  if(_external_file_transfer) {
    file_pid = run_tcpdump("IFSCP", "file.csv");
    if(file_pid == 0) return;
  }
  
  _quic_server->start();

  if(quic_pid > 0) kill(quic_pid, SIGTERM);
  if(file_pid > 0) kill(file_pid, SIGTERM);
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
  fmt::print("Set datagrams : {}\n", enable);
  _quic_server->set_datagrams(enable);
}

std::shared_ptr<OutTunnel> OutTunnel::create(std::string_view impl,
					     std::string_view server_addr,
					     uint16_t server_port,
					     uint16_t out_port)
{
  auto id     = _random_generator();
  if(id == -1) return nullptr;
  
  auto server = std::make_shared<OutTunnel>(id, impl, server_addr, server_port, out_port);

  sessions[id] = server;
  
  return server;
}

void OutTunnel::get_capabilities(std::vector<Capabilities>& impls)
{
  QuicServerBuilder::get_capabilities(impls);
}
