
add_library( lsquic_server SHARED )
add_library( lsquic::server ALIAS lsquic_server )

find_package( lsquic REQUIRED )

target_sources( lsquic_server PRIVATE
  lsquic_server.cpp
  lsquic_server.h
  lsquic_hash.h
  lsquic_xxhash.h
  lsquic_hash.c
  lsquic_xxhash.c
  )

target_include_directories( lsquic_server PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories( lsquic_server PRIVATE
  /opt/boringssl/include
)

target_link_libraries( lsquic_server
  PUBLIC
  PRIVATE
  lsquic::lsquic
  /opt/boringssl/lib/libssl.a
  /opt/boringssl/lib/libcrypto.a
  /usr/lib/x86_64-linux-gnu/libevent.so
  fmt::fmt
  z
  )
