#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_
#define _WEBSOCKETPP_CPP11_THREAD_
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_MEMORY_

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <thread>

#include "json_parser.h"
#include "concurrent_queue.h"

using server_t = websocketpp::server<websocketpp::config::asio>;

class WebsocketServer
{
  using AcceptorPtr = websocketpp::lib::shared_ptr<websocketpp::lib::asio::ip::tcp::acceptor>;

  server_t   _endpoint;
  JsonParser _parser;

  std::jthread _process_th;
  
  ConcurrentQueue<std::pair<websocketpp::connection_hdl,server_t::message_ptr>> _queue;
  
  /**
   * @brief Callback at initialisation of the tcp socket so we can set REUSE_PORT before the socket is initialized
  */
  std::error_code tcp_handler(AcceptorPtr acceptor);
  
  /**
   * @brief Called when a message is received
   */
  void on_message(websocketpp::connection_hdl, server_t::message_ptr msg);

  /**
   * @brief Thread to pop request from queue and send it to the database manager (can't do it from message handler)
   */
  void process(std::stop_token stoken);
  
public:
  WebsocketServer() noexcept;
  ~WebsocketServer() noexcept;

  /**
   *\brief Run the websocket server on the given port
   *\param port Port to bind server
   */  
  void run(int port);

  void send(websocketpp::connection_hdl, const std::string & msg);
  void send(websocketpp::connection_hdl, const void * msg, size_t size);
};

#endif /* WEBSOCKET_SERVER_H */
