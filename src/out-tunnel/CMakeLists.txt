# -- out tunnel

target_sources( quic-tunnel PRIVATE  
  quic_server.cpp
  udp_socket.cpp
  outtunnel.cpp

  quic_server.h
  udp_socket.h
  outtunnel.h

  tcp/tcp_server.cpp
  tcp/tcp_server.h

  udp/udp_server.cpp
  udp/udp_server.h  
  )

add_subdirectory( mvfst )
add_subdirectory( quicgo )
add_subdirectory( lsquic )
add_subdirectory( quiche )
add_subdirectory( msquic )

target_include_directories( quic-tunnel PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

target_link_libraries( quic-tunnel PRIVATE
  mvfst::server
  lsquic::server
  quicgo::server
  quiche::server
  msquic::server
  )

