#include <nlohmann/json.hpp>

#include "response.h"

using nlohmann::json;

namespace response
{

nlohmann::json Error::data() const
{
  json d = {
    { "message", message }
  };
  
  return d;
}

nlohmann::json StartClient::data() const
{
  json d = {
    { "port", port },
    { "id", id }
  };

  if(stream_id.has_value()) d.emplace("streamId", *stream_id);

  return d;
}

nlohmann::json StopClient::data() const
{
  return json{ { "url", url } };
}

nlohmann::json StartServer::data() const
{
  return json{ { "id", id }};
}

nlohmann::json StopServer::data() const
{
  return json{ { "url", url } };
}

nlohmann::json Link::data() const
{
  return json{};
}

nlohmann::json Capabilities::data() const
{
  json d = json{};
  
  if(!in_cap.empty()) {
    std::vector<json> caps;

    for(auto& cap : in_cap) {
      json c;
      c.emplace("impl", cap.impl);
      c.emplace("cc", cap.cc);
      c.emplace("datagrams", cap.datagrams);
      caps.push_back(c);
    }
    
    d.emplace("in_impls", caps);
  }
  
  if(!out_cap.empty()) {
    std::vector<json> caps;

    for(auto& cap : out_cap) {
      json c;
      c.emplace("impl", cap.impl);
      c.emplace("cc", cap.cc);
      c.emplace("datagrams", cap.datagrams);
      caps.push_back(c);
    }
    
    d.emplace("out_impls", caps);
  }
  
  return d;
}

nlohmann::json UploadRTCStats::data() const
{
  return json{};
}

nlohmann::json GetStats::data() const
{
  return json{ { "url", url } };
}


}
