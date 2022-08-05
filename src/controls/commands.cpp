#include <thread>

#include "commands.h"
#include "response.h"
#include "link.h"

#include "in-tunnel/intunnel.h"
#include "out-tunnel/outtunnel.h"

namespace cmd
{

// StartClient ////////////////////////////////////////////////////////////////

std::unique_ptr<response::Response> StartClient::run()
{
  // Create quic client
  auto client = ::MvfstInClient::create(quic_host, quic_port);

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

  if(external_file_transfer)
    client->enable_external_file_transfer();
  
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

    if(const char * qvis = std::getenv("QVIS_URL")) {
      resp->url = qvis;
    }
    else {
      resp->url = "https://qvis.dabaldassi.fr";
    }
    
    resp->url += "?file=" + client->get_qlog_file();

    client = nullptr;

    return resp;
  }

  // Does not exist, return an error
  auto resp = std::make_unique<response::Error>();
  resp->trans_id = _trans_id;
  resp->message = "Specified quic session id does not exist";
  
  return std::move(resp);
}

// StartServer ////////////////////////////////////////////////////////////////

ResponsePtr StartServer::run()
{
  // Create quic client
  auto server = ::MvfstOutClient::create(addr_out, quic_port, port_out);
  server->set_cc(cc);
  
  // Ensure it is created
  if(server == nullptr) {
    auto resp = std::make_unique<response::Error>();
    resp->trans_id = _trans_id;
    resp->message = "Internal error, could not create quic session";

    return resp;
  }

  // Create websocket response
  auto resp = std::make_unique<response::StartServer>();
  
  // Fill websocket response
  resp->trans_id = _trans_id;
  resp->id = server->id();

  // Run the client in a detached thread
  std::thread([server]() -> void { server->run(); }).detach();

  // return response
  return resp;
}

// StopServer /////////////////////////////////////////////////////////////////

ResponsePtr StopServer::run()
{
  // Check if the specified id exists
  if(MvfstOutClient::sessions.contains(id)) {
    auto server = MvfstOutClient::sessions[id];
    MvfstOutClient::sessions.erase(id);

    server->stop();

    auto resp = std::make_unique<response::StopServer>();
    resp->trans_id = _trans_id;
    
    if(const char * qvis = std::getenv("QVIS_URL")) {
      resp->url = qvis;
    }
    else {
      resp->url = "https://qvis.dabaldassi.fr";
    }
    
    resp->url += "?file=" + server->get_qlog_file();

    server = nullptr;

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
