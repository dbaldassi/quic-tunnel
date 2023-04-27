#ifndef RESPONSE_H
#define RESPONSE_H

#include <optional>
#include <map>
#include <nlohmann/json_fwd.hpp>

#include "capabilities.h"

namespace response
{

struct Response
{
  int trans_id;

  virtual nlohmann::json data() const = 0;
  virtual std::string type() const { return "response"; }

  virtual ~Response() = default;
};

// error //////////////////////////////////////////////////////////////////////

struct Error : public Response
{
  std::string message;

  nlohmann::json data() const override;
  std::string type() const override { return "error"; }
};

// StartClient ////////////////////////////////////////////////////////////////

struct StartClient : public Response
{
  int port;
  int id;
  std::optional<int> stream_id;

  nlohmann::json data() const override;
};

// StopClient /////////////////////////////////////////////////////////////////

struct StopClient : public Response
{
  std::string url;
  
  nlohmann::json data() const override;
};

// Start Server ///////////////////////////////////////////////////////////////

struct StartServer : public Response
{
  int id;

  nlohmann::json data() const override;
};

// StopServer /////////////////////////////////////////////////////////////////

struct StopServer : public Response
{
  std::string url;
  nlohmann::json data() const override;
};

// Link ///////////////////////////////////////////////////////////////////////

struct Link : public Response
{
  nlohmann::json data() const override;
};

// Impl ///////////////////////////////////////////////////////////////////////

struct Capabilities : public Response
{
  std::vector<::Capabilities> in_cap;
  std::vector<::Capabilities> out_cap;

  nlohmann::json data() const override;
};

// UploadStats ////////////////////////////////////////////////////////////////

struct UploadRTCStats : public Response
{
  nlohmann::json data() const override;
};

// GetStats ///////////////////////////////////////////////////////////////////

struct GetStats : public Response
{
  std::string                url;
  std::optional<std::string> tcp_url;
  std::optional<std::string> qvis_url;
  
  nlohmann::json data() const override;
};

}



#endif /* RESPONSE_H */
