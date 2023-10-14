

add_library( lsquic_server )
add_library( lsquic::server ALIAS lsquic_server )

find_package( OpenSSL REQUIRED )

target_sources( lsquic_server PRIVATE
  lsquic_server.cpp
  lsquic_server.h
  )

target_include_directories( lsquic_server PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

target_link_libraries( lsquic_server PUBLIC
  lsquic::lsquic
  /opt/boringssl/lib/libssl.a
  /opt/boringssl/lib/libcrypto.a
  /usr/lib/x86_64-linux-gnu/libevent.a
  )
