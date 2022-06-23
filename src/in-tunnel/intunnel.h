#ifndef INTUNNEL_H
#define INTUNNEL_H

#include <string_view>
#include <unordered_map>
#include <memory>

#include "udp_socket.h"
#include "random_generator.h"

class QuicClient;

class MvfstInClient
{
  int _id;
  int _in_port;
  std::unique_ptr<QuicClient> _quic_client;
  in::UdpSocket _udp_socket;
  bool _datagrams;

  static RandomGenerator _random_generator;
public:

  static constexpr auto MAX_NUMBER_SESSION = 5;
  
  static std::unordered_map<int, std::shared_ptr<MvfstInClient>> sessions;


  MvfstInClient(int id, std::string_view server_addr, uint16_t server_port);
  ~MvfstInClient() noexcept;
  
  void set_cc(std::string_view cc);
  void set_datagram(bool enable);
  int allocate_in_port();
  int id() const { return _id; }

  std::string get_qlog_file();
  
  void run();
  void stop();

  static std::shared_ptr<MvfstInClient> create(std::string_view server_addr, uint16_t server_port);
};

#endif /* INTUNNEL_H */
