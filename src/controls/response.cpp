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

nlohmann::json Capabilities::data() const
{
  json d = {
	    
  };

  return d;
}

nlohmann::json Link::data() const
{
  return json{};
}

}
