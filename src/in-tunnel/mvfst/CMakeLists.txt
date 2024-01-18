
add_library( mvfst_tunnel_client SHARED )
add_library( mvfst::client ALIAS mvfst_tunnel_client )

target_sources( mvfst_tunnel_client PRIVATE
  mvfst_client.cpp
  mvfst_client.h
  mvfst_client_impl.cpp
  mvfst_client_impl.h
  )

target_include_directories( mvfst_tunnel_client PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

# -- check for required package
find_package( mvfst  REQUIRED )
find_package( glog REQUIRED )

target_link_libraries( mvfst_tunnel_client PRIVATE
  mvfst::mvfst_client
  mvfst::mvfst_fizz_client
  glog::glog
  gflags
  fmt::fmt
  zstd
  /opt/liburing/lib/liburing.a
  )
