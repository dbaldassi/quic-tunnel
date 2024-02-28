#ifndef OUTTUNNEL_H
#define OUTTUNNEL_H

#include <string_view>
#include <unordered_map>
#include <memory>
#include <vector>

#include "out-tunnel/udp_socket.h"
#include "random_generator.h"
#include "capabilities.h"

class QuicServer;

class OutTunnel
{
  int _id;
  int _out_port;
  out::UdpSocket _udp_socket;
  std::unique_ptr<QuicServer> _quic_server;
  bool _external_file_transfer;

  static RandomGenerator _random_generator;
public:

  static constexpr auto MAX_NUMBER_SESSION = 5;
  
  static std::unordered_map<int, std::shared_ptr<OutTunnel>> sessions;

  OutTunnel(int id, std::string_view impl, std::string_view server_addr, uint16_t server_port, uint16_t out_port);
  ~OutTunnel() noexcept;
    
  int id() const { return _id; }


  bool start();
  void set_datagrams(bool enable);
  void run();
  void stop();

  void set_cc(std::string_view cc);
  void set_external_file_transfer(bool enable) noexcept { _external_file_transfer = enable; }
  std::string get_qlog_file();

  static std::shared_ptr<OutTunnel> create(std::string_view impl,
					   std::string_view server_addr,
					   uint16_t server_port,
					   uint16_t out_port);

  static void get_capabilities(std::vector<Capabilities>& impls);
};

#endif /* OUTTUNNEL_H */
