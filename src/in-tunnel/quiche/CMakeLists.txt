
add_library( quiche_client )
add_library( quiche::client ALIAS quiche_client )

target_sources( quiche_client PRIVATE
  quiche_client.cpp
  quiche_client.h
  )

target_include_directories( quiche_client PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories( quiche_client PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/quiche/include
  )

string( TOLOWER ${CMAKE_BUILD_TYPE} lower_build_type )
target_link_directories( quiche_client PUBLIC ${CMAKE_SOURCE_DIR}/lib/quiche/lib/${lower_build_type} )
target_link_libraries( quiche_client PRIVATE quiche ev )
