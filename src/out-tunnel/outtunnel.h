#ifndef OUTTUNNEL_H
#define OUTTUNNEL_H

#include <string_view>
#include <unordered_map>
#include <memory>

#include "out-tunnel/udp_socket.h"
#include "random_generator.h"

class QuicServer;

class OutTunnel
{
  int _id;
  int _out_port;
  out::UdpSocket _udp_socket;
  std::unique_ptr<QuicServer> _quic_server;

  static RandomGenerator _random_generator;
public:

  static constexpr auto MAX_NUMBER_SESSION = 5;
  
  static std::unordered_map<int, std::shared_ptr<OutTunnel>> sessions;

  OutTunnel(int id, std::string_view server_addr, uint16_t server_port, uint16_t out_port);
  ~OutTunnel() noexcept;
    
  int id() const { return _id; }


  void set_datagrams(bool enable);
  void run();
  void stop();

  void set_cc(std::string_view cc);
  std::string get_qlog_file();

  static std::shared_ptr<OutTunnel> create(std::string_view server_addr,
					   uint16_t server_port,
					   uint16_t out_port);
};

#endif /* OUTTUNNEL_H */
