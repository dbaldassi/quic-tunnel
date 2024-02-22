
add_library( msquic_client SHARED )
add_library( msquic::client ALIAS msquic_client )

target_sources( msquic_client PRIVATE
  msquic_client.cpp
  msquic_client.h
  )

target_include_directories( msquic_client PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

# target_include_directories( msquic_client PRIVATE
#   ${QUICHE_DIR}/include
#   )

set_target_properties( msquic_client PROPERTIES CXX_STANDARD 20 )

find_package( msquic REQUIRED )

target_include_directories( msquic_client PRIVATE
  ${msquic_DIR}/../../include
  )

target_link_libraries( msquic_client
  msquic
  )
