#include "json_parser.h"

#include <fmt/core.h>

#include <nlohmann/json.hpp>

#include "response.h"
#include "commands.h"

using nlohmann::json;

JsonParser::JsonParser() noexcept
{
  _request.emplace("startclient", [this](const json& d) { return parse_start_client(d); });
  _request.emplace("stopclient", [this](const json& d) { return parse_stop_client(d); });
  _request.emplace("link", [this](const json& d) { return parse_link(d); });
}

JsonParser::CommandPtr JsonParser::parse_start_client(const json& data)
{
  auto cmd = std::make_unique<cmd::StartClient>();

  if(auto dgram = data.find("datagrams"); dgram != data.end()) {
    cmd->datagrams = dgram->get<bool>();
  }
  if(auto cc = data.find("cc"); cc != data.end()) {
    cmd->cc = cc->get<std::string>();
  }
  if(auto impl = data.find("impl"); impl != data.end()) {
    cmd->impl = impl->get<std::string>();
  }
  
  return std::move(cmd);
}

JsonParser::CommandPtr JsonParser::parse_stop_client(const json& data)
{
  if(auto id = data.find("id"); id != data.end()) {
    auto cmd = std::make_unique<cmd::StopClient>();
    cmd->id = id->get<int>();
    
    return cmd;
  }

  return nullptr;
}

JsonParser::CommandPtr JsonParser::parse_link(const json& data)
{
  auto cmd = std::make_unique<cmd::Link>();
  
  if(auto delay = data.find("delay"); delay != data.end()) {
    cmd->delay = delay->get<int>();
  }

  if(auto bitrate = data.find("bitrate"); bitrate != data.end()) {
    cmd->bitrate = bitrate->get<int>();
  }
  else cmd->reset = true;

  return cmd;
}

JsonParser::CommandPtr JsonParser::parse_request(const nlohmann::json &request)
{
  auto cmd      = request.find("cmd");
  auto data     = request.find("data");
  auto trans_id = request.find("transId");
  
  // if one is missing -> parsing error
  if((cmd == request.end() || data == request.end()) || trans_id == request.end())
    return nullptr;
  
  if(auto it = _request.find(cmd->get<std::string>()); it != _request.end()) {
    auto command = it->second(*data);
    if(!command) return nullptr;
    
    command->set_transaction_id(trans_id->get<int>());

    return command;
  }
  
  return nullptr;
}

std::string JsonParser::make_response(response::Response* response)
{
  json r = {
    { "type", response->type() },
    { "transId", response->trans_id },
    { "data", response->data() }
  };

  return r.dump();
}
