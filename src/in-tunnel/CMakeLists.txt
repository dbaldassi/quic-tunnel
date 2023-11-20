# -- in-tunnel

target_sources( quic-tunnel PRIVATE
  quic_client.cpp
  udp_socket.cpp
  intunnel.cpp

  quic_client.h
  udp_socket.h
  intunnel.h

  tcp/tcp_client.cpp
  tcp/tcp_client.h

  udp/udp_client.cpp
  udp/udp_client.h
  )

add_subdirectory( mvfst )
add_subdirectory( quicgo )
add_subdirectory( lsquic )
add_subdirectory( quiche )

target_include_directories( quic-tunnel PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

target_link_libraries( quic-tunnel PRIVATE
  lsquic::client
  mvfst::client
  quicgo::client
  quiche::client
  )
