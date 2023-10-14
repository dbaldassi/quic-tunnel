# -- global cmakelists

cmake_minimum_required( VERSION 3.16 FATAL_ERROR )

project( quic-tunnel )

# -- Module path
set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )

# -- check for required package
find_package( mvfst  REQUIRED )
find_package( lsquic REQUIRED )
find_package( fmt    REQUIRED )

# -- External dependencies
# --- ASIO
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/external/asio/asio/include )
# --- websocketpp
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp )
# --- JSON
set( JSON_BuildTests      OFF CACHE INTERNAL "" )
set( JSON_Install         OFF CACHE INTERNAL "" )
set( JSON_MultipleHeaders ON  CACHE INTERNAL "" )
add_subdirectory( external/nlohmann_json )

add_executable( quic-tunnel )

set_target_properties( quic-tunnel PROPERTIES CXX_STANDARD 20 )

add_subdirectory( src )
