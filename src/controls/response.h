#ifndef RESPONSE_H
#define RESPONSE_H

#include <optional>
#include <nlohmann/json_fwd.hpp>

namespace response
{

struct Response
{
  int trans_id;

  virtual nlohmann::json data() const = 0;
  virtual std::string type() const { return "response"; }

  virtual ~Response() = default;
};

struct Error : public Response
{
  std::string message;

  nlohmann::json data() const override;
  std::string type() const override { return "error"; }
};

struct StartClient : public Response
{
  int port;
  int id;
  std::optional<int> stream_id;

  nlohmann::json data() const override;
};

struct StopClient : public Response
{
  std::string url;
  
  nlohmann::json data() const override;
};

struct StartServer : public Response
{
  int id;

  nlohmann::json data() const override;
};

struct StopServer : public Response
{  
  nlohmann::json data() const override;
};

struct Capabilities : public Response
{
  std::string impl_name;
  std::vector<std::string> cc;
  bool datagram;
  
  nlohmann::json data() const override;
};

struct Link : public Response
{
  nlohmann::json data() const override;
};

}



#endif /* RESPONSE_H */
