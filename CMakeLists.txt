cmake_minimum_required( VERSION 3.16 FATAL_ERROR )

project( quic_tunnel )

set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )

find_package( mvfst REQUIRED )

add_executable( in-tunnel
  in-tunnel/main.cpp
  in-tunnel/quic_client.cpp
  in-tunnel/udp_socket.cpp
  )

add_executable( out-tunnel
  out-tunnel/main.cpp
  out-tunnel/callback_handler.cpp
  out-tunnel/quic_server.cpp
  out-tunnel/udp_socket.cpp
  )

set_target_properties( in-tunnel  PROPERTIES CXX_STANDARD 17 )
set_target_properties( out-tunnel PROPERTIES CXX_STANDARD 17 )

target_link_libraries( in-tunnel PRIVATE
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
  )

target_link_libraries( out-tunnel PRIVATE
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
  )
