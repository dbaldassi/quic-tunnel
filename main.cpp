
#include <iostream>
#include <getopt.h>

#include <fmt/core.h>
#include <fmt/color.h>

#include "in-tunnel/intunnel.h"
#include "out-tunnel/outtunnel.h"

namespace def
{

constexpr auto UDP_PORT  = 3479;
constexpr auto TURN_PORT = 3478;
constexpr auto QUIC_PORT = 8888;
constexpr const char * TURN_HOSTNAME = "turn.dabaldassi.fr";

}

void display_help()
{
  fmt::print("{:*^50}\n", " Quic tunnel help ");
  fmt::print("\n");

  fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "--mode ");
  fmt::print(fg(fmt::color::steel_blue) | fmt::emphasis::bold, "MODE");
  fmt::print(" : Set the mode <client|server>\n");
  
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
  fmt::print(" : Set the port of the actual turn relay server "
	     "(default {})\n", def::TURN_PORT);

  fmt::print("\n");
}

enum OptInd : uint8_t {
  MODE = 0,
  UDP_PORT,
  QUIC_PORT,
  TURN_PORT,
  HELP
};

using namespace std::string_literals;

int main(int argc, char *argv[])
{
  struct option long_options[] = {
    { "mode", required_argument, 0, 0 },
    { "udp-port", required_argument, 0, 0 },
    { "quic-port", required_argument, 0, 0 },
    { "turn-port", required_argument, 0, 0 },
    { "help", no_argument, 0, 0 },
    { 0, 0, 0, 0 },
  };

  std::optional<std::string> mode;
  int turn_port = def::TURN_PORT;
  int quic_port = def::QUIC_PORT;
  int udp_port = def::UDP_PORT;
  
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
      if(optarg != "client"s && optarg != "server"s) {
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
    case OptInd::QUIC_PORT: quic_port = std::stoi(optarg); break;
    case OptInd::HELP: display_help(); std::exit(0);
    }    
  }
  
  if(!mode.has_value()) {
    fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "error : ");
    fmt::print("You must specify a running mode\n");

    return -1;
  }

  if(*mode == "client") run_client(udp_port, quic_port);
  else run_quic_server(def::TURN_HOSTNAME, turn_port, quic_port);
  
  return 0;
}

