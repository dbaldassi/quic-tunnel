# -- controls

target_sources( quic-tunnel PRIVATE  
  websocket_server.cpp
  json_parser.cpp
  commands.cpp
  response.cpp
  link.cpp
  
  concurrent_queue.h
  commands.h
  link.h
  json_parser.h
  response.h
  websocket_server.h
  )

target_include_directories( quic-tunnel PRIVATE
  /usr/include/libnl3
  )

target_link_libraries( quic-tunnel PRIVATE
  nl-3
  nl-route-3

  nlohmann_json
  )
