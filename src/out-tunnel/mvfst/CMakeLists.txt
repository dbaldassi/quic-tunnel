
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

target_link_libraries( mvfst_server
  PUBLIC
  PRIVATE
  mvfst::mvfst_server

  glog::glog
  gflags
  zstd
  )
