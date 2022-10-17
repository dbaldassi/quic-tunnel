#ifndef COMMANDS_H
#define COMMANDS_H

#include <memory>
#include <vector>
#include <string>
#include <optional>

namespace response
{
class Response;
}

namespace cmd
{

using ResponsePtr = std::unique_ptr<response::Response>;

// BaseClass //////////////////////////////////////////////////////////////////

class Command
{
protected:
  int _trans_id;
  
public:
  virtual ~Command() = default;
  
  void set_transaction_id(int trans_id) { _trans_id = trans_id; }
  
  virtual ResponsePtr run() = 0;
};

// StartClient ////////////////////////////////////////////////////////////////

class StartClient : public Command
{
public:
  bool        datagrams = true;
  std::string cc        = "bbr";
  std::string impl      = "mvfst"; // TODO

  int         quic_port;
  std::string quic_host;

  bool external_file_transfer    = false;
  bool multiplexed_file_transfer = false;
  
  ResponsePtr run() override;
};

// StopClient /////////////////////////////////////////////////////////////////

class StopClient : public Command
{

public:
  int id; // Id of the quic session
  
  ResponsePtr run() override;
};

// StartServer ////////////////////////////////////////////////////////////////

class StartServer : public Command
{
public:
  int port_out;
  int quic_port;
  
  std::string cc;
  std::string quic_host;
  std::string addr_out;
  std::string impl = "mvfst"; // TODO

  bool datagrams = true;
  bool external_file_transfer    = false;
  bool multiplexed_file_transfer = false;

  ResponsePtr run() override;
};

// StopServer /////////////////////////////////////////////////////////////////

class StopServer : public Command
{

public:
  int id; // Id of the quic session
  
  ResponsePtr run() override;
};


// Link ///////////////////////////////////////////////////////////////////////

class Link : public Command
{
public:
  int bitrate; // max bitrate in kbps
  int delay = 1; // one way delay / propagation delay in ms
  int burst = 20; // burst in kb
  int latency = 400; // queing latency in ms

  std::optional<int> loss;
  std::optional<int> duplicates;

  bool reset = false; // if reset has been requested

  ResponsePtr run() override;
};

// Impl ///////////////////////////////////////////////////////////////////////

class Capabilities : public Command
{
public:

  bool out_requested = false;
  bool in_requested = false;
  
  ResponsePtr run() override;
};

// Stats //////////////////////////////////////////////////////////////////////
class UploadRTCStats : public Command
{

public:
  struct Point {
    int x;
    int y;
  };

  std::vector<Point> link;
  std::vector<Point> bitrate;
  
  ResponsePtr run() override;
};

class GetStats : public Command
{
public:
  std::string exp_name;
  std::optional<std::string> qvis_file;
  
  ResponsePtr run() override;
};

}

#endif /* COMMANDS_H */
