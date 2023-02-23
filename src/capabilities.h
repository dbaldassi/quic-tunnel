#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#include <string>
#include <vector>

struct Capabilities
{
  std::string impl;
  bool datagrams;
  bool streams = true;
  std::vector<std::string> cc;
};

#endif /* CAPABILITIES_H */
