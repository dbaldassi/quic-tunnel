#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#include <string>
#include <vector>

/* Capabilities supported by a QUIC implementation */
struct Capabilities
{
  std::string              impl; /* which quic implementation (or if tcp / udp)*/
  bool                     datagrams; /* Does it support datagrams */
  bool                     streams = true; /* Does it support streams */
  std::vector<std::string> cc; /* Which congestion contorl algorithm it supports */
};

#endif /* CAPABILITIES_H */
