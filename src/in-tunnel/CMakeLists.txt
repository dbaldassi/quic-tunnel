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
  )

add_subdirectory( mvfst )
add_subdirectory( quicgo )

target_include_directories( quic-tunnel PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

target_link_libraries( quic-tunnel PRIVATE
  mvfst::client
  quicgo::client
  )
