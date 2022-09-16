cmake_minimum_required( VERSION 3.16 FATAL_ERROR )

project( quic-tunnel )

set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )

find_package( mvfst REQUIRED )

# --- ASIO
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/external/asio/asio/include )
# --- websocketpp
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp )
# --- JSON
set( JSON_BuildTests      OFF CACHE INTERNAL "" )
set( JSON_Install         OFF CACHE INTERNAL "" )
set( JSON_MultipleHeaders ON  CACHE INTERNAL "" )
add_subdirectory( external/nlohmann_json )

add_executable( quic-tunnel
  src/in-tunnel/quic_client.cpp
  src/in-tunnel/mvfst_client.cpp
  src/in-tunnel/udp_socket.cpp
  src/in-tunnel/intunnel.cpp
  
  src/out-tunnel/callback_handler.cpp
  src/out-tunnel/quic_server.cpp
  src/out-tunnel/mvfst_server.cpp
  src/out-tunnel/udp_socket.cpp
  src/out-tunnel/outtunnel.cpp

  src/controls/websocket_server.cpp
  src/controls/json_parser.cpp
  src/controls/commands.cpp
  src/controls/response.cpp
  src/controls/link.cpp
  src/controls/concurrent_queue.h
  
  src/main.cpp
  )

set_target_properties( quic-tunnel PROPERTIES CXX_STANDARD 20 )

target_include_directories( quic-tunnel PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  /usr/include/libnl3
  )

target_link_libraries( quic-tunnel PRIVATE
  nl-3
  nl-route-3

  nlohmann_json
  
  mvfst::mvfst_constants
  mvfst::mvfst_exception
  mvfst::mvfst_transport
  mvfst::mvfst_client
  mvfst::mvfst_codec_types
  mvfst::mvfst_codec_decode
  mvfst::mvfst_codec_pktbuilder
  mvfst::mvfst_codec_pktrebuilder
  mvfst::mvfst_codec_packet_number_cipher
  mvfst::mvfst_codec
  mvfst::mvfst_looper
  mvfst::mvfst_buf_accessor
  mvfst::mvfst_bufutil
  mvfst::mvfst_socketutil
  mvfst::mvfst_transport_knobs
  mvfst::mvfst_cc_algo
  mvfst::mvfst_dsr_sender
  mvfst::mvfst_dsr_types
  mvfst::mvfst_dsr_frontend
  mvfst::mvfst_fizz_client
  mvfst::mvfst_fizz_handshake
  mvfst::mvfst_flowcontrol
  mvfst::mvfst_handshake
  mvfst::mvfst_happyeyeballs
  mvfst::mvfst_qlogger
  mvfst::mvfst_loss
  mvfst::mvfst_server
  mvfst::mvfst_server_state
  mvfst::mvfst_state_machine
  mvfst::mvfst_state_ack_handler
  mvfst::mvfst_state_datagram_handler
  mvfst::mvfst_state_stream_functions
  mvfst::mvfst_state_pacing_functions
  mvfst::mvfst_state_functions
  mvfst::mvfst_state_simple_frame_functions
  mvfst::mvfst_state_stream
  mvfst::mvfst_d6d_probe_raiser
  mvfst::mvfst_d6d_state_functions
  mvfst::mvfst_d6d_types
  gflags
  )
