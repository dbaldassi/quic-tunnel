#ifndef COMMANDS_H
#define COMMANDS_H

#include <memory>

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
  bool datagrams = true;
  std::string cc = "BBR";
  std::string impl = "mvfst";

  ResponsePtr run() override;
};

// StopClient /////////////////////////////////////////////////////////////////

class StopClient : public Command
{

public:
  int id; // Id of the quic session
  
  ResponsePtr run() override;
};

}

#endif /* COMMANDS_H */
