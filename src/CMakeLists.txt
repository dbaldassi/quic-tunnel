# -- src

target_sources( quic-tunnel PRIVATE    
  main.cpp

  capabilities.h
  qlogfile.h
  random_generator.h
  )

target_include_directories( quic-tunnel PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

target_link_libraries( quic-tunnel PRIVATE
  fmt::fmt
  )

add_subdirectory( in-tunnel )
add_subdirectory( out-tunnel )
add_subdirectory( controls )
