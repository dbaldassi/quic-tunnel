#include <iostream>
#include <optional>

#include <getopt.h>

#include "in-tunnel/intunnel.h"

#include <fmt/core.h>
#include <fmt/color.h>

#include "out-tunnel/outtunnel.h"
#include "controls/websocket_server.h"
#include "controls/link.h"

namespace def
{

constexpr auto UDP_PORT  = 3479;
constexpr auto TURN_PORT = 3478;
constexpr auto QUIC_PORT = 8888;
constexpr auto QUIC_SERVER_HOST = "127.0.0.1";
constexpr auto WEBSOCKET_PORT = 3333;
constexpr const char * TURN_HOSTNAME = "turn.dabaldassi.fr";
constexpr const char * IF_NAME = "eth0";
constexpr const char * QUIC_IMPL = "mvfst";

}

void display_help()
{
  fmt::print("{:*^50}\n", " Quic tunnel help ");
  fmt::print("\n");

  fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "--mode ");
  fmt::print(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "MODE");
  fmt::print(" : Set the mode <client|server>\n");

  fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "--quic-impl ");
  fmt::print(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "IMPL");
  fmt::print(" : Set the quic implementation to use (for both client and server) "
	     "(default {})\n", def::QUIC_IMPL);
  
  fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "--udp-port ");
  fmt::print(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "PORT");
  fmt::print(" : Set the port of the udp socket, which is the \"fake\" turn server "
	     "(default {})\n", def::UDP_PORT);

  fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "--quic-port ");
  fmt::print(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "PORT");
  fmt::print(" : Set the port of the quic internal connection "
	     "(default {})\n", def::QUIC_PORT);

  fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "--turn-port ");
  fmt::print(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "PORT");
  fmt::print(" : Set the port of the actual turn server "
	     "(default {})\n", def::TURN_HOSTNAME);

  fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "--turn-addr ");
  fmt::print(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "host");
  fmt::print(" : Set the hostname of the actual turn relay server "
	     "(default {})\n", def::TURN_PORT);

  fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "--websocket-port ");
  fmt::print(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "PORT");
  fmt::print(" : Set the port of the websocket server "
	     "(default {})\n", def::WEBSOCKET_PORT);

  fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "--quic-server-host ");
  fmt::print(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "host");
  fmt::print(" : Set the host of the quic server "
	     "(default {})\n", def::QUIC_SERVER_HOST);

  fmt::print("\n");
}

enum OptInd : uint8_t {
  MODE = 0,
  UDP_PORT,
  QUIC_PORT,
  TURN_PORT,
  TURN_ADDR,
  WEBSOCKET_PORT,
  IF_NAME,
  QUIC_SERVER_HOST,
  QUIC_IMPL,
  HELP
};

InTunnel * __in__;
OutTunnel * __out__;
std::thread __stop_thread__;

template<typename T>
void onstop(int sig)
{
  if constexpr (std::is_same_v<InTunnel, T>) {
    __stop_thread__ = std::thread([](){__in__->stop();});
  }
  else {
     __stop_thread__ = std::thread([](){__out__->stop();});
  }
}

using namespace std::string_literals;

int main(int argc, char *argv[])
{
  std::srand(time(0));

  struct option long_options[] = {
    { "mode", required_argument, 0, 0 },
    { "udp-port", required_argument, 0, 0 },
    { "quic-port", required_argument, 0, 0 },
    { "turn-port", required_argument, 0, 0 },
    { "turn-addr", required_argument, 0, 0 },
    { "websocket-port", required_argument, 0, 0 },
    { "if_name", required_argument, 0, 0 },
    { "quic-server-host", required_argument, 0, 0 },
    { "quic-impl", required_argument, 0, 0 },
    { "help", no_argument, 0, 0 },
    { 0, 0, 0, 0 },
  };

  std::optional<std::string> mode;
  int turn_port         = def::TURN_PORT;
  int quic_port         = def::QUIC_PORT;
  int udp_port          = def::UDP_PORT;
  int websocket_port    = def::WEBSOCKET_PORT;
  std::string if_name   = def::IF_NAME;
  std::string turn_addr = def::TURN_HOSTNAME;
  std::string quic_server_host = def::QUIC_SERVER_HOST;
  std::string quic_impl = def::QUIC_IMPL;
  
  while(true) {
    int option_index = 0;
    
    int c = getopt_long(argc, argv, "h", long_options, &option_index);

    if(c == -1) break;
    else if(c == 'h') {
      display_help();
      std::exit(0);
    }
    else if(c != 0) continue;
    
    switch(option_index) {
    case OptInd::MODE:
      if((optarg != "client"s && optarg != "server"s) && optarg != "websocket"s) {
	fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "error : ");
	fmt::print("{} is not a valid mode. Accepted mode are ", optarg);
	fmt::print(fmt::emphasis::italic, "client");
	fmt::print(" or ");
	fmt::print(fmt::emphasis::italic, "server\n");
      }
      else mode = optarg;
      break;
    case OptInd::UDP_PORT: udp_port = std::stoi(optarg);   break;
    case OptInd::TURN_PORT: turn_port = std::stoi(optarg); break;
    case OptInd::WEBSOCKET_PORT: websocket_port = std::stoi(optarg); break;
    case OptInd::TURN_ADDR: turn_addr = optarg; break;
    case OptInd::IF_NAME: if_name = optarg; break;
    case OptInd::QUIC_SERVER_HOST: quic_server_host = optarg; break;
    case OptInd::QUIC_PORT: quic_port = std::stoi(optarg); break;
    case OptInd::QUIC_IMPL: quic_impl = optarg; break;
    case OptInd::HELP: display_help(); std::exit(0);
    }    
  }
  
  if(!mode.has_value()) {
    fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "error : ");
    fmt::print("You must specify a running mode\n");

    return -1;
  }

  if(*mode == "client") {
    InTunnel in(0, quic_impl, quic_server_host, quic_port);
    in.allocate_in_port();
    in.set_datagram(true);

    __in__ = &in;
    
    signal(SIGINT, onstop<InTunnel>);
    
    in.run();

    if(__stop_thread__.joinable()) __stop_thread__.join();
  }
  else if(*mode == "server") {
    OutTunnel out(0, quic_impl, turn_addr, quic_port, turn_port);
    out.set_datagrams(true);

    __out__ = &out;    
    signal(SIGINT, onstop<OutTunnel>);
    
    out.run();

    if(__stop_thread__.joinable()) __stop_thread__.join();
  }
  else if(*mode == "websocket") {  
    // init tc
    tc::Link::init(if_name.c_str());
  
    WebsocketServer ws;
    ws.run(websocket_port);

    tc::Link::exit();
  }

  return 0;
}
