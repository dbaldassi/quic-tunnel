# -- quic go server implementation

add_library( quicgo_server )
add_library( quicgo::server ALIAS quicgo_server )

target_include_directories( quicgo_server PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

target_sources( quicgo_server PRIVATE
  quicgo_server.cpp
  quicgo_server.h
  )

add_custom_target( libgo_server ALL
  COMMAND go build -buildmode c-shared -o libgo_server.so quic_tunnel/go_server
  COMMENT "Build go server implementation package"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/go_impl
  )

add_dependencies( quicgo_server libgo_server )

target_link_directories( quicgo_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/go_impl )
target_link_libraries( quicgo_server PUBLIC go_server fmt::fmt )
