
add_library( mvfst_tunnel_server SHARED )
add_library( mvfst::server ALIAS mvfst_tunnel_server )

target_sources( mvfst_tunnel_server PRIVATE
  callback_handler.cpp
  mvfst_server.cpp

  callback_handler.h
  mvfst_server.h
  )

target_include_directories( mvfst_tunnel_server PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

# -- check for required package
find_package( mvfst  REQUIRED )
find_package( glog REQUIRED )
find_package( fmt  REQUIRED )

target_link_libraries( mvfst_tunnel_server PRIVATE
  mvfst::mvfst_server
  fmt::fmt
  glog::glog
  gflags
  zstd
  /opt/liburing/lib/liburing.a
  )
