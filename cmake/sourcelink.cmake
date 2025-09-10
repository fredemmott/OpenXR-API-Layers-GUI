set(SOURCELINK "" CACHE STRING "URL to retrieve the source")
if (NOT SOURCELINK)
  return()
endif ()

if (NOT MSVC)
  message(FATAL_ERROR "SOURCELINK was specified, but not using MSVC")
endif ()

message(STATUS "Using SOURCELINK: ${SOURCELINK}")
cmake_path(
  NATIVE_PATH
  CMAKE_SOURCE_DIR
  NORMALIZE
  NATIVE_SOURCE
)
string(REPLACE "\\" "\\\\" NATIVE_SOURCE "${NATIVE_SOURCE}")
file(
  WRITE
  "${CMAKE_CURRENT_BINARY_DIR}/sourcelink.json"
  "{ \"documents\": { \"${NATIVE_SOURCE}\\\\*\": \"${SOURCELINK}/*\" } }"
)
add_link_options("/SOURCELINK:${CMAKE_CURRENT_BINARY_DIR}/sourcelink.json")
