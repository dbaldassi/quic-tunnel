
add_library( mvfst_client )
add_library( mvfst::client ALIAS mvfst_client )

target_sources( mvfst_client PRIVATE
  mvfst_client.cpp
  mvfst_client.h
  )

target_include_directories( mvfst_client PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

find_package( glog REQUIRED )

target_link_libraries( mvfst_client PRIVATE
  mvfst::mvfst_client
  mvfst::mvfst_fizz_client
  glog::glog
  gflags
  zstd
  )
