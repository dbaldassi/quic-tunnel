

add_library( msquic_server SHARED )
add_library( msquic::server ALIAS msquic_server )

target_sources( msquic_server PRIVATE
  msquic_server.cpp
  msquic_server.h
  )

target_include_directories( msquic_server PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

# target_include_directories( msquic_server PRIVATE
#   ${QUICHE_DIR}/include
#   )

set_target_properties( msquic_server PROPERTIES CXX_STANDARD 20 )

find_package( msquic REQUIRED )

target_include_directories( msquic_server PRIVATE
  ${msquic_DIR}/../../include
  )

target_link_libraries( msquic_server
  msquic
  )
