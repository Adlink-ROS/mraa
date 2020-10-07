# MRAA
SET(MRAA_INCLUDE_PATH "/opt/adlink/neuron-sdk/neuron-library/include/")
if (NOT "$ENV{NLIB_MRAA_INCLUDE_PATH}" STREQUAL "")
  SET(MRAA_INCLUDE_PATH "$ENV{NLIB_MRAA_INCLUDE_PATH}")
endif()
SET(MRAA_LIBRARY_PATH "/opt/adlink/neuron-sdk/neuron-library/lib/")
if (NOT "$ENV{NLIB_MRAA_LIBRARY_PATH}" STREQUAL "")
  SET(MRAA_LIBRARY_PATH "$ENV{NLIB_MRAA_LIBRARY_PATH}")
endif()
include_directories(${MRAA_INCLUDE_PATH})
link_directories(${MRAA_LIBRARY_PATH})

## Set the RPATH
SET(CMAKE_INSTALL_RPATH ${MRAA_LIBRARY_PATH}) 

add_subdirectory (c)
add_subdirectory (platform)

if (NOT ANDROID_TOOLCHAIN)
  add_subdirectory (c++)
endif ()

install (DIRECTORY ${PROJECT_SOURCE_DIR}/examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/mraa/examples)
