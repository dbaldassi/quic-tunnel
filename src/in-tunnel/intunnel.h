#ifndef INTUNNEL_H
#define INTUNNEL_H

#include <coroutine>

#include <string_view>
#include <unordered_map>
#include <memory>

#include "udp_socket.h"

// int run_client(int turn_port, uint16_t quic_port);

class QuicClient;

struct RandomGenerator
{
  struct promise_type;
  using handle_type = std::coroutine_handle<promise_type>;

  struct promise_type
  {
    int value;

    RandomGenerator get_return_object() {
      return RandomGenerator(handle_type::from_promise(*this));
    }

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() {}
    
    template<std::convertible_to<int> From> // C++20 concept
    std::suspend_always yield_value(From &&from) {
      value = from;
      return {};
    }
    void return_void() {}
  };
 
  handle_type _h;
 
  RandomGenerator(handle_type h) : _h(h) {}
  ~RandomGenerator() { _h.destroy(); }

  int operator()() { return _h.promise().value;  }
};

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
