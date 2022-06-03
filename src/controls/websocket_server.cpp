
#include <fmt/core.h>

#include "nlohmann/json.hpp"

#include "response.h"
#include "commands.h"
#include "websocket_server.h"

using nlohmann::json;

WebsocketServer::WebsocketServer() noexcept
{
  _endpoint.set_error_channels(websocketpp::log::elevel::all);
  _endpoint.set_access_channels(websocketpp::log::alevel::none);
  _endpoint.set_reuse_addr(true);
  _endpoint.init_asio();
  
  _endpoint.set_message_handler([this](auto&& hdl, auto&& msg) { on_message(hdl, msg); });
  _endpoint.set_tcp_pre_bind_handler([this](auto&& acceptor) { return tcp_handler(acceptor); });

  _process_th = std::jthread([this](std::stop_token stoken) -> void { process(stoken); });
}

WebsocketServer::~WebsocketServer() noexcept
{
  _process_th.request_stop();
  _queue.push({});
  _process_th.join();
}

void WebsocketServer::process(std::stop_token stoken)
{
  while(true) {
    auto [hdl, msg] = _queue.pop();
    
    if(stoken.stop_requested()) return;

      std::thread([this, hdl, msg]() {

        try {
	  auto message = json::parse(msg->get_payload());
	  auto cmd = _parser.parse_request(message);
	  
	  if(cmd == nullptr) {
	    response::Error err;
	    err.message = "Could not parse request";
	    
	    send(hdl, _parser.make_response(&err));
	  }
	  else {
	    auto response = cmd->run();
	    send(hdl, _parser.make_response(response.get()));
	  }
	  
	} catch(json::parse_error& error) {
	  response::Error err;
	  err.message = "Not valid JSON message";
	  
	  send(hdl, _parser.make_response(&err));
	}
    }).detach();    
  }
}

std::error_code WebsocketServer::tcp_handler(AcceptorPtr acceptor)
{
  int reuse = 1;
  int ec = setsockopt(acceptor->native_handle(),
		      SOL_SOCKET,
		      SO_REUSEPORT,
		      (const char *)&reuse,
		      sizeof(reuse));

  return std::error_code(ec, std::generic_category());
}

void WebsocketServer::run(int port)
{
  fmt::print("Running websocket server on port : {}\n", port);
  while(true) {
    try {
      _endpoint.listen(port);
      _endpoint.start_accept();
      _endpoint.run();
    } catch(std::exception& e) {
      std::cout << "An exception occured. what() : " << e.what() << "\n";
    }
  }
}

void WebsocketServer::on_message(websocketpp::connection_hdl hdl, server_t::message_ptr msg)
{
  _queue.push(std::make_pair(hdl, msg));
}

void WebsocketServer::send(websocketpp::connection_hdl hdl, const std::string& msg)
{
  _endpoint.send(hdl, msg, websocketpp::frame::opcode::text);
}

void WebsocketServer::send(websocketpp::connection_hdl hdl, const void * msg, size_t size)
{
  _endpoint.send(hdl, msg, size, websocketpp::frame::opcode::binary);
}
