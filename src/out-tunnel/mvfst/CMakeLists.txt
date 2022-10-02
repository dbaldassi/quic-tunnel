
add_library( mvfst_server )
add_library( mvfst::server ALIAS mvfst_server )

target_sources( mvfst_server PRIVATE
  callback_handler.cpp
  mvfst_server.cpp

  callback_handler.h
  mvfst_server.h
  )

target_include_directories( mvfst_server PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

find_package( glog REQUIRED )

target_link_libraries( mvfst_server PUBLIC
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

  glog::glog
  gflags
  zstd
  )
