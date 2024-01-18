
add_library( lsquic_client SHARED )
add_library( lsquic::client ALIAS lsquic_client )

find_package( lsquic REQUIRED )

target_sources( lsquic_client PRIVATE
  lsquic_client.cpp
  lsquic_client.h
  )

target_include_directories( lsquic_client PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories( lsquic_client PRIVATE
  /opt/boringssl/include
)

target_link_libraries( lsquic_client
  PUBLIC
  PRIVATE
  lsquic::lsquic
  /opt/boringssl/lib/libssl.a
  /opt/boringssl/lib/libcrypto.a
  /usr/lib/x86_64-linux-gnu/libevent.so
  fmt::fmt
  z
  )
