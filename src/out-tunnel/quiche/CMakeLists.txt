
add_library( quiche_server SHARED )
add_library( quiche::server ALIAS quiche_server )

target_sources( quiche_server PRIVATE
  quiche_server.cpp
  quiche_server.h
  )

target_include_directories( quiche_server PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

target_include_directories( quiche_server PRIVATE
  ${QUICHE_DIR}/include
  )

set_target_properties( quiche_server PROPERTIES CXX_STANDARD 20 )

string( TOLOWER ${CMAKE_BUILD_TYPE} lower_build_type )
target_link_directories( quiche_server PRIVATE ${QUICHE_DIR}/lib/${lower_build_type} )

target_link_libraries( quiche_server PRIVATE
  quiche
  /opt/ev/lib/libev.a
  fmt::fmt
  )

if( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" )
  target_compile_definitions( quiche_server PRIVATE -DQUICHE_DEBUG )
endif()
