#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <memory>
#include <unordered_map>
#include <functional>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace cmd
{
class Command;
}

namespace response
{
class Response;
}

class JsonParser
{
public:
  using CommandPtr = std::unique_ptr<cmd::Command>;
  
private:
  std::unordered_map<std::string, std::function<CommandPtr(const nlohmann::json&)>> _request;

  CommandPtr parse_start_client(const nlohmann::json& data);
  CommandPtr parse_stop_client(const nlohmann::json& data);
  CommandPtr parse_start_server(const nlohmann::json& data);
  CommandPtr parse_stop_server(const nlohmann::json& data);
  CommandPtr parse_link(const nlohmann::json& data);
  
public:				    
  JsonParser() noexcept;
  virtual ~JsonParser() = default;

  CommandPtr parse_request(const nlohmann::json& request);
  
  std::string make_response(response::Response* response);
};


#endif /* JSON_PARSER_H */
