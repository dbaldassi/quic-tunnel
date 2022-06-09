#include <thread>

#include "commands.h"
#include "response.h"
#include "link.h"

#include "in-tunnel/intunnel.h"

namespace cmd
{

// StartClient ////////////////////////////////////////////////////////////////

std::unique_ptr<response::Response> StartClient::run()
{
  // Create quic client
  auto client = ::MvfstInClient::create("127.0.0.1", 8888);

  // Ensure it is created
  if(client == nullptr) {
    auto resp = std::make_unique<response::Error>();
    resp->trans_id = _trans_id;
    resp->message = "Internal error, could not create quic session";

    return resp;
  }

  // Set the cc
  client->set_cc(cc);
  client->set_datagram(datagrams);
  
  // Create websocket response
  auto resp = std::make_unique<response::StartClient>();
  
  // Fill websocket response
  resp->trans_id = _trans_id;
  resp->port = client->allocate_in_port();
  resp->id = client->id();

  // Run the client in a detached thread
  std::thread([client]() -> void { client->run(); }).detach();

  // return response
  return resp;
}

// StopClient /////////////////////////////////////////////////////////////////

ResponsePtr StopClient::run()
{
  // Check if the specified id exists
  if(MvfstInClient::sessions.contains(id)) {
    auto client = MvfstInClient::sessions[id];
    MvfstInClient::sessions.erase(id);

    client->stop();

    auto resp = std::make_unique<response::StopClient>();
    resp->trans_id = _trans_id;
    resp->url = "https://qvis.dabaldassi.fr?file=" + client->get_qlog_file();

    client = nullptr;

    return resp;
  }

  // Does not exist, return an error
  auto resp = std::make_unique<response::Error>();
  resp->trans_id = _trans_id;
  resp->message = "Specified quic session id does not exist";
  
  return std::move(resp);
}

// Link ///////////////////////////////////////////////////////////////////////
ResponsePtr Link::run()
{
  bool success = true;

  if(reset) {
    tc::Link::reset_limit();
  }
  else {
    success = tc::Link::set_limit(std::chrono::milliseconds(delay), bit::KiloBits(bitrate));
  }

  if(!success) {
    auto resp = std::make_unique<response::Error>();
    resp->trans_id = _trans_id;
    resp->message = "Could not set link limit";
    return resp;
  }
  
  auto resp = std::make_unique<response::Link>();
  resp->trans_id = _trans_id;
  
  return resp;
}

}
