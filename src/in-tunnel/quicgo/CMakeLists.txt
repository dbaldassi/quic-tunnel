# -- quic go client implementation

add_library( quicgo_client )
add_library( quicgo::client ALIAS quicgo_client )

# go build -buildmode c-shared -o libgotest.so gotest

target_include_directories( quicgo_client PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

target_sources( quicgo_client PRIVATE
  quicgo_client.cpp
  quicgo_client.h
  )

add_custom_target( libgo_client ALL
  COMMAND go build -buildmode c-shared -o libgo_client.so quic_tunnel/go_client
  COMMENT "Build go client implementation package"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/go_impl
  )

add_dependencies( quicgo_client libgo_client )

target_link_directories( quicgo_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/go_impl )
target_link_libraries( quicgo_client PUBLIC go_client fmt::fmt )
